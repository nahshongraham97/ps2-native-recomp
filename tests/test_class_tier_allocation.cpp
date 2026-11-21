#include "mka/class_tier_allocation.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace mka::memory;
namespace {
bool armed{};
unsigned failures{};
std::vector<std::size_t> host_sizes;
void check(bool value, const char* message) {
    if (!value) { armed = false; std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
void arm(unsigned n) { armed = false; host_sizes.clear(); host_sizes.reserve(16); failures = n; armed = true; }
void disarm() { armed = false; }
auto owner(const char* name, std::shared_ptr<NativeHeapOwner> parent = {}) {
    NativeHeapFailure f; auto result = NativeHeapOwner::create(name, std::move(parent), f);
    check(bool(result), "real native owner"); return result;
}
struct WaveSink final : WaveAllocationDiagnostics {
    std::vector<WaveAllocationDiagnostic> events;
    bool report(const WaveAllocationDiagnostic& e) override { events.push_back(e); return true; }
};
struct Sink final : TierAllocationDiagnostics {
    NativeTierAllocator* allocator{};
    std::array<std::shared_ptr<NativeHeapOwner>, 9> owners;
    std::vector<TierAllocationDiagnostic> events;
    bool reject{}, throwing{}, reenter{};
    TierDiagnosticKind fail_kind{TierDiagnosticKind::inner_fallback};
    bool report(const TierAllocationDiagnostic& e) override {
        auto probe = std::async(std::launch::async, [&] { return allocator->observe(); });
        check(probe.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
              "sink observes actual allocator without held registry mutex");
        const auto state = probe.get();
        check(state.active_requests == 1 && state.active_callbacks ==
              (e.kind == TierDiagnosticKind::inner_fallback ? 1u : 0u), "callback marker cleared only before outer diagnostics");
        const auto selected = static_cast<std::size_t>(e.requested_target);
        auto check_owner = [&](std::size_t i) {
            const auto heap = owners[i]->observe(); NativeHeapFailure f;
            check(heap.active_requests == 1 && heap.inflight_allocations == 0 && heap.live_allocations == 0,
                  "applicable route owners retained across real failed-attempt gaps");
            check(!owners[i]->shutdown(f) && f.kind == NativeHeapError::owner_busy, "route request alone blocks shutdown");
        };
        check_owner(selected); check_owner(6); check_owner(8);
        if (selected < 5) { check_owner(selected + 1); check_owner(7); }
        check(owners[selected]->observe().overflow_attempted == e.overflow_already_attempted,
              "diagnostics observe requested owner's prior sticky state");
        events.push_back(e);
        if (reenter) {
            reenter = false;
            TierAllocationFailure f; auto nested = allocator->allocate(1, f);
            check(nested.has_value(), "same-thread diagnostic reentry performs actual native allocation");
            NativeHeapFailure release; check(nested->release(release), "nested real storage retirement");
        }
        if (e.kind == fail_kind && throwing) throw std::runtime_error("fixture sink");
        return !(e.kind == fail_kind && reject);
    }
};
constexpr std::array<std::size_t, 6> capacities{64, 128, 256, 512, 2048, 4096};
constexpr std::array<AllocationClassRole, 6> roles{AllocationClassRole::size_64, AllocationClassRole::size_128,
    AllocationClassRole::size_256, AllocationClassRole::size_512, AllocationClassRole::size_2048, AllocationClassRole::size_4096};
struct Domain {
    std::array<std::shared_ptr<NativeHeapOwner>, 9> owners;
    std::shared_ptr<WaveSink> wave_sink = std::make_shared<WaveSink>();
    std::shared_ptr<Sink> sink = std::make_shared<Sink>();
    TierAllocationBindings bindings;
    std::unique_ptr<NativeTierAllocator> allocator;
    explicit Domain(bool present_overlay = true) {
        owners[6] = owner("Wave"); owners[7] = owner("actual overlay"); owners[8] = owner("overflow");
        for (std::size_t i = 0; i < 6; ++i) {
            owners[i] = owner("actual class", owners[6]); AllocationClassFailure f;
            auto c = NativeAllocationClass::create({owners[i], roles[i], AllocationClassSchema::source_kind2_payload, capacities[i], 16}, f);
            check(bool(c), "real matched class"); bindings.classes[i] = {std::move(c), owners[i]};
        }
        WaveAllocationBindings w;
        w.wave = owners[6]; w.alternate_state = present_overlay ? WaveAlternateState::ordinary_owner : WaveAlternateState::absent;
        w.alternate = present_overlay ? owners[7] : nullptr;
        w.overflow_state = WaveOverflowState::ordinary_owner; w.overflow = owners[8]; w.diagnostics = wave_sink;
        WaveAllocationFailure wf; bindings.wave_policy = NativeWaveAllocator::create(std::move(w), wf);
        check(bool(bindings.wave_policy), "actual complete outer Wave context");
        bindings.diagnostics = sink; TierAllocationFailure f;
        allocator = NativeTierAllocator::create(bindings, f);
        if (present_overlay) check(bool(allocator), "supported explicit present-overlay tier profile");
        else check(!allocator && f.kind == TierAllocationError::unsupported_overlay, "known absent is unsupported, never fake terminal null");
        sink->allocator = allocator.get(); sink->owners = owners;
    }
    void retired() {
        if (allocator) { const auto s = allocator->observe(); check(s.active_requests == 0 && s.active_callbacks == 0, "tier scopes retire"); }
        for (const auto& o : owners) {
            const auto s = o->observe();
            check(s.live_allocations == 0 && s.requested_live_allocations == 0 && s.actual_live_allocations == 0 &&
                  s.inflight_allocations == 0 && s.active_requests == 0, "all actual/requested/admission ownership retires");
        }
    }
};
void actual_result(Domain& d, std::optional<TierAllocation>& a, std::size_t requested, std::size_t actual, std::size_t bytes) {
    check(a && a->requested_size() == bytes && a->requested_owner() == d.owners[requested]->observe().identity &&
          a->actual_owner() == d.owners[actual]->observe().identity, "variant retains exact logical bytes and actual/requested owners");
    const auto index = bytes ? bytes - 1 : 0;
    static_cast<unsigned char*>(a->data())[index] = 93;
    check(static_cast<unsigned char*>(a->data())[index] == 93, "real returned host storage writable");
    NativeHeapFailure f; check(a->release(f), "real result storage frees"); d.retired();
}
void primary_and_binding() {
    Domain absent(false); absent.retired();
    Domain d; TierAllocationFailure f;
    auto bad = d.bindings; bad.wave_policy.reset();
    check(!NativeTierAllocator::create(bad, f) && f.kind == TierAllocationError::unbound_wave, "complete outer Wave policy required");
    bad = d.bindings; bad.diagnostics.reset();
    check(!NativeTierAllocator::create(bad, f) && f.kind == TierAllocationError::unbound_diagnostics, "typed tier diagnostics required");
    auto foreign_owner = owner("not a Wave child"); AllocationClassFailure cf;
    auto foreign_class = NativeAllocationClass::create({foreign_owner, roles[0], AllocationClassSchema::source_kind2_payload, 64, 16}, cf);
    bad = d.bindings; bad.classes[0] = {std::move(foreign_class), foreign_owner};
    check(!NativeTierAllocator::create(bad, f) && f.kind == TierAllocationError::invalid_class, "actual class must have source Wave parent");
    bad = d.bindings; bad.classes[0].owner = d.owners[1];
    check(!NativeTierAllocator::create(bad, f) && f.kind == TierAllocationError::invalid_class, "capability owner mismatch rejected");
    bad = d.bindings; bad.classes[0] = d.bindings.classes[1];
    check(!NativeTierAllocator::create(bad, f) && f.kind == TierAllocationError::invalid_class, "role/payload mismatch rejected");
    for (std::size_t i = 0; i < 6; ++i) {
        const auto bytes = i == 0 ? 0 : capacities[i - 1] + 1;
        arm(0); auto a = d.allocator->allocate(bytes, f); disarm();
        check(host_sizes == std::vector<std::size_t>{capacities[i]} && d.sink->events.empty(), "exact unsigned threshold chooses actual primary class");
        actual_result(d, a, i, i, bytes);
    }
}
void move_ownership() {
    Domain d; TierAllocationFailure f;
    auto first = d.allocator->allocate(12, f);
    auto second = d.allocator->allocate(65, f);
    check(first && second, "actual distinct variant move candidates");
    const auto first_id = first->actual_owner();
    TierAllocation moved(std::move(*first));
    check(!*first && first->requested_size() == 0 && first->requested_owner() == NativeHeapIdentity{} &&
          first->actual_owner() == NativeHeapIdentity{} && moved && moved.requested_size() == 12,
          "move construction empties scalar and owner identities without freeing successor");
    auto* same = &moved;
    moved = std::move(*same);
    check(moved && moved.requested_size() == 12 && moved.actual_owner() == first_id &&
          d.owners[0]->observe().actual_live_allocations == 1, "self move preserves exact owned allocation");
    moved = std::move(*second);
    check(!*second && second->requested_size() == 0 && second->requested_owner() == NativeHeapIdentity{} &&
          second->actual_owner() == NativeHeapIdentity{} && moved.requested_size() == 65 &&
          d.owners[0]->observe().actual_live_allocations == 0 && d.owners[1]->observe().actual_live_allocations == 1,
          "move assignment retires prior actual storage once and adopts successor");
    NativeHeapFailure release;
    check(moved.release(release) && !moved.release(release) && release.kind == NativeHeapError::already_released,
          "adopted result frees exactly once");
    d.retired();
    auto class_result = d.allocator->allocate(12, f);
    arm(0); auto ordinary = d.allocator->allocate(5000, f); disarm();
    check(class_result && ordinary, "class and real ordinary variant results coexist");
    TierAllocation cross(std::move(*class_result));
    cross = std::move(*ordinary);
    check(!*ordinary && ordinary->requested_size() == 0 && ordinary->requested_owner() == NativeHeapIdentity{} &&
          ordinary->actual_owner() == NativeHeapIdentity{} && cross.requested_size() == 5000 &&
          cross.actual_owner() == d.owners[6]->observe().identity &&
          d.owners[0]->observe().actual_live_allocations == 0 && d.owners[6]->observe().actual_live_allocations == 1,
          "cross-variant move retires class once and retains exact ordinary successor");
    static_cast<unsigned char*>(cross.data())[4999] = 61;
    check(static_cast<unsigned char*>(cross.data())[4999] == 61 && cross.release(release) &&
          !cross.release(release) && release.kind == NativeHeapError::already_released,
          "cross-variant successor is real writable storage and frees once");
    d.retired();
}
void callback_routes() {
    for (unsigned faults = 1; faults <= 5; ++faults) {
        Domain d; TierAllocationFailure f; arm(faults); auto a = d.allocator->allocate(12, f); disarm();
        const std::vector<std::size_t> expected = faults == 1 ? std::vector<std::size_t>{64,128} :
            faults == 2 ? std::vector<std::size_t>{64,128,16} :
            faults == 3 ? std::vector<std::size_t>{64,128,16,16} : std::vector<std::size_t>{64,128,16,16,16};
        check(host_sizes == expected, "class64 exact direct attempts include actual ordinary overlay");
        check(d.wave_sink->events.empty(), "internal direct Wave bypasses complete Wave callback diagnostics");
        if (faults < 5) actual_result(d, a, 0, faults == 1 ? 1 : faults == 2 ? 6 : faults == 3 ? 7 : 8, 12);
        else check(!a && f.kind == TierAllocationError::heap && f.heap.kind == NativeHeapError::allocation_failed,
                   "exhausted real injected attempts report terminal heap failure");
        check(d.sink->events.size() == (faults <= 3 ? 1u : faults == 4 ? 2u : 3u), "separate inner/outer/terminal diagnostic count");
        check(d.owners[0]->observe().overflow_attempted == (faults >= 4), "only actual outer overflow sets requested owner sticky state");
        d.retired();
    }
    for (std::size_t i = 1; i < 5; ++i) {
        for (unsigned faults : {3u,4u}) {
            Domain d; const auto bytes = capacities[i - 1] + 1;
            TierAllocationFailure f; arm(faults); auto a = d.allocator->allocate(bytes, f); disarm();
            std::vector<std::size_t> expected{capacities[i], capacities[i+1], (bytes+15)&~std::size_t{15}, capacities[i+1]};
            if (faults == 4) expected.push_back((bytes+15)&~std::size_t{15});
            check(host_sizes == expected && d.sink->events.size() == 2 &&
                  d.sink->events[1].completed_attempts == 3, "middle tiers preserve second diagnostic and repeated next-class attempt");
            actual_result(d, a, i, faults == 3 ? i+1 : 7, bytes);
        }
    }
    Domain d; TierAllocationFailure f; arm(2); auto a = d.allocator->allocate(2049, f); disarm();
    check(host_sizes == std::vector<std::size_t>{4096,2064,2064} && d.sink->events.empty(), "largest class repeats direct Wave without tier diagnostic");
    actual_result(d, a, 5, 6, 2049);
    Domain exhausted;
    arm(4); a = exhausted.allocator->allocate(2049, f); disarm();
    check(!a && f.kind == TierAllocationError::heap && f.heap.kind == NativeHeapError::allocation_failed &&
          host_sizes == std::vector<std::size_t>{4096,2064,2064,2064},
          "largest callback's three actual failures plus real overflow failure");
    check(exhausted.sink->events.size() == 2 &&
          exhausted.sink->events[0].kind == TierDiagnosticKind::overflow_attempt &&
          exhausted.sink->events[0].completed_attempts == 3 &&
          exhausted.sink->events[1].kind == TierDiagnosticKind::terminal_failure &&
          exhausted.sink->events[1].completed_attempts == 4 &&
          exhausted.owners[5]->observe().overflow_attempted && !exhausted.owners[6]->observe().overflow_attempted &&
          exhausted.wave_sink->events.empty(),
          "largest tier has no inner diagnostic and marks requested class only, never direct Wave");
    exhausted.retired();
}
void outer_wave_and_lifecycle() {
    Domain d; TierAllocationFailure f; arm(2); auto a = d.allocator->allocate(5000, f); disarm();
    check(host_sizes == std::vector<std::size_t>{5008,5008,5008} && d.sink->events.empty() &&
          d.wave_sink->events.size() == 1 && d.owners[6]->observe().overflow_attempted,
          "large request delegates complete Wave route once, no duplicated tier diagnostics/sticky");
    actual_result(d, a, 6, 8, 5000);
    Domain terminal;
    arm(3); a = terminal.allocator->allocate(5000, f); disarm();
    check(!a && f.kind == TierAllocationError::wave_policy &&
          f.wave.kind == WaveAllocationError::heap && f.wave.stage == WaveAllocationStage::terminal &&
          f.wave.heap.kind == NativeHeapError::allocation_failed &&
          host_sizes == std::vector<std::size_t>{5008,5008,5008},
          "large terminal failure preserves full nested Wave failure without another route");
    check(terminal.sink->events.empty() && terminal.wave_sink->events.size() == 2 &&
          terminal.wave_sink->events[0].kind == WaveDiagnosticKind::overflow_attempt &&
          terminal.wave_sink->events[1].kind == WaveDiagnosticKind::terminal_failure &&
          terminal.owners[6]->observe().overflow_attempted && !terminal.owners[0]->observe().overflow_attempted &&
          terminal.allocator->observe().active_callbacks == 0,
          "delegation adds no Tier callback/diagnostic or duplicate sticky state");
    terminal.retired();
    NativeHeapFailure close; check(d.owners[0]->shutdown(close), "explicit later lifecycle revocation");
    arm(0); a = d.allocator->allocate(1, f); disarm();
    check(!a && f.kind == TierAllocationError::heap && f.heap.kind == NativeHeapError::inactive_owner && host_sizes.empty(),
          "inactive applicable owner fails before any payload attempt, not exhaustion"); d.retired();
}
void diagnostics_and_reentry() {
    for (bool throwing : {false,true}) {
        Domain d; d.sink->reject = !throwing; d.sink->throwing = throwing;
        TierAllocationFailure f; arm(1); auto a = d.allocator->allocate(1, f); disarm();
        check(!a && f.kind == (throwing ? TierAllocationError::diagnostic_exception : TierAllocationError::diagnostic_rejected) &&
              host_sizes.size() == 1 && !d.owners[0]->observe().overflow_attempted, "failed inner sink stops route without fake overflow"); d.retired();
    }
    Domain d; d.sink->reject = true; d.sink->fail_kind = TierDiagnosticKind::overflow_attempt;
    TierAllocationFailure f; arm(4); auto a = d.allocator->allocate(1, f); disarm();
    check(!a && f.kind == TierAllocationError::diagnostic_rejected && host_sizes.size() == 4 &&
          !d.owners[0]->observe().overflow_attempted, "outer diagnostic must complete before sticky write or overflow allocation"); d.retired();
    Domain nested; nested.sink->reenter = true; arm(1); a = nested.allocator->allocate(1, f); disarm();
    check(a && nested.sink->events.size() == 1, "real same-thread diagnostic reentry returns to outer route");
    actual_result(nested, a, 0, 1, 1);
}
} // namespace
extern "C" void* __real__ZnwySt11align_val_t(std::size_t, std::align_val_t);
extern "C" void* __wrap__ZnwySt11align_val_t(std::size_t bytes, std::align_val_t alignment) {
    if (armed) { host_sizes.push_back(bytes); if (failures) { --failures; throw std::bad_alloc{}; } }
    return __real__ZnwySt11align_val_t(bytes, alignment);
}
int main() {
    primary_and_binding(); move_ownership(); callback_routes(); outer_wave_and_lifecycle(); diagnostics_and_reentry();
    std::puts("PASS_NATIVE_CLASS_TIER_ALLOCATION: 5 groups exact direct/outer routes and actual ownership; test-only host faults, no object/profile/reset or retail startup claim");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
