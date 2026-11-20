#include "mka/wave_allocation.hpp"

#include <limits>
#include <new>
#include <utility>

namespace mka::memory {
namespace {
bool fail(WaveAllocationFailure& f, WaveAllocationError kind) noexcept {
    f.kind = kind; return false;
}
}
struct NativeWaveAllocator::Call {
    NativeWaveAllocator& domain;
    bool admitted{}, callback{};
    std::uint64_t request{};
    bool begin(WaveAllocationFailure& f) {
        std::lock_guard lock(domain.mutex_);
        if (domain.next_request_ == std::numeric_limits<std::uint64_t>::max() ||
            domain.active_requests_ == std::numeric_limits<std::size_t>::max() ||
            domain.active_callbacks_ == std::numeric_limits<std::size_t>::max())
            return fail(f, WaveAllocationError::counter_overflow);
        request = domain.next_request_++;
        ++domain.active_requests_; ++domain.active_callbacks_;
        admitted = callback = true;
        return true;
    }
    void end_callback() noexcept {
        if (callback) {
            std::lock_guard lock(domain.mutex_);
            --domain.active_callbacks_; callback = false;
        }
    }
    ~Call() {
        end_callback();
        if (admitted) {
            std::lock_guard lock(domain.mutex_);
            --domain.active_requests_;
        }
    }
};

NativeWaveAllocator::NativeWaveAllocator(WaveAllocationBindings b, NativeHeapIdentity id) noexcept
    : bindings_(std::move(b)), identity_(id) {}

std::unique_ptr<NativeWaveAllocator> NativeWaveAllocator::create(
    WaveAllocationBindings b, WaveAllocationFailure& f) noexcept {
    f = {};
    if (!b.wave) { fail(f, WaveAllocationError::unbound_wave); return {}; }
    if (b.alternate_state == WaveAlternateState::unbound) {
        fail(f, WaveAllocationError::unbound_alternate); return {};
    }
    if (b.alternate_state != WaveAlternateState::absent && b.alternate_state != WaveAlternateState::ordinary_owner) {
        fail(f, WaveAllocationError::invalid_alternate); return {};
    }
    if ((b.alternate_state == WaveAlternateState::ordinary_owner) != bool(b.alternate)) {
        fail(f, WaveAllocationError::invalid_alternate); return {};
    }
    if (b.overflow_state == WaveOverflowState::unbound) {
        fail(f, WaveAllocationError::unbound_overflow); return {};
    }
    if (b.overflow_state != WaveOverflowState::disabled && b.overflow_state != WaveOverflowState::ordinary_owner) {
        fail(f, WaveAllocationError::invalid_overflow); return {};
    }
    if ((b.overflow_state == WaveOverflowState::ordinary_owner) != bool(b.overflow)) {
        fail(f, WaveAllocationError::invalid_overflow); return {};
    }
    if (!b.diagnostics) { fail(f, WaveAllocationError::unbound_diagnostics); return {}; }
    try {
        const auto id = b.wave->observe().identity;
        return std::unique_ptr<NativeWaveAllocator>(new NativeWaveAllocator(std::move(b), id));
    } catch (...) { fail(f, WaveAllocationError::native_exception); return {}; }
}

WaveAllocationObservation NativeWaveAllocator::observe() const {
    const auto overflow_attempted = bindings_.wave->observe().overflow_attempted;
    std::lock_guard lock(mutex_);
    return {identity_, active_requests_, active_callbacks_, overflow_attempted};
}

bool NativeWaveAllocator::report(WaveDiagnosticKind kind, std::uint64_t request,
    std::size_t bytes, unsigned exhausted, WaveAllocationFailure& f) noexcept {
    try {
        const bool prior = bindings_.wave->observe().overflow_attempted;
        // Diagnostic completion precedes the source overflow-status write.
        if (!bindings_.diagnostics->report({kind, identity_, request, bytes, exhausted, prior}))
            return fail(f, WaveAllocationError::diagnostic_rejected);
        return true;
    } catch (...) { return fail(f, WaveAllocationError::diagnostic_exception); }
}

std::optional<NativeAllocation> NativeWaveAllocator::allocate(
    std::size_t bytes, WaveAllocationFailure& f) noexcept {
    f = {};
    try {
        // Retain every selected native owner throughout all attempts and sinks.
        // Readiness is a required producer precondition, not a guessed empty route.
        auto request = bindings_.wave->retain_request(f.heap);
        if (!request) { fail(f, WaveAllocationError::heap); return {}; }
        std::optional<NativeHeapRequest> alternate, overflow;
        if (bindings_.alternate && bindings_.alternate != bindings_.wave) {
            f.stage = WaveAllocationStage::alternate;
            alternate = bindings_.alternate->retain_request(f.heap);
            if (!alternate) { fail(f, WaveAllocationError::heap); return {}; }
        }
        if (bindings_.overflow && bindings_.overflow != bindings_.wave && bindings_.overflow != bindings_.alternate) {
            f.stage = WaveAllocationStage::overflow;
            overflow = bindings_.overflow->retain_request(f.heap);
            if (!overflow) { fail(f, WaveAllocationError::heap); return {}; }
        }
        Call call{*this};
        if (!call.begin(f)) return {};
        unsigned exhausted = 0;
        auto attempt = [&](const std::shared_ptr<NativeHeapOwner>& actual, WaveAllocationStage stage) {
            f.stage = stage;
            auto result = bindings_.wave->allocate_from(actual, bytes, f.heap, 16);
            if (!result && f.heap.kind == NativeHeapError::allocation_failed) ++exhausted;
            return result;
        };
        auto result = attempt(bindings_.wave, WaveAllocationStage::primary);
        if (result) { f = {}; return result; }
        if (f.heap.kind != NativeHeapError::allocation_failed) { fail(f, WaveAllocationError::heap); return {}; }
        if (bindings_.alternate) {
            result = attempt(bindings_.alternate, WaveAllocationStage::alternate);
            if (result) { f = {}; return result; }
            if (f.heap.kind != NativeHeapError::allocation_failed) { fail(f, WaveAllocationError::heap); return {}; }
        }
        call.end_callback();
        if (bindings_.overflow) {
            f.stage = WaveAllocationStage::overflow;
            if (!report(WaveDiagnosticKind::overflow_attempt, call.request, bytes, exhausted, f)) return {};
            bindings_.wave->mark_overflow_attempted();
            result = attempt(bindings_.overflow, WaveAllocationStage::overflow);
            if (result) { f = {}; return result; }
            if (f.heap.kind != NativeHeapError::allocation_failed) { fail(f, WaveAllocationError::heap); return {}; }
        }
        f.stage = WaveAllocationStage::terminal;
        if (!report(WaveDiagnosticKind::terminal_failure, call.request, bytes, exhausted, f)) return {};
        fail(f, WaveAllocationError::heap);
        return {};
    } catch (...) { fail(f, WaveAllocationError::native_exception); return {}; }
}
} // namespace mka::memory
