#include "mka/wave_allocation.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace mka::memory;
namespace {
constexpr std::size_t request_size = 90001, host_size = 90016;
std::atomic<unsigned> failures{}, attempts{};
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
auto owner(const char* name) {
    NativeHeapFailure f; auto result = NativeHeapOwner::create(name, {}, f);
    check(bool(result), "actual native owner"); return result;
}
void arm(unsigned count) { failures = count; attempts = 0; }
struct Diagnostics final : WaveAllocationDiagnostics {
    NativeWaveAllocator* allocator{};
    std::vector<std::shared_ptr<NativeHeapOwner>> owners;
    std::vector<WaveAllocationDiagnostic> events;
    bool reject{}, throwing{}, reenter{};
    bool report(const WaveAllocationDiagnostic& event) override {
        auto probe = std::async(std::launch::async, [&] { return allocator->observe(); });
        check(probe.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
              "sink can inspect domain without a held domain mutex");
        const auto state = probe.get();
        check(state.active_requests == 1 && state.active_callbacks == 0,
              "Wave callback ends before diagnostics; outer transaction remains active");
        check(state.overflow_attempted == event.overflow_already_attempted,
              "diagnostic observes status before source status write");
        for (const auto& o : owners) {
            const auto heap = o->observe(); NativeHeapFailure f;
            check(heap.live_allocations == 0 && heap.inflight_allocations == 0 && heap.active_requests == 1,
                  "all selected owners retained between failed allocation attempts");
            check(!o->shutdown(f) && f.kind == NativeHeapError::owner_busy,
                  "route admission alone excludes shutdown across callback/sink");
        }
        events.push_back(event);
        if (reenter && event.kind == WaveDiagnosticKind::overflow_attempt) {
            WaveAllocationFailure f; auto nested = allocator->allocate(48, f);
            check(nested && nested->requested_owner() == event.requested_owner &&
                  nested->actual_owner() == event.requested_owner, "diagnostic can reenter actual native primary allocation");
            NativeHeapFailure release; check(nested->release(release), "nested real allocation retirement");
            check(allocator->observe().active_requests == 1 && allocator->observe().active_callbacks == 0,
                  "nested scope returns to outer admission");
        }
        if (throwing) throw std::runtime_error("fixture sink failure");
        return !reject;
    }
};
struct Domain {
    std::shared_ptr<NativeHeapOwner> wave = owner("Wave"), alternate = owner("Wave overlay"), overflow = owner("Overflow");
    std::shared_ptr<Diagnostics> sink = std::make_shared<Diagnostics>();
    std::unique_ptr<NativeWaveAllocator> allocator;
    explicit Domain(bool have_alternate = true, bool have_overflow = true) {
        WaveAllocationBindings b;
        b.wave = wave;
        b.alternate_state = have_alternate ? WaveAlternateState::ordinary_owner : WaveAlternateState::absent;
        b.alternate = have_alternate ? alternate : nullptr;
        b.overflow_state = have_overflow ? WaveOverflowState::ordinary_owner : WaveOverflowState::disabled;
        b.overflow = have_overflow ? overflow : nullptr; b.diagnostics = sink;
        WaveAllocationFailure f; allocator = NativeWaveAllocator::create(std::move(b), f);
        check(bool(allocator), "explicit known typed Wave bindings"); sink->allocator = allocator.get();
        sink->owners = {wave};
        if (have_alternate) sink->owners.push_back(alternate);
        if (have_overflow) sink->owners.push_back(overflow);
    }
    void retired() {
        check(allocator->observe().active_requests == 0 && allocator->observe().active_callbacks == 0,
              "domain scopes retired");
        for (const auto& heap : {wave, alternate, overflow}) {
            const auto s = heap->observe();
            check(s.live_allocations == 0 && s.inflight_allocations == 0 && s.active_requests == 0 &&
                  s.requested_live_allocations == 0 && s.actual_live_allocations == 0, "all native ownership roles retired");
        }
    }
};
void bindings() {
    WaveAllocationFailure f; WaveAllocationBindings b;
    arm(0); check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::unbound_wave, "missing actual Wave owner");
    b.wave = owner("Wave");
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::unbound_alternate, "unknown overlay is not absent");
    b.alternate_state = WaveAlternateState::absent;
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::unbound_overflow, "unknown overflow is not disabled");
    b.overflow_state = WaveOverflowState::disabled;
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::unbound_diagnostics, "missing diagnostic implementation");
    b.diagnostics = std::make_shared<Diagnostics>(); b.alternate_state = WaveAlternateState::ordinary_owner;
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::invalid_alternate, "known alternate must carry actual owner");
    b.alternate_state = static_cast<WaveAlternateState>(99);
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::invalid_alternate, "unknown enum is not absent");
    b.alternate_state = WaveAlternateState::absent; b.overflow_state = WaveOverflowState::ordinary_owner;
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::invalid_overflow, "enabled overflow must carry actual owner");
    b.overflow_state = static_cast<WaveOverflowState>(99);
    check(!NativeWaveAllocator::create(b, f) && f.kind == WaveAllocationError::invalid_overflow, "unknown enum is not disabled");
    check(attempts == 0 && b.wave->observe().active_requests == 0, "invalid binding cannot allocate or retain a request");
}
void primary() {
    Domain d; arm(0); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
    check(allocation && f.kind == WaveAllocationError::none && attempts == 1 && d.sink->events.empty(), "actual primary succeeds once");
    check(allocation->requested_owner() == d.wave->observe().identity && allocation->actual_owner() == d.wave->observe().identity &&
          allocation->alignment() == 16 && allocation->requested_size() == request_size, "exact native flags4 contract");
    auto state = d.wave->observe();
    check(state.live_allocations == 1 && state.requested_live_allocations == 1 && state.actual_live_allocations == 1,
          "same-owner actual and requested roles with one allocation");
    static_cast<unsigned char*>(allocation->data())[request_size - 1] = 42;
    check(static_cast<unsigned char*>(allocation->data())[request_size - 1] == 42, "real writable storage");
    NativeHeapFailure release; check(allocation->release(release), "primary real free"); d.retired();
}
void alternate() {
    Domain d; arm(1); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
    check(allocation && attempts == 2 && d.sink->events.empty(), "alternate after actual primary allocation failure");
    check(allocation->requested_owner() == d.wave->observe().identity && allocation->actual_owner() == d.alternate->observe().identity,
          "actual alternate is retained independently");
    check(d.wave->observe().actual_live_allocations == 0 && d.wave->observe().requested_live_allocations == 1 &&
          d.alternate->observe().actual_live_allocations == 1 && d.alternate->observe().requested_live_allocations == 0,
          "source actual-only overlay count is distinct from requested count");
    NativeHeapFailure release; check(allocation->release_from(*d.alternate, release), "free through actual alternate"); d.retired();
}
void overflow() {
    Domain d; arm(2); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
    check(allocation && attempts == 3 && allocation->actual_owner() == d.overflow->observe().identity, "overflow is third real owner");
    check(d.sink->events.size() == 1 && d.sink->events[0].kind == WaveDiagnosticKind::overflow_attempt &&
          d.sink->events[0].exhausted_attempts == 2 && !d.sink->events[0].overflow_already_attempted &&
          d.allocator->observe().overflow_attempted, "overflow diagnostic precedes sticky requested-owner state");
    NativeHeapFailure release; check(allocation->release(release), "overflow real free");
    WaveAllocationBindings shared;
    shared.wave = d.wave; shared.alternate_state = WaveAlternateState::ordinary_owner; shared.alternate = d.alternate;
    shared.overflow_state = WaveOverflowState::ordinary_owner; shared.overflow = d.overflow; shared.diagnostics = d.sink;
    auto sibling = NativeWaveAllocator::create(shared, f);
    check(sibling && sibling->observe().overflow_attempted && d.wave->observe().overflow_attempted,
          "two wrappers observe the same actual owner-generation sticky status");
    sibling.reset(); d.allocator.reset();
    d.allocator = NativeWaveAllocator::create(std::move(shared), f); d.sink->allocator = d.allocator.get();
    check(d.allocator && d.allocator->observe().overflow_attempted,
          "recreating all routing wrappers cannot reset the live Wave owner's status");
    arm(0); allocation = d.allocator->allocate(request_size, f);
    check(allocation && attempts == 1 && d.allocator->observe().overflow_attempted && d.sink->events.size() == 1,
          "later primary success does not clear requested-generation overflow state");
    check(allocation->release(release), "later primary free"); d.retired();
}
void terminal() {
    Domain d; arm(3); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
    check(!allocation && attempts == 3 && f.kind == WaveAllocationError::heap && f.stage == WaveAllocationStage::terminal &&
          f.heap.kind == NativeHeapError::allocation_failed, "exhausted typed route reports actual failure");
    check(d.sink->events.size() == 2 && d.sink->events[1].kind == WaveDiagnosticKind::terminal_failure &&
          d.sink->events[1].overflow_already_attempted && d.sink->events[1].exhausted_attempts == 3,
          "separate terminal diagnostic follows failed overflow"); d.retired();
    Domain disabled(false, false); arm(1); allocation = disabled.allocator->allocate(request_size, f);
    check(!allocation && attempts == 1 && disabled.sink->events.size() == 1 &&
          disabled.sink->events[0].kind == WaveDiagnosticKind::terminal_failure && !disabled.allocator->observe().overflow_attempted,
          "explicit known absent and disabled do not create imaginary attempts"); disabled.retired();
    Domain no_alternate(false, true); arm(1); allocation = no_alternate.allocator->allocate(request_size, f);
    check(allocation && attempts == 2 && no_alternate.sink->events[0].exhausted_attempts == 1,
          "known absent alternate goes directly to diagnosed overflow");
    NativeHeapFailure release; check(allocation->release(release), "known-absent route real free"); no_alternate.retired();
}
void diagnostic_errors() {
    for (bool throwing : {false, true}) {
        Domain d; d.sink->throwing = throwing; d.sink->reject = !throwing;
        arm(2); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
        check(!allocation && attempts == 2 && f.kind == (throwing ? WaveAllocationError::diagnostic_exception : WaveAllocationError::diagnostic_rejected),
              "uncompleted native diagnostic is named, never bypassed");
        check(!d.allocator->observe().overflow_attempted && d.sink->events.size() == 1,
              "diagnostic failure cannot claim completed source overflow status/attempt"); d.retired();
    }
}
void lifecycle_errors() {
    Domain d; NativeHeapFailure close; check(d.alternate->shutdown(close), "close selected alternate before transaction");
    arm(0); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
    check(!allocation && attempts == 0 && f.kind == WaveAllocationError::heap && f.stage == WaveAllocationStage::alternate &&
          f.heap.kind == NativeHeapError::inactive_owner && d.sink->events.empty(), "lifecycle invalidity is not source memory exhaustion"); d.retired();
    Domain overflow_size; arm(0); allocation = overflow_size.allocator->allocate(std::numeric_limits<std::size_t>::max(), f);
    check(!allocation && attempts == 0 && f.kind == WaveAllocationError::heap && f.stage == WaveAllocationStage::primary &&
          f.heap.kind == NativeHeapError::size_overflow && overflow_size.sink->events.empty(), "checked host arithmetic does not trigger fallback"); overflow_size.retired();
}
void reentrant_diagnostic() {
    Domain d; d.sink->reenter = true; arm(2); WaveAllocationFailure f; auto allocation = d.allocator->allocate(request_size, f);
    check(allocation && attempts == 3 && d.sink->events.size() == 1, "source-ordered outer route survives real nested native request");
    NativeHeapFailure release; check(allocation->release(release), "outer reentrant route real free"); d.retired();
}
}
extern "C" void* __real__ZnwySt11align_val_t(std::size_t, std::align_val_t);
extern "C" void* __wrap__ZnwySt11align_val_t(std::size_t bytes, std::align_val_t alignment) {
    if (bytes == host_size && alignment == std::align_val_t{16}) {
        ++attempts; auto n = failures.load();
        while (n && !failures.compare_exchange_weak(n, n - 1)) {}
        if (n) throw std::bad_alloc{};
    }
    return __real__ZnwySt11align_val_t(bytes, alignment);
}
int main() {
    bindings(); primary(); alternate(); overflow(); terminal(); diagnostic_errors(); lifecycle_errors(); reentrant_diagnostic();
    std::puts("PASS_NATIVE_WAVE_ALLOCATION: 8 groups source-ordered native routes/ownership/diagnostics; test-only host faults, no retail producer execution");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
