#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace mka::memory {

enum class NativeHeapError {
    none, invalid_owner, inactive_owner, invalid_alignment, size_overflow,
    allocation_failed, owner_busy, foreign_owner, already_released,
    identity_exhausted, counter_overflow, native_exception,
    unbound_callback, unbound_allocator_kind, unbound_source_flags, unbound_bulk_reset
};
struct NativeHeapFailure { NativeHeapError kind{NativeHeapError::none}; };
struct NativeHeapIdentity {
    std::uint64_t value{};
    std::uint64_t generation{};
    bool operator==(const NativeHeapIdentity&) const = default;
};
struct NativeHeapObservation {
    NativeHeapIdentity identity;
    std::optional<NativeHeapIdentity> parent;
    bool active{};
    std::size_t live_allocations{};
    std::size_t inflight_allocations{};
    std::size_t child_owners{};
    std::size_t requested_live_allocations{};
    std::size_t actual_live_allocations{};
    std::size_t active_requests{};
    bool overflow_attempted{};
};
namespace detail { struct NativeHeapState; }
class NativeHeapOwner;
class NativeWaveAllocator;

// Route-wide native admission only; no source callback or fallback policy is implied.
class NativeHeapRequest final {
public:
    NativeHeapRequest() noexcept = default;
    ~NativeHeapRequest();
    NativeHeapRequest(NativeHeapRequest&&) noexcept;
    NativeHeapRequest& operator=(NativeHeapRequest&&) noexcept;
    NativeHeapRequest(const NativeHeapRequest&) = delete;
    NativeHeapRequest& operator=(const NativeHeapRequest&) = delete;
    explicit operator bool() const noexcept { return static_cast<bool>(state_); }
    bool release() noexcept;
private:
    friend class NativeHeapOwner;
    std::shared_ptr<detail::NativeHeapState> state_;
};

// No raw-pointer free API: the move-only owner carries the actual host deleter.
class NativeAllocation final {
public:
    NativeAllocation() noexcept = default;
    ~NativeAllocation();
    NativeAllocation(NativeAllocation&&) noexcept;
    NativeAllocation& operator=(NativeAllocation&&) noexcept;
    NativeAllocation(const NativeAllocation&) = delete;
    NativeAllocation& operator=(const NativeAllocation&) = delete;
    void* data() const noexcept { return pointer_; }
    explicit operator bool() const noexcept { return pointer_ != nullptr; }
    std::size_t requested_size() const noexcept { return requested_size_; }
    std::size_t rounded_size() const noexcept { return rounded_size_; }
    std::size_t alignment() const noexcept { return alignment_; }
    NativeHeapIdentity requested_owner() const noexcept { return requested_identity_; }
    NativeHeapIdentity actual_owner() const noexcept { return actual_identity_; }
    bool release(NativeHeapFailure&) noexcept;
    bool release_from(const NativeHeapOwner&, NativeHeapFailure&) noexcept;
private:
    friend class NativeHeapOwner;
    void reset() noexcept;
    void* pointer_{};
    std::size_t requested_size_{}, rounded_size_{}, alignment_{};
    NativeHeapIdentity requested_identity_{}, actual_identity_{};
    std::shared_ptr<detail::NativeHeapState> requested_, actual_;
};

class NativeHeapOwner final {
public:
    static std::shared_ptr<NativeHeapOwner> create(
        std::string name, std::shared_ptr<NativeHeapOwner> parent,
        NativeHeapFailure&) noexcept;
    std::optional<NativeAllocation> allocate(
        std::size_t bytes, NativeHeapFailure&, std::size_t alignment = 16) noexcept;
    // Explicit typed routing only; this does not implement source callbacks/fallback policy.
    std::optional<NativeAllocation> allocate_from(
        const std::shared_ptr<NativeHeapOwner>& actual, std::size_t bytes,
        NativeHeapFailure&, std::size_t alignment = 16) noexcept;
    std::optional<NativeHeapRequest> retain_request(NativeHeapFailure&) noexcept;
    NativeHeapObservation observe() const;
    const std::string& name() const noexcept;
    bool shutdown(NativeHeapFailure&) noexcept;
    NativeHeapOwner(const NativeHeapOwner&) = delete;
    NativeHeapOwner& operator=(const NativeHeapOwner&) = delete;
private:
    friend class NativeAllocation;
    friend class NativeWaveAllocator;
    void mark_overflow_attempted() noexcept;
    explicit NativeHeapOwner(std::shared_ptr<detail::NativeHeapState>) noexcept;
    std::optional<NativeAllocation> allocate_on(
        const std::shared_ptr<detail::NativeHeapState>& actual, std::size_t bytes,
        NativeHeapFailure&, std::size_t alignment) noexcept;
    std::shared_ptr<detail::NativeHeapState> state_;
};
} // namespace mka::memory
