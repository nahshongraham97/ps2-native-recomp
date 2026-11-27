#include "mka/retail_archive.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::uint64_t header_bytes = 20, record_bytes = 12;
std::uint32_t u32(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
        (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
bool fail(RetailArchiveFailure& f, RetailArchiveError kind, const char* operation) noexcept {
    f.kind = kind; f.operation = operation; return false;
}
void exception(RetailArchiveFailure& f, RetailArchiveError kind) noexcept {
    fail(f, kind, "retail archive native allocation or exception");
}
bool range(std::uint64_t offset, std::uint64_t bytes, std::uint64_t limit) noexcept {
    return offset <= limit && bytes <= limit - offset;
}
bool overlap(std::uint64_t a, std::uint64_t n, std::uint64_t b, std::uint64_t m) noexcept {
    // All inputs have already passed range(), so these additions cannot wrap.
    return n && m && a < b + m && b < a + n;
}
}

NativeRetailArchive::NativeRetailArchive(content::NativeContentFile&& source)
    : source_(std::move(source)) {}

bool NativeRetailArchive::owner_thread(RetailArchiveFailure& f) const noexcept {
    return thread_ == std::this_thread::get_id() ||
        fail(f, RetailArchiveError::wrong_thread, "retail archive exclusive cursor owner thread");
}

std::optional<std::string> NativeRetailArchive::key(std::string_view text, RetailArchiveFailure& f) {
    if (text.empty()) { fail(f, RetailArchiveError::invalid_name, "retail archive empty name"); return {}; }
    std::string result;
    result.reserve(text.size());
    for (unsigned char c : text) {
        if (c < 0x20 || c > 0x7e || c == ':') {
            fail(f, RetailArchiveError::invalid_name, "retail archive ASCII logical name"); return {};
        }
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + ('a' - 'A'));
        result.push_back(static_cast<char>(c));
    }
    // A single leading slash is part of the logical identifier, not a host
    // absolute pathname. Empty/interior and traversal components are refused.
    std::size_t begin = result.front() == '/' ? 1 : 0;
    while (begin <= result.size()) {
        auto end = result.find('/', begin);
        if (end == result.npos) end = result.size();
        const std::string_view component(result.data() + begin, end - begin);
        if (component.empty() || component == "." || component == "..") {
            fail(f, RetailArchiveError::invalid_name, "retail archive unsafe logical name component"); return {};
        }
        if (end == result.size()) break;
        begin = end + 1;
    }
    return result;
}

bool NativeRetailArchive::exact_read(std::uint64_t offset, std::span<std::uint8_t> out,
    RetailArchiveFailure& f) {
    if (!range(offset, out.size(), source_size_) ||
        offset > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return fail(f, RetailArchiveError::invalid_read_range, "retail archive file range");
    if (out.empty()) return true;
    if (!source_.seek(static_cast<std::int64_t>(offset), content::SeekOrigin::begin, f.content))
        return fail(f, RetailArchiveError::source_seek, "retail archive retained file seek");
    std::size_t done = 0;
    while (done != out.size()) {
        const auto r = source_.read(out.data() + done, out.size() - done, f.content);
        if (r.error || !r.bytes || r.bytes > out.size() - done)
            return fail(f, RetailArchiveError::source_read, "retail archive retained file short read");
        done += r.bytes;
    }
    return true;
}

bool NativeRetailArchive::validate(RetailArchiveFailure& f) {
    if (!source_.valid()) return fail(f, RetailArchiveError::invalid_source, "retail archive moved or closed source");
    const auto length = source_.length(f.content);
    if (!length) return fail(f, RetailArchiveError::source_length, "retail archive source length");
    source_size_ = *length;
    if (source_size_ < header_bytes) return fail(f, RetailArchiveError::invalid_header, "retail archive truncated header");
    std::array<std::uint8_t, header_bytes> header{};
    if (!exact_read(0, header, f)) return false;
    // Actual pinned MKDA.PAK bytes204B415000010000. No endian/version guessing.
    if (u32(header.data()) != 0x50414b20 || u32(header.data() + 4) != 0x100)
        return fail(f, RetailArchiveError::unsupported_format, "retail archive unsupported PAK magic or version");
    const std::uint64_t count = u32(header.data() + 8);
    const std::uint64_t table = u32(header.data() + 12);
    const std::uint64_t name_bytes = u32(header.data() + 16);
    const std::uint64_t table_bytes = count * record_bytes; // u32 *12 fits u64.
    if (table < header_bytes || !range(table, table_bytes, source_size_) ||
        !range(table + table_bytes, name_bytes, source_size_))
        return fail(f, RetailArchiveError::invalid_table, "retail archive TOC and name table range");
    const std::uint64_t names_at = table + table_bytes;
    std::vector<std::uint8_t> records, names;
    if (table_bytes > records.max_size() || name_bytes > names.max_size() ||
        count > members_.max_size() || count > by_name_.max_size())
        return fail(f, RetailArchiveError::host_capacity, "retail archive native table capacity");
    records.resize(static_cast<std::size_t>(table_bytes));
    names.resize(static_cast<std::size_t>(name_bytes));
    if (!exact_read(table, records, f) || !exact_read(names_at, names, f)) return false;
    members_.reserve(static_cast<std::size_t>(count));
    by_name_.reserve(static_cast<std::size_t>(count));
    std::vector<std::pair<std::size_t,std::size_t>> name_ranges;
    name_ranges.reserve(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const auto* r = records.data() + i * record_bytes;
        const std::uint64_t name = u32(r), offset = u32(r + 4), bytes = u32(r + 8);
        if (name >= names.size())
            return fail(f, RetailArchiveError::invalid_name, "retail archive name offset outside name table");
        const auto begin = names.begin() + static_cast<std::size_t>(name);
        const auto end = std::find(begin, names.end(), std::uint8_t{0});
        if (end == names.end()) return fail(f, RetailArchiveError::invalid_name, "retail archive unterminated name");
        // Original names are individually addressed and may have nonzero
        // alignment padding before them. Only referenced NUL extents matter.
        name_ranges.emplace_back(static_cast<std::size_t>(name),static_cast<std::size_t>(end-names.begin())+1);
        const std::string original(begin, end);
        auto normalized = key(original, f);
        if (!normalized) return false;
        if (!range(offset, bytes, source_size_) || offset < header_bytes || (!bytes && offset != source_size_))
            return fail(f, RetailArchiveError::invalid_member_range, "retail archive payload range or empty EOF member");
        if (overlap(offset, bytes, 0, header_bytes) || overlap(offset, bytes, table, table_bytes) ||
            overlap(offset, bytes, names_at, name_bytes))
            return fail(f, RetailArchiveError::overlapping_ranges, "retail archive payload overlaps metadata");
        if (!by_name_.emplace(std::move(*normalized), i).second)
            return fail(f, RetailArchiveError::duplicate_name, "retail archive ambiguous logical name");
        members_.push_back({i, original, offset, bytes});
    }
    std::sort(name_ranges.begin(),name_ranges.end());
    for(std::size_t i=1;i<name_ranges.size();++i)
        if(name_ranges[i].first<name_ranges[i-1].second)
            return fail(f,RetailArchiveError::invalid_name,"retail archive overlapping referenced names");
    std::vector<std::size_t> order;
    order.reserve(members_.size());
    for (std::size_t i = 0; i < members_.size(); ++i) if (members_[i].size) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return members_[a].offset < members_[b].offset; });
    for (std::size_t i = 1; i < order.size(); ++i) {
        const auto& a = members_[order[i - 1]];
        const auto& b = members_[order[i]];
        if (overlap(a.offset, a.size, b.offset, b.size))
            return fail(f, RetailArchiveError::overlapping_ranges, "retail archive overlapping member payloads");
    }
    return true;
}

std::unique_ptr<NativeRetailArchive> NativeRetailArchive::open(
    content::NativeContentFile&& source, RetailArchiveFailure& f) noexcept {
    f = {};
    try {
        auto archive = std::unique_ptr<NativeRetailArchive>(new NativeRetailArchive(std::move(source)));
        if (!archive->validate(f)) return {};
        return archive;
    } catch (const std::bad_alloc&) { exception(f, RetailArchiveError::allocation); }
      catch (const std::length_error&) { exception(f, RetailArchiveError::host_capacity); }
      catch (...) { exception(f, RetailArchiveError::native_exception); }
    return {};
}

std::optional<RetailArchiveMember> NativeRetailArchive::at(std::size_t index, RetailArchiveFailure& f) const noexcept {
    f = {};
    if (!owner_thread(f)) return {};
    if (index >= members_.size()) { fail(f, RetailArchiveError::invalid_member_index, "retail archive member ordinal"); return {}; }
    try { return members_[index]; }
    catch (const std::bad_alloc&) { exception(f, RetailArchiveError::allocation); }
    catch (const std::length_error&) { exception(f, RetailArchiveError::host_capacity); }
    catch (...) { exception(f, RetailArchiveError::native_exception); }
    return {};
}

std::optional<RetailArchiveMember> NativeRetailArchive::find(std::string_view name, RetailArchiveFailure& f) const noexcept {
    f = {};
    if (!owner_thread(f)) return {};
    try {
        auto normalized = key(name, f);
        if (!normalized) return {};
        const auto it = by_name_.find(*normalized);
        if (it == by_name_.end()) { fail(f, RetailArchiveError::missing_member, "retail archive missing full logical name"); return {}; }
        return members_[it->second];
    } catch (const std::bad_alloc&) { exception(f, RetailArchiveError::allocation); }
      catch (const std::length_error&) { exception(f, RetailArchiveError::host_capacity); }
      catch (...) { exception(f, RetailArchiveError::native_exception); }
    return {};
}

std::optional<std::size_t> NativeRetailArchive::read(std::size_t index, std::uint64_t offset,
    std::span<std::uint8_t> out, RetailArchiveFailure& f) noexcept {
    f = {};
    if (!owner_thread(f)) return {};
    if (index >= members_.size()) { fail(f, RetailArchiveError::invalid_member_index, "retail archive member ordinal"); return {}; }
    const auto& m = members_[index];
    if (offset > m.size) { fail(f, RetailArchiveError::invalid_read_range, "retail archive member-relative range"); return {}; }
    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(m.size - offset, out.size()));
    try { if (exact_read(m.offset + offset, out.first(count), f)) return count; }
    catch (const std::bad_alloc&) { exception(f, RetailArchiveError::allocation); }
    catch (const std::length_error&) { exception(f, RetailArchiveError::host_capacity); }
    catch (...) { exception(f, RetailArchiveError::native_exception); }
    return {};
}

std::optional<std::size_t> NativeRetailArchive::read(std::string_view name, std::uint64_t offset,
    std::span<std::uint8_t> out, RetailArchiveFailure& f) noexcept {
    const auto m = find(name, f);
    return m ? read(m->index, offset, out, f) : std::optional<std::size_t>{};
}

std::optional<std::vector<std::uint8_t>> NativeRetailArchive::extract(std::size_t index, RetailArchiveFailure& f) noexcept {
    f = {};
    if (!owner_thread(f)) return {};
    if (index >= members_.size()) { fail(f, RetailArchiveError::invalid_member_index, "retail archive extraction ordinal"); return {}; }
    try {
        std::vector<std::uint8_t> bytes;
        if (members_[index].size > bytes.max_size()) { fail(f, RetailArchiveError::host_capacity, "retail archive native extraction capacity"); return {}; }
        bytes.resize(static_cast<std::size_t>(members_[index].size));
        if (!read(index, 0, bytes, f)) return {};
        return bytes;
    } catch (const std::bad_alloc&) { exception(f, RetailArchiveError::allocation); }
      catch (const std::length_error&) { exception(f, RetailArchiveError::host_capacity); }
      catch (...) { exception(f, RetailArchiveError::native_exception); }
    return {};
}

std::optional<std::vector<std::uint8_t>> NativeRetailArchive::extract(std::string_view name, RetailArchiveFailure& f) noexcept {
    const auto m = find(name, f);
    return m ? extract(m->index, f) : std::optional<std::vector<std::uint8_t>>{};
}
} // namespace mka::retail
