#pragma once
#include "mka/allocation_class.hpp"
#include "mka/wave_allocation.hpp"
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>

namespace mka::memory {
enum class TierTarget { class64, class128, class256, class512, class2048, class4096, wave, overlay, overflow };
enum class TierDiagnosticKind { inner_fallback, overflow_attempt, terminal_failure };
struct TierAllocationDiagnostic {
    TierDiagnosticKind kind;
    std::uint64_t request;
    TierTarget requested_target, next_target;
    NativeHeapIdentity requested_owner;
    std::size_t requested_bytes;
    unsigned completed_attempts;
    bool overflow_already_attempted;
};
class TierAllocationDiagnostics {
public:
    virtual ~TierAllocationDiagnostics() = default;
    // Outside owner/allocator locks; overlapping and reentrant calls are possible.
    virtual bool report(const TierAllocationDiagnostic&) = 0;
};
struct TierClassBinding {
    std::shared_ptr<NativeAllocationClass> capability;
    std::shared_ptr<NativeHeapOwner> owner;
};
struct TierAllocationBindings {
    std::array<TierClassBinding, 6> classes;
    std::shared_ptr<NativeWaveAllocator> wave_policy;
    std::shared_ptr<TierAllocationDiagnostics> diagnostics;
};
enum class TierAllocationError {
    none, unbound_class, invalid_class, unbound_wave, unsupported_overlay,
    unsupported_overflow, invalid_owner, unbound_diagnostics, heap, class_leaf,
    wave_policy, diagnostic_rejected, diagnostic_exception, counter_overflow,
    allocation_failed, native_exception
};
struct TierAllocationFailure {
    TierAllocationError kind{TierAllocationError::none};
    TierTarget target{TierTarget::class64};
    NativeHeapFailure heap;
    AllocationClassFailure class_leaf;
    WaveAllocationFailure wave;
};
struct TierAllocationObservation { std::size_t active_requests{}, active_callbacks{}; };
class NativeTierAllocator;
class TierAllocation final {
public:
    TierAllocation() noexcept = default;
    TierAllocation(TierAllocation&&) noexcept;
    TierAllocation& operator=(TierAllocation&&) noexcept;
    TierAllocation(const TierAllocation&) = delete;
    TierAllocation& operator=(const TierAllocation&) = delete;
    explicit operator bool() const noexcept;
    void* data() const noexcept;
    std::size_t requested_size() const noexcept { return logical_bytes_; }
    NativeHeapIdentity requested_owner() const noexcept;
    NativeHeapIdentity actual_owner() const noexcept;
    bool release(NativeHeapFailure&) noexcept;
private:
    friend class NativeTierAllocator;
    explicit TierAllocation(ClassAllocation&&, std::size_t) noexcept;
    explicit TierAllocation(NativeAllocation&&, std::size_t) noexcept;
    std::variant<ClassAllocation, NativeAllocation> storage_;
    std::size_t logical_bytes_{};
};
// Exact supported storage policy only; no object header/generation/task/reset effects.
class NativeTierAllocator final {
public:
    static std::unique_ptr<NativeTierAllocator> create(TierAllocationBindings, TierAllocationFailure&) noexcept;
    std::optional<TierAllocation> allocate(std::size_t, TierAllocationFailure&) noexcept;
    TierAllocationObservation observe() const;
private:
    struct Call;
    explicit NativeTierAllocator(TierAllocationBindings, std::array<std::shared_ptr<NativeHeapOwner>, 9>) noexcept;
    bool report(TierDiagnosticKind, TierTarget, TierTarget, std::uint64_t,
        std::size_t, unsigned, TierAllocationFailure&) noexcept;
    TierAllocationBindings bindings_;
    std::array<std::shared_ptr<NativeHeapOwner>, 9> owners_;
    mutable std::mutex mutex_;
    std::size_t active_requests_{}, active_callbacks_{};
    std::uint64_t next_request_{1};
};
} // namespace mka::memory
