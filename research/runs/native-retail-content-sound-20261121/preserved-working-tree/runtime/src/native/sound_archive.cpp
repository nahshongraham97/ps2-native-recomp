#include "mka/sound_archive.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_set>

namespace mka::sound {
namespace {
std::uint32_t word(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at + 1]) << 8) |
           (std::uint32_t(bytes[at + 2]) << 16) | (std::uint32_t(bytes[at + 3]) << 24);
}
std::string folded(std::string_view text) {
    std::string result(text);
    for (char& ch : result)
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
    return result;
}
std::string bank_name(std::string_view request) {
    request = request.substr(0, request.find('\0'));
    const auto separator = request.find_last_of("/\\:");
    if (separator != std::string_view::npos) request.remove_prefix(separator + 1);
    const auto dot = request.find_last_of('.');
    if (dot != std::string_view::npos) request = request.substr(0, dot);
    std::string name(request);
    name += ".mst";
    return folded(name);
}
bool position(AudioResourceBacking& opened, std::uint64_t& value, ArchiveFailure& failure) {
    if (opened.content_file) {
        content::ContentVolumeFailure error;
        auto result = opened.content_file->position(error);
        if (!result) { failure.content = error; failure.host_error = error.host_error; return false; }
        value = *result; return true;
    }
    return opened.file.position(value, failure.host_error);
}
bool length(AudioResourceBacking& opened, std::uint64_t& value, ArchiveFailure& failure) {
    if (opened.content_file) {
        content::ContentVolumeFailure error;
        auto result = opened.content_file->length(error);
        if (!result) { failure.content = error; failure.host_error = error.host_error; return false; }
        value = *result; return true;
    }
    return opened.file.length(value, failure.host_error);
}
bool seek(AudioResourceBacking& opened, std::int64_t offset, ArchiveFailure& failure) {
    if (opened.content_file) {
        content::ContentVolumeFailure error;
        const bool ok = opened.content_file->seek(offset, content::SeekOrigin::begin, error);
        if (!ok) { failure.content = error; failure.host_error = error.host_error; }
        return ok;
    }
    return opened.file.seek(offset, content::SeekOrigin::begin, failure.host_error);
}
content::FileRead read(AudioResourceBacking& opened, void* bytes, std::size_t count, ArchiveFailure& failure) {
    if (opened.content_file) {
        content::ContentVolumeFailure error;
        auto result = opened.content_file->read(bytes, count, error);
        if (result.error) { failure.content = error; failure.host_error = error.host_error; }
        return result;
    }
    const auto result = opened.file.read(bytes, count);
    if (result.error) failure.host_error = result.error;
    return result;
}
}

struct SoundArchiveBacking::State {
    std::shared_ptr<AudioResourceDomain> domain;
    std::unique_ptr<AudioResourceBacking> opened = std::make_unique<AudioResourceBacking>();
    ~State() { domain->retire_backing(std::move(opened)); }
    std::uint64_t length = 0;
    mutable std::mutex mutex;
};
SoundArchiveBacking::SoundArchiveBacking(std::unique_ptr<State> state) : state_(std::move(state)) {}
SoundArchiveBacking::~SoundArchiveBacking() = default;

std::shared_ptr<SoundArchiveBacking> SoundArchiveBacking::open(
    const std::filesystem::path& path, ArchiveFailure& failure) {
    failure = {};
    auto domain = AudioResourceDomain::create(failure.resource);
    if (!domain) { failure.kind = ArchiveError::resource_access; return {}; }
    return open(path, std::move(domain), failure);
}
std::shared_ptr<AudioResourceDomain> SoundArchiveBacking::resource_domain() const noexcept {
    return state_->domain;
}
std::shared_ptr<SoundArchiveBacking> SoundArchiveBacking::open(
    const std::filesystem::path& path, std::shared_ptr<AudioResourceDomain> domain, ArchiveFailure& failure) {
    failure = {};
    if (!domain) { failure.kind = ArchiveError::resource_access; failure.resource.kind = AudioResourceError::invalid_owner; return {}; }
    auto access = domain->acquire(AudioResourceWork::backing, failure.resource);
    if (!access) { failure.kind = ArchiveError::resource_access; return {}; }
    try {
        auto state = std::make_unique<State>();
        state->domain = std::move(domain);
        if (!state->opened->file.open(path, failure.host_error)) {
            failure.kind = ArchiveError::open_failed;
            return {};
        }
        if (!state->opened->file.length(state->length, failure.host_error)) {
            failure.kind = ArchiveError::io_error;
            return {};
        }
        return std::shared_ptr<SoundArchiveBacking>(new SoundArchiveBacking(std::move(state)));
    } catch (const std::bad_alloc&) { failure.kind = ArchiveError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = ArchiveError::host_capacity; }
      catch (const std::filesystem::filesystem_error& error) {
          failure = {ArchiveError::open_failed, error.code()};
      }
    return {};
}
std::shared_ptr<SoundArchiveBacking> SoundArchiveBacking::adopt(
    content::NativeContentFile&& file, std::shared_ptr<AudioResourceDomain> domain, ArchiveFailure& failure) {
    failure = {};
    if (!file.valid()) { failure.kind = ArchiveError::invalid_content_file; failure.content.kind = content::ContentVolumeError::moved_file; return {}; }
    if (!domain) { failure.kind = ArchiveError::resource_access; failure.resource.kind = AudioResourceError::invalid_owner; return {}; }
    auto access = domain->acquire(AudioResourceWork::backing, failure.resource);
    if (!access) { failure.kind = ArchiveError::resource_access; return {}; }
    try {
        auto state = std::make_unique<State>();
        state->domain = std::move(domain);
        state->opened->content_file.emplace(std::move(file));
        if (!length(*state->opened, state->length, failure)) { failure.kind = ArchiveError::io_error; return {}; }
        return std::shared_ptr<SoundArchiveBacking>(new SoundArchiveBacking(std::move(state)));
    } catch (const std::bad_alloc&) { failure.kind = ArchiveError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = ArchiveError::host_capacity; }
    return {};
}
std::uint64_t SoundArchiveBacking::size() const noexcept { return state_->length; }
std::optional<std::uint64_t> SoundArchiveBacking::opened_position(ArchiveFailure& failure) const {
    failure = {};
    auto access = state_->domain->acquire(AudioResourceWork::backing, failure.resource);
    if (!access) { failure.kind = ArchiveError::resource_access; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    std::uint64_t cursor = 0;
    if (!position(*state_->opened, cursor, failure)) { failure.kind = ArchiveError::io_error; return std::nullopt; }
    return cursor;
}

std::optional<std::vector<std::uint8_t>> SoundArchiveBacking::read_bytes(
    std::uint64_t offset, std::uint64_t count, ArchiveFailure& failure) const {
    failure = {};
    auto access = state_->domain->acquire(AudioResourceWork::backing, failure.resource);
    if (!access) { failure.kind = ArchiveError::resource_access; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto fail = [&](ArchiveError kind)
        -> std::optional<std::vector<std::uint8_t>> { failure.kind = kind; return std::nullopt; };
    std::uint64_t saved = 0, file_length = 0;
    if (!position(*state_->opened, saved, failure)) return fail(ArchiveError::io_error);
    auto restore = [&](std::optional<std::vector<std::uint8_t>> value) {
        ArchiveFailure restore_failure;
        if (saved > std::uint64_t(INT64_MAX) ||
            !seek(*state_->opened, static_cast<std::int64_t>(saved), restore_failure)) {
            failure = restore_failure;
            return fail(ArchiveError::cursor_restore_failed);
        }
        return value;
    };
    if (!length(*state_->opened, file_length, failure)) return restore(fail(ArchiveError::io_error));
    if (offset > file_length || count > file_length - offset) return restore(fail(ArchiveError::entry_out_of_bounds));
    std::vector<std::uint8_t> bytes;
    if (offset > std::uint64_t(INT64_MAX) || count > bytes.max_size())
        return restore(fail(ArchiveError::host_capacity));
    try { bytes.resize(static_cast<std::size_t>(count)); }
    catch (const std::bad_alloc&) { return restore(fail(ArchiveError::allocation_failed)); }
    catch (const std::length_error&) { return restore(fail(ArchiveError::host_capacity)); }
    if (!seek(*state_->opened, static_cast<std::int64_t>(offset), failure))
        return restore(fail(ArchiveError::io_error));
    if (!bytes.empty()) {
        const auto result = read(*state_->opened, bytes.data(), bytes.size(), failure);
        if (result.error) return restore(fail(ArchiveError::io_error));
        if (result.bytes != bytes.size()) return restore(fail(ArchiveError::short_read));
    }
    return restore(std::optional<std::vector<std::uint8_t>>(std::move(bytes)));
}
std::optional<SamplePayload> SoundArchiveBacking::read_payload(
    const ResourceLookup& resource, PayloadFailure& failure) const {
    AudioResourceFailure resource_failure;
    auto access = state_->domain->acquire(AudioResourceWork::backing, resource_failure);
    if (!access) { failure = {}; failure.kind = PayloadError::resource_access; failure.resource = resource_failure; return std::nullopt; }
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->opened->content_file)
        return SamplePayload::read(*state_->opened->content_file, resource, failure);
    return SamplePayload::read(state_->opened->file, resource, failure);
}

std::optional<SoundArchive> SoundArchive::open(const std::filesystem::path& path,
                                              ArchiveFailure& failure) {
    failure = {};
    auto domain = AudioResourceDomain::create(failure.resource);
    if (!domain) { failure.kind = ArchiveError::resource_access; return std::nullopt; }
    return open(path, std::move(domain), failure);
}
std::optional<SoundArchive> SoundArchive::open(const std::filesystem::path& path,
    std::shared_ptr<AudioResourceDomain> domain, ArchiveFailure& failure) {
    failure = {};
    auto backing = SoundArchiveBacking::open(path, std::move(domain), failure);
    if (!backing) return std::nullopt;
    return parse(std::move(backing), failure);
}
std::optional<SoundArchive> SoundArchive::adopt(content::NativeContentFile&& file, ArchiveFailure& failure) {
    failure = {};
    if (!file.valid()) { failure.kind = ArchiveError::invalid_content_file; failure.content.kind = content::ContentVolumeError::moved_file; return std::nullopt; }
    auto domain = AudioResourceDomain::create(failure.resource);
    if (!domain) { failure.kind = ArchiveError::resource_access; return std::nullopt; }
    return adopt(std::move(file), std::move(domain), failure);
}
std::optional<SoundArchive> SoundArchive::adopt(content::NativeContentFile&& file,
    std::shared_ptr<AudioResourceDomain> domain, ArchiveFailure& failure) {
    failure = {};
    auto backing = SoundArchiveBacking::adopt(std::move(file), std::move(domain), failure);
    if (!backing) return std::nullopt;
    return parse(std::move(backing), failure);
}
std::optional<SoundArchive> SoundArchive::parse(std::shared_ptr<SoundArchiveBacking> backing,
    ArchiveFailure& failure) {
    auto fail = [&](ArchiveError kind) -> std::optional<SoundArchive> {
        failure = {kind, {}}; return std::nullopt;
    };
    try {
        SoundArchive archive;
        archive.backing_ = std::move(backing);
        if (archive.backing_->size() < 12) return fail(ArchiveError::truncated_header);
        auto header = archive.backing_->read_bytes(0, 12, failure);
        if (!header) return std::nullopt;
        const auto type = word(*header, 0), count = word(*header, 4), table_bytes = word(*header, 8);
        if (type != 1 && type != 2) return fail(ArchiveError::unsupported_type);
        if (count == 0 || count > 0x7fffffffu) return fail(ArchiveError::invalid_count);
        if (table_bytes > archive.backing_->size() - 12 || count > table_bytes / 12u)
            return fail(ArchiveError::directory_out_of_bounds);
        auto table = archive.backing_->read_bytes(12, table_bytes, failure);
        if (!table) return std::nullopt;
        // reserve enforces the host vector limit and reports length_error;
        // there is no separate original-console capacity constant.
        archive.records_.reserve(count);
        std::unordered_set<std::uint32_t> keys;
        std::size_t named = count;
        for (std::size_t index = 0; index < count; ++index) {
            const auto at = index * 12;
            ArchiveRecord record{word(*table, at), word(*table, at + 4), word(*table, at + 8), {}};
            if (record.offset > archive.backing_->size() ||
                record.size > archive.backing_->size() - record.offset)
                return fail(ArchiveError::entry_out_of_bounds);
            if (!keys.insert(record.key).second) return fail(ArchiveError::duplicate_key);
            if (type == 2 && index != 0 && named == count && (record.key & 0x20000000u)) named = index;
            archive.records_.push_back(std::move(record));
        }
        std::size_t cursor = static_cast<std::size_t>(count) * 12;
        std::unordered_set<std::string> names;
        archive.names_.reserve(named);
        for (std::size_t index = 0; index < named; ++index) {
            if (archive.records_[index].key & 0x20000000u) {
                archive.names_.push_back(std::nullopt);
                continue;
            }
            const auto end = std::find(table->begin() + cursor, table->end(), 0);
            if (end == table->end()) return fail(ArchiveError::unterminated_name);
            std::string name(reinterpret_cast<const char*>(table->data() + cursor),
                             static_cast<std::size_t>(end - table->begin()) - cursor);
            cursor = static_cast<std::size_t>(end - table->begin()) + 1;
            const auto normalized = folded(name);
            if (index != 0 && !names.insert(normalized).second) return fail(ArchiveError::ambiguous_name);
            archive.records_[index].name = name;
            archive.names_.push_back(std::optional<std::string>(normalized));
        }
        return archive;
    } catch (const std::bad_alloc&) { return fail(ArchiveError::allocation_failed); }
      catch (const std::length_error&) { return fail(ArchiveError::host_capacity); }
}
void SoundArchive::swap(SoundArchive& other) noexcept {
    backing_.swap(other.backing_);
    records_.swap(other.records_);
    names_.swap(other.names_);
}
std::size_t SoundArchive::named_entry_count() const noexcept {
    if (!backing_ || names_.empty()) return 0;
    return static_cast<std::size_t>(std::count_if(names_.begin() + 1, names_.end(),
                                                [](const auto& name) { return name.has_value(); }));
}
std::optional<ArchiveRecord> SoundArchive::find_bank(std::string_view request,
                                                   ArchiveFailure& failure) const {
    failure = {};
    if (!backing_) { failure.kind = ArchiveError::closed; return std::nullopt; }
    try {
        const auto query = bank_name(request);
        // Original cached search starts/wraps at1. Unique names make the
        // optimization irrelevant; sentinel0 is never a normal bank result.
        for (std::size_t index = 1; index < names_.size(); ++index)
            if (names_[index] && *names_[index] == query) return records_[index];
        failure.kind = ArchiveError::absent_bank;
    } catch (const std::bad_alloc&) { failure.kind = ArchiveError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = ArchiveError::host_capacity; }
    return std::nullopt;
}
std::optional<BankAssetSource> SoundArchive::read_bank(std::string_view request,
                                                    ArchiveFailure& failure) const {
    auto origin = find_bank(request, failure);
    if (!origin) return std::nullopt;
    auto bytes = backing_->read_bytes(origin->offset, origin->size, failure);
    if (!bytes) return std::nullopt;
    return BankAssetSource{std::move(*origin), std::move(*bytes), backing_};
}
} // namespace mka::sound
