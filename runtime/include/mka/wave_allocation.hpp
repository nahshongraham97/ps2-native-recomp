#pragma once

#include "mka/native_heap.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace mka::memory {

enum class WaveAlternateState { unbound, absent, ordinary_owner };
enum class WaveOverflowState { unbound, disabled, ordinary_owner };
enum class WaveAllocationStage { primary, alternate, overflow, terminal };
enum class WaveAllocationError {
    none, unbound_wave, unbound_alternate, invalid_alternate, unbound_overflow,
    invalid_overflow, unbound_diagnostics, heap, counter_overflow,
    diagnostic_rejected, diagnostic_exception, native_exception
};
struct WaveAllocationFailure {
    WaveAllocationError kind{WaveAllocationError::none};
    WaveAllocationStage stage{WaveAllocationStage::primary};
    NativeHeapFailure heap;
};
enum class WaveDiagnosticKind { overflow_attempt, terminal_failure };
struct WaveAllocationDiagnostic {
    WaveDiagnosticKind kind;
    NativeHeapIdentity requested_owner;
    std::uint64_t request;
    std::size_t requested_bytes;
    unsigned exhausted_attempts;
    bool overflow_already_attempted;
};
class WaveAllocationDiagnostics {
public:
    virtual ~WaveAllocationDiagnostics() = default;
    // Called without heap/domain mutexes; implementations must support overlapping
    // and reentrant calls. False or an exception blocks the route.
    virtual bool report(const WaveAllocationDiagnostic&) = 0;
};
struct WaveAllocationBindings {
    // These are real, source-resolved ordinary owners, never guest selectors.
    // The retail producer must establish the Wave kind/callback and overlay readiness.
    std::shared_ptr<NativeHeapOwner> wave;
    WaveAlternateState alternate_state{WaveAlternateState::unbound};
    std::shared_ptr<NativeHeapOwner> alternate;
    WaveOverflowState overflow_state{WaveOverflowState::unbound};
    std::shared_ptr<NativeHeapOwner> overflow;
    std::shared_ptr<WaveAllocationDiagnostics> diagnostics;
};
struct WaveAllocationObservation {
    NativeHeapIdentity requested_owner;
    std::size_t active_requests{}, active_callbacks{};
    bool overflow_attempted{};
};

// Typed source policy for 12DA40 -> 429D20 -> 4295B0/310850, flags4 only.
// This implements the transaction, not the missing retail owner/overlay producer.
class NativeWaveAllocator final {
public:
    static std::unique_ptr<NativeWaveAllocator> create(
        WaveAllocationBindings, WaveAllocationFailure&) noexcept;
    std::optional<NativeAllocation> allocate(std::size_t bytes, WaveAllocationFailure&) noexcept;
    WaveAllocationObservation observe() const;
    NativeWaveAllocator(const NativeWaveAllocator&) = delete;
    NativeWaveAllocator& operator=(const NativeWaveAllocator&) = delete;
private:
    friend class NativeTierAllocator;
    struct Call;
    NativeWaveAllocator(WaveAllocationBindings, NativeHeapIdentity) noexcept;
    bool report(WaveDiagnosticKind, std::uint64_t, std::size_t, unsigned,
                WaveAllocationFailure&) noexcept;
    WaveAllocationBindings bindings_;
    NativeHeapIdentity identity_;
    mutable std::mutex mutex_;
    std::uint64_t next_request_{1};
    std::size_t active_requests_{}, active_callbacks_{};
};
} // namespace mka::memory
