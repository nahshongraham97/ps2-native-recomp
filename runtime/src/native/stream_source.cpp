#include "mka/stream_source.hpp"
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::sound {
namespace {
std::uint32_t word(const ResourceLookup& r, std::size_t at) noexcept {
    return std::uint32_t(r.bytes[at]) | (std::uint32_t(r.bytes[at+1]) << 8) |
        (std::uint32_t(r.bytes[at+2]) << 16) | (std::uint32_t(r.bytes[at+3]) << 24);
}
const std::filesystem::path& empty_path() noexcept {
    static const std::filesystem::path path;
    return path;
}
bool add(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (b > std::numeric_limits<std::uint64_t>::max() - a) return false;
    out = a + b; return true;
}
bool multiply(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (a && b > std::numeric_limits<std::uint64_t>::max() / a) return false;
    out = a * b; return true;
}
}
struct StreamSource::State {
    std::filesystem::path path;
    std::shared_ptr<SoundArchiveBacking> backing;
};
struct StreamSourceGroup::Data {
    StreamGroupMetadata metadata;
    std::filesystem::path path;
    std::shared_ptr<SoundArchiveBacking> backing;
    std::vector<std::uint8_t> bytes;
};
StreamGroupMetadata StreamSourceGroup::metadata() const noexcept {
    return data_ ? data_->metadata : StreamGroupMetadata{};
}
PayloadBytes StreamSourceGroup::raw_group() const noexcept {
    return data_ ? PayloadBytes{data_->bytes.data(), data_->bytes.size()} : PayloadBytes{};
}
PayloadBytes StreamSourceGroup::range0() const noexcept {
    return data_ ? PayloadBytes{data_->bytes.data(), data_->metadata.stride} : PayloadBytes{};
}
PayloadBytes StreamSourceGroup::range1() const noexcept {
    return data_ ? PayloadBytes{data_->bytes.data()+data_->metadata.stride, data_->metadata.stride} : PayloadBytes{};
}
const std::filesystem::path& StreamSourceGroup::associated_path() const noexcept {
    return data_ ? data_->path : empty_path();
}
std::uint64_t StreamSource::backing_size() const noexcept { return state_ ? state_->backing->size() : 0; }
const std::filesystem::path& StreamSource::associated_path() const noexcept {
    return state_ ? state_->path : empty_path();
}
std::optional<StreamSource> StreamSource::open_associated_ms4(
    const std::filesystem::path& path, StreamFailure& failure) {
    failure = {};
    auto domain = AudioResourceDomain::create(failure.backing.resource);
    if (!domain) { failure.kind = StreamError::backing_failure; failure.backing.kind = ArchiveError::resource_access; return std::nullopt; }
    return open_associated_ms4(path, std::move(domain), failure);
}
std::shared_ptr<AudioResourceDomain> StreamSource::resource_domain() const noexcept {
    return state_ ? state_->backing->resource_domain() : nullptr;
}
std::optional<StreamSource> StreamSource::open_associated_ms4(
    const std::filesystem::path& path, std::shared_ptr<AudioResourceDomain> domain, StreamFailure& failure) {
    failure = {};
    try {
        auto state = std::make_shared<State>();
        state->path = path;
        state->backing = SoundArchiveBacking::open(path, std::move(domain), failure.backing);
        if (!state->backing) { failure.kind = StreamError::backing_failure; return std::nullopt; }
        return StreamSource(std::move(state));
    } catch (const std::bad_alloc&) { failure.kind = StreamError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = StreamError::host_capacity; }
    return std::nullopt;
}
std::optional<StreamSourceGroup> StreamSource::read_group(
    const ResourceLookup& resource, std::uint64_t ordinal, StreamFailure& failure) const {
    failure = {};
    auto fail = [&](StreamError kind) -> std::optional<StreamSourceGroup> {
        failure.kind = kind; return std::nullopt;
    };
    if (!state_) return fail(StreamError::closed);
    if (resource.status != ResourceLookupStatus::found) return fail(StreamError::resource_not_found);
    if (resource.key_mode != ResourceKeyMode::numeric) return fail(StreamError::unsupported_key_mode);
    if (!(word(resource, 0x20) & 1u)) return fail(StreamError::not_alternate_resource);
    StreamGroupMetadata m;
    m.resource = resource; m.key = word(resource, 8); m.rate_value = word(resource, 0xC);
    m.offset = word(resource, 0x10); m.length = word(resource, 0x14);
    m.stride = word(resource, 0x18); m.tail_control = word(resource, 0x1C); m.ordinal = ordinal;
    const auto size = state_->backing->size();
    if (m.offset > size || m.length > size - m.offset) return fail(StreamError::extent_out_of_bounds);
    if (!m.length) return fail(StreamError::unsupported_empty_extent);
    constexpr auto group = StreamSourceGroup::serialized_group_bytes;
    if (m.length % group) return fail(StreamError::unsupported_partial_extent);
    m.group_count = m.length / group;
    if (ordinal >= m.group_count) return fail(StreamError::group_out_of_bounds);
    std::uint64_t two_stride = 0, relative = 0, position = 0, end = 0;
    if (!multiply(m.stride, 2, two_stride)) return fail(StreamError::arithmetic_overflow);
    if (!m.stride || two_stride > group) return fail(StreamError::invalid_stride);
    if (!multiply(ordinal, group, relative) || !add(m.offset, relative, position) ||
        !add(position, group, end)) return fail(StreamError::arithmetic_overflow);
    if (relative > m.length || group > m.length - relative || end > size)
        return fail(StreamError::extent_out_of_bounds);
    try {
        auto bytes = state_->backing->read_bytes(position, group, failure.backing);
        if (!bytes) return fail(StreamError::backing_failure);
        // Defensive publication check; the backing contract never returns a short success.
        if (bytes->size() != group) {
            failure.backing.kind = ArchiveError::short_read;
            return fail(StreamError::backing_failure);
        }
        auto data = std::make_shared<StreamSourceGroup::Data>();
        data->metadata = m; data->path = state_->path; data->backing = state_->backing;
        data->bytes = std::move(*bytes);
        return StreamSourceGroup(std::move(data));
    } catch (const std::bad_alloc&) { return fail(StreamError::allocation_failed); }
      catch (const std::length_error&) { return fail(StreamError::host_capacity); }
}
} // namespace mka::sound
