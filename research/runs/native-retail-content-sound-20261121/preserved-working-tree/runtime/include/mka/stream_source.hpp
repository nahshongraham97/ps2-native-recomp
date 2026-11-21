#pragma once
#include "mka/sound_archive.hpp"
#include <memory>

namespace mka::sound {
enum class StreamError {
    none, closed, resource_not_found, unsupported_key_mode, not_alternate_resource,
    unsupported_empty_extent, unsupported_partial_extent, invalid_stride,
    extent_out_of_bounds, group_out_of_bounds, arithmetic_overflow,
    backing_failure, allocation_failed, host_capacity
};
struct StreamFailure {
    StreamError kind = StreamError::none;
    ArchiveFailure backing;
};
struct StreamGroupMetadata {
    ResourceLookup resource;
    std::uint32_t key = 0, rate_value = 0, stride = 0, tail_control = 0;
    std::uint64_t offset = 0, length = 0, ordinal = 0, group_count = 0;
};
// Immutable owned source bytes; range0/range1 are ordered encoded spans only.
// No channel, codec, loop, decoded duration, preparation or readiness meaning.
// Copies share immutable ownership; moves leave an empty queryable object.
class StreamSourceGroup {
public:
    static constexpr std::uint64_t serialized_group_bytes = 0x8000;
    StreamSourceGroup(const StreamSourceGroup&) = default;
    StreamSourceGroup(StreamSourceGroup&&) noexcept = default;
    StreamSourceGroup& operator=(const StreamSourceGroup&) = default;
    StreamSourceGroup& operator=(StreamSourceGroup&&) noexcept = default;
    StreamGroupMetadata metadata() const noexcept;
    PayloadBytes raw_group() const noexcept;
    PayloadBytes range0() const noexcept;
    PayloadBytes range1() const noexcept;
    const std::filesystem::path& associated_path() const noexcept;
private:
    struct Data;
    explicit StreamSourceGroup(std::shared_ptr<const Data> data) : data_(std::move(data)) {}
    std::shared_ptr<const Data> data_;
    friend class StreamSource;
};
// Explicitly associates a supplied MS4 path with this source context. Never
// searches by suffix, reopens by path, parses MS2, or derives association from
// resource keys. Caller must establish that this is the bank's associated MS4.
class StreamSource {
public:
    StreamSource(const StreamSource&) = default;
    StreamSource(StreamSource&&) noexcept = default;
    StreamSource& operator=(const StreamSource&) = default;
    StreamSource& operator=(StreamSource&&) noexcept = default;
    static std::optional<StreamSource> open_associated_ms4(
        const std::filesystem::path&, StreamFailure&);
    static std::optional<StreamSource> open_associated_ms4(
        const std::filesystem::path&, std::shared_ptr<AudioResourceDomain>, StreamFailure&);
    static std::optional<StreamSource> adopt_associated_ms4(
        content::NativeContentFile&&, std::shared_ptr<AudioResourceDomain>, StreamFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
    std::optional<StreamSourceGroup> read_group(
        const ResourceLookup&, std::uint64_t ordinal, StreamFailure&) const;
    std::uint64_t backing_size() const noexcept;
    const std::filesystem::path& associated_path() const noexcept;
private:
    struct State;
    explicit StreamSource(std::shared_ptr<const State> state) : state_(std::move(state)) {}
    std::shared_ptr<const State> state_;
};
} // namespace mka::sound
