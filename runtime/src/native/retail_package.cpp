#include "mka/retail_package.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::string_view source_request = "/game/mkda.pak";
constexpr std::uint32_t pak_magic = 0x50414b20u; // actual LE bytes 20 4B 41 50
constexpr std::uint32_t pak_version = 0x100u;
constexpr std::size_t header_size = 20;
constexpr std::size_t record_size = 12;

std::uint32_t word(const std::uint8_t* p) noexcept {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 |
        std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
RetailPackageError content_error(content::ContentVolumeError kind) noexcept {
    using C = content::ContentVolumeError;
    if (kind == C::allocation_failed) return RetailPackageError::allocation;
    if (kind == C::host_capacity) return RetailPackageError::host_capacity;
    if (kind == C::length_failed) return RetailPackageError::source_length;
    if (kind == C::seek_failed) return RetailPackageError::source_seek;
    if (kind == C::read_failed) return RetailPackageError::source_read;
    return RetailPackageError::source_open;
}
}

std::optional<std::string> NativeRetailPackage::key(std::string_view raw, RetailPackageFailure& f) {
    if (raw.empty() || raw.find('\0') != raw.npos) {
        f.kind = RetailPackageError::invalid_name;
        f.operation = "retail package member name";
        return {};
    }
    std::string result;
    result.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (ch > 0x7f || ch < 0x20 || ch == 0x7f) {
            f.kind = RetailPackageError::invalid_name;
            f.operation = "retail package member ASCII name";
            return {};
        }
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<unsigned char>(ch + ('a' - 'A'));
        result.push_back(static_cast<char>(ch));
    }
    return result;
}

bool NativeRetailPackage::exact_read(std::uint64_t offset, void* dst, std::size_t count,
    RetailPackageFailure& f) {
    if (offset > file_size_ || count > file_size_ - offset ||
        offset > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        f.kind = RetailPackageError::invalid_read_range;
        f.operation = "retail package bounded read";
        return false;
    }
    if (!file_.seek(static_cast<std::int64_t>(offset), content::SeekOrigin::begin, f.content)) {
        f.kind = content_error(f.content.kind);
        f.operation = "retail package seek";
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(dst);
    std::size_t done = 0;
    while (done < count) {
        const auto r = file_.read(bytes + done, count - done, f.content);
        if (r.error || r.bytes == 0) {
            f.kind = RetailPackageError::source_read;
            f.operation = "retail package short read";
            return false;
        }
        done += r.bytes;
    }
    return true;
}

bool NativeRetailPackage::build_index(RetailPackageFailure& f) {
    const auto size = file_.length(f.content);
    if (!size) {
        f.kind = content_error(f.content.kind);
        f.operation = "retail package length";
        return false;
    }
    file_size_ = *size;
    if (file_size_ < header_size) {
        f.kind = RetailPackageError::invalid_header;
        f.operation = "retail package short header";
        return false;
    }
    std::array<std::uint8_t, header_size> header{};
    if (!exact_read(0, header.data(), header.size(), f)) return false;
    const auto magic = word(header.data());
    const auto version = word(header.data() + 4);
    const auto count = word(header.data() + 8);
    const auto table_offset = word(header.data() + 12);
    const auto names_size = word(header.data() + 16);
    if (magic != pak_magic || version != pak_version) {
        f.kind = RetailPackageError::invalid_header;
        f.operation = "retail package format";
        return false;
    }
    const auto table_bytes = std::uint64_t(count) * record_size;
    if (table_offset > file_size_ || table_bytes > file_size_ - table_offset ||
        names_size > file_size_ - table_offset - table_bytes ||
        table_bytes > std::numeric_limits<std::size_t>::max() ||
        names_size > std::numeric_limits<std::size_t>::max()) {
        f.kind = RetailPackageError::invalid_index;
        f.operation = "retail package table bounds";
        return false;
    }
    std::vector<std::uint8_t> table(static_cast<std::size_t>(table_bytes));
    std::vector<std::uint8_t> names(names_size);
    if (!exact_read(table_offset, table.data(), table.size(), f) ||
        !exact_read(table_offset + table_bytes, names.data(), names.size(), f)) return false;
    members_.reserve(count);
    by_name_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* row = table.data() + std::size_t(i) * record_size;
        const auto name_offset = word(row);
        const auto offset = word(row + 4);
        const auto length = word(row + 8);
        if (offset > file_size_ || length > file_size_ - offset) {
            f.kind = RetailPackageError::invalid_entry_range;
            f.operation = "retail package member bounds";
            return false;
        }
        if (name_offset >= names.size()) {
            f.kind = RetailPackageError::invalid_name;
            f.operation = "retail package member name offset";
            return false;
        }
        const auto begin = names.begin() + name_offset;
        const auto end = std::find(begin, names.end(), std::uint8_t{0});
        if (end == names.end()) {
            f.kind = RetailPackageError::invalid_name;
            f.operation = "retail package unterminated member name";
            return false;
        }
        std::string name(begin, end);
        auto normalized = key(name, f);
        if (!normalized) return false;
        members_.push_back({std::move(name), offset, length, i});
        // 2404A0 replaces a matching value in its hash chain. Last index wins.
        by_name_[std::move(*normalized)] = members_.size() - 1;
    }
    return true;
}

std::unique_ptr<NativeRetailPackage> NativeRetailPackage::open(
    const content::NativeContentVolume& volume, const NativeRetailFilePaths& paths,
    RetailPackageFailure& f) noexcept {
    f = {};
    try {
        auto file = paths.open(volume, source_request, f.path);
        if (!file) {
            f.content = f.path.content;
            switch (f.path.kind) {
            case RetailFilePathError::content_file: f.kind = content_error(f.content.kind); break;
            case RetailFilePathError::allocation: f.kind = RetailPackageError::allocation; break;
            case RetailFilePathError::host_capacity: f.kind = RetailPackageError::host_capacity; break;
            case RetailFilePathError::native_exception: f.kind = RetailPackageError::native_exception; break;
            default: f.kind = RetailPackageError::source_path; break;
            }
            f.operation = source_request.data();
            return {};
        }
        auto owner = std::unique_ptr<NativeRetailPackage>(new NativeRetailPackage(std::move(*file)));
        if (!owner->build_index(f)) return {};
        return owner;
    } catch (const std::bad_alloc&) {
        f.kind = RetailPackageError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailPackageError::host_capacity;
    } catch (...) {
        f.kind = RetailPackageError::native_exception;
    }
    f.operation = "retail package index construction";
    return {};
}

std::optional<RetailPackageMember> NativeRetailPackage::find(
    std::string_view name, RetailPackageFailure& f) const noexcept {
    f = {};
    try {
        auto normalized = key(name, f);
        if (!normalized) return {};
        const auto it = by_name_.find(*normalized);
        if (it == by_name_.end()) {
            f.kind = RetailPackageError::missing_member;
            f.operation = "retail package member lookup";
            return {};
        }
        return members_[it->second];
    } catch (const std::bad_alloc&) {
        f.kind = RetailPackageError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailPackageError::host_capacity;
    } catch (...) {
        f.kind = RetailPackageError::native_exception;
    }
    f.operation = "retail package member lookup";
    return {};
}

std::optional<std::size_t> NativeRetailPackage::read(std::string_view name,
    std::uint64_t relative_offset, std::span<std::uint8_t> out,
    RetailPackageFailure& f) noexcept {
    if (owner_thread_ != std::this_thread::get_id()) {
        f = {};
        f.kind = RetailPackageError::wrong_thread;
        f.operation = "retail package cursor owner thread";
        return {};
    }
    try {
        const auto member = find(name, f);
        if (!member) return {};
        if (relative_offset > member->size) {
            f.kind = RetailPackageError::invalid_read_range;
            f.operation = "retail package member-relative read";
            return {};
        }
        const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(
            member->size - relative_offset, out.size()));
        if (n && !exact_read(member->offset + relative_offset, out.data(), n, f)) return {};
        return n;
    } catch (const std::bad_alloc&) {
        f.kind = RetailPackageError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailPackageError::host_capacity;
    } catch (...) {
        f.kind = RetailPackageError::native_exception;
    }
    f.operation = "retail package member read";
    return {};
}

namespace main_binding {
bool initialize_retail_package_stage(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.package) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "retail package owner exists";
        return false;
    }
    if (!main.file_paths) {
        failure.kind = StartupError::native_exception;
        failure.operation = "retail package native file-path service missing";
        return false;
    }
    RetailPackageFailure source;
    auto owner = NativeRetailPackage::open(main.startup.content, *main.file_paths, source);
    if (!owner) {
        failure.content = source.content;
        failure.operation = source.operation;
        switch (source.kind) {
        case RetailPackageError::allocation: failure.kind = StartupError::allocation; break;
        case RetailPackageError::host_capacity: failure.kind = StartupError::host_capacity; break;
        case RetailPackageError::source_path:
        case RetailPackageError::source_open:
        case RetailPackageError::source_read:
        case RetailPackageError::source_seek:
        case RetailPackageError::source_length: failure.kind = StartupError::content; break;
        default: failure.kind = StartupError::native_exception; break;
        }
        return false;
    }
    main.package = std::move(owner);
    return true;
}
} // namespace main_binding

} // namespace mka::retail
