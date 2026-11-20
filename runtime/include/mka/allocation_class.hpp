#pragma once
#include "mka/native_heap.hpp"
#include <cstdint>
#include <memory>
#include <optional>

namespace mka::memory {
enum class AllocationClassRole {
    unbound, pfx, mkobj, mkbone, mksobj, mkproc,
    size_64, size_128, size_256, size_512, size_2048, size_4096, tiny_stack, big_stack
};
// Supported direct native payload only; no typed object-init or callback route implied.
enum class AllocationClassSchema { unbound, source_kind2_payload };
enum class AllocationClassError {
    none, invalid_owner, unbound_role, unbound_schema, invalid_payload,
    invalid_alignment, size_overflow, request_alignment, class_fit, heap,
    allocation_failed, native_exception, wrong_role, host_range
};
struct AllocationClassFailure {
    AllocationClassError kind{AllocationClassError::none};
    NativeHeapFailure heap;
};
struct AllocationClassConfig {
    std::shared_ptr<NativeHeapOwner> owner;
    AllocationClassRole role{AllocationClassRole::unbound};
    AllocationClassSchema schema{AllocationClassSchema::unbound};
    std::size_t payload_bytes{};
    std::size_t alignment{16};
};
struct AllocationClassObservation {
    AllocationClassRole role;
    AllocationClassSchema schema;
    std::size_t configured_payload_bytes, payload_bytes, alignment;
    NativeHeapObservation owner;
};
namespace detail { struct AllocationClassState; }
class NativeAllocationClass;
class ClassAllocation final {
public:
    ClassAllocation() noexcept = default;
    ~ClassAllocation() = default;
    ClassAllocation(ClassAllocation&&) noexcept;
    ClassAllocation& operator=(ClassAllocation&&) noexcept;
    ClassAllocation(const ClassAllocation&) = delete;
    ClassAllocation& operator=(const ClassAllocation&) = delete;
    explicit operator bool() const noexcept { return static_cast<bool>(storage_); }
    void* data() const noexcept { return storage_.data(); }
    std::size_t requested_size() const noexcept { return requested_bytes_; }
    std::size_t normalized_request_size() const noexcept { return normalized_bytes_; }
    std::size_t requested_alignment() const noexcept { return requested_alignment_; }
    std::size_t payload_size() const noexcept;
    std::size_t storage_size() const noexcept { return storage_.rounded_size(); }
    NativeHeapIdentity requested_owner() const noexcept { return storage_.requested_owner(); }
    NativeHeapIdentity actual_owner() const noexcept { return storage_.actual_owner(); }
    bool release(NativeHeapFailure&) noexcept;
private:
    friend class NativeAllocationClass;
    std::shared_ptr<const detail::AllocationClassState> state_;
    NativeAllocation storage_;
    std::size_t requested_bytes_{}, normalized_bytes_{}, requested_alignment_{};
};

// Logical class payload leaf, not a completed retail callback/object-init route.
class NativeAllocationClass final {
public:
    static std::unique_ptr<NativeAllocationClass> create(AllocationClassConfig, AllocationClassFailure&) noexcept;
    std::optional<ClassAllocation> allocate(const std::shared_ptr<NativeHeapOwner>& requested_owner,
        std::size_t bytes, std::size_t requested_alignment, AllocationClassFailure&) noexcept;
    std::optional<AllocationClassObservation> observe(AllocationClassFailure&) const noexcept;
    std::optional<std::int64_t> pfx_headroom(std::uint64_t configured_game_count, AllocationClassFailure&) const noexcept;
private:
    explicit NativeAllocationClass(std::shared_ptr<const detail::AllocationClassState>) noexcept;
    std::shared_ptr<const detail::AllocationClassState> state_;
};
// Semantic signed game predicate only; never an allocation capacity restriction.
std::optional<std::int64_t> checked_pfx_headroom(std::uint64_t configured_game_count,
    std::uint64_t actual_live_count, AllocationClassFailure&) noexcept;
} // namespace mka::memory
