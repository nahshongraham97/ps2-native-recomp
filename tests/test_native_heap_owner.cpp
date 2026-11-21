#include "mka/native_heap.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

using namespace mka::memory;
namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::abort(); }
}
auto owner(const char* name, std::shared_ptr<NativeHeapOwner> parent = {}) {
    NativeHeapFailure f;
    auto result = NativeHeapOwner::create(name, std::move(parent), f);
    require(result && f.kind == NativeHeapError::none, "create real native owner");
    return result;
}
void allocation_and_validation() {
    auto heap = owner("ordinary");
    NativeHeapFailure f;
    require(!heap->allocate_from({}, 64, f) && f.kind == NativeHeapError::invalid_owner,
            "null explicit actual owner rejected");
    require(heap->observe().live_allocations == 0 && heap->observe().inflight_allocations == 0,
            "null route leaves no live allocation or admission");
    auto recovery = heap->allocate(64, f);
    require(recovery && f.kind == NativeHeapError::none && recovery->release(f),
            "ordinary allocation recovers after invalid route");
    auto zero = heap->allocate(0, f);
    require(zero && *zero && zero->requested_size() == 0 && zero->rounded_size() == 16,
            "zero owns real sixteen-byte allocation");
    require(heap->observe().live_allocations == 1 &&
            heap->observe().requested_live_allocations == 1 &&
            heap->observe().actual_live_allocations == 1, "same owner counts once total and once in each role");
    require(reinterpret_cast<std::uintptr_t>(zero->data()) % 16 == 0, "default alignment");
    auto* z = static_cast<unsigned char*>(zero->data());
    for (std::size_t i = 0; i < 16; ++i) z[i] = static_cast<unsigned char>(i + 1);
    for (std::size_t i = 0; i < 16; ++i) require(z[i] == i + 1, "zero allocation writable");
    auto wide = heap->allocate(65539, f, 65536);
    require(wide && wide->rounded_size() == 131072 && wide->requested_size() == 65539,
            "explicit large alignment with checked rounding");
    require(reinterpret_cast<std::uintptr_t>(wide->data()) % 65536 == 0, "host pointer aligned");
    auto* bytes = static_cast<unsigned char*>(wide->data());
    bytes[0] = 31; bytes[65538] = 97;
    NativeAllocation moved = std::move(*wide);
    require(!*wide && moved && static_cast<unsigned char*>(moved.data())[65538] == 97,
            "move transfers actual storage");
    auto replacement = heap->allocate(3, f);
    require(replacement && heap->observe().live_allocations == 3, "three live allocations");
    moved = std::move(*replacement);
    require(!*replacement && moved.requested_size() == 3 && heap->observe().live_allocations == 2,
            "move assignment releases old actual allocation exactly once");
    require(!heap->allocate(7, f, 24) && f.kind == NativeHeapError::invalid_alignment,
            "nonpower alignment rejected");
    require(!heap->allocate(7, f, 8) && f.kind == NativeHeapError::invalid_alignment,
            "below ordinary default rejected");
    require(!heap->allocate(std::numeric_limits<std::size_t>::max(), f) &&
            f.kind == NativeHeapError::size_overflow, "rounding overflow rejected before allocation");
    require(heap->observe().live_allocations == 2 && heap->observe().inflight_allocations == 0,
            "invalid requests leave no transaction");
    require(!heap->shutdown(f) && f.kind == NativeHeapError::owner_busy, "shutdown refuses live owner");
    require(moved.release(f) && zero->release(f), "explicit real release");
    require(!zero->release(f) && f.kind == NativeHeapError::already_released,
            "double release named without a second free");
    auto old = heap->observe().identity;
    require(heap->shutdown(f), "empty owner shutdown");
    auto closed = heap->observe();
    require(!closed.active && closed.identity.value == old.value &&
            closed.identity.generation == old.generation + 1, "shutdown revokes owner generation");
    require(!heap->allocate(16, f) && f.kind == NativeHeapError::inactive_owner, "no reopen after shutdown");
    require(!heap->shutdown(f) && f.kind == NativeHeapError::inactive_owner, "shutdown exact phase");
}
void routing_parent_and_lifetime() {
    auto parent = owner("parent");
    auto requested = owner("requested", parent);
    auto actual = owner("explicit alternate", parent);
    auto foreign = owner("foreign");
    NativeHeapFailure f;
    require(requested->observe().parent == parent->observe().identity &&
            parent->observe().child_owners == 2, "native parent distinct from routing");
    require(!parent->shutdown(f) && f.kind == NativeHeapError::owner_busy, "parent retains child ownership");
    auto allocation = requested->allocate_from(actual, 193, f, 64);
    require(allocation && allocation->requested_owner() == requested->observe().identity &&
            allocation->actual_owner() == actual->observe().identity, "actual owner metadata survives route");
    require(requested->observe().live_allocations == 1 && actual->observe().live_allocations == 1,
            "both route identities retain live admission");
    require(requested->observe().requested_live_allocations == 1 &&
            requested->observe().actual_live_allocations == 0 &&
            actual->observe().requested_live_allocations == 0 &&
            actual->observe().actual_live_allocations == 1, "routed roles remain distinct");
    require(!requested->shutdown(f) && !actual->shutdown(f), "neither owner can close routed allocation");
    auto* pointer = allocation->data();
    require(!allocation->release_from(*requested, f) && f.kind == NativeHeapError::foreign_owner &&
            allocation->data() == pointer, "requested owner cannot spoof actual free owner");
    require(!allocation->release_from(*foreign, f) && f.kind == NativeHeapError::foreign_owner,
            "foreign identity rejected without mutation");
    require(allocation->release_from(*actual, f), "actual owner releases routed allocation");
    require(requested->observe().requested_live_allocations == 0 &&
            requested->observe().actual_live_allocations == 0 &&
            actual->observe().requested_live_allocations == 0 &&
            actual->observe().actual_live_allocations == 0, "actual deletion retires both live roles");
    require(actual->shutdown(f), "close alternate");
    require(!requested->allocate_from(actual, 64, f) && f.kind == NativeHeapError::inactive_owner &&
            requested->observe().inflight_allocations == 0, "failed alternate rolls back requested admission");
    require(!NativeHeapOwner::create("late child", actual, f) &&
            f.kind == NativeHeapError::inactive_owner, "closed parent rejects child");
    require(requested->shutdown(f) && parent->observe().child_owners == 0 && parent->shutdown(f),
            "explicit child shutdown detaches parent once");
    auto orphan_parent = owner("retained parent");
    auto orphan = owner("allocation outlives public owners", orphan_parent);
    auto held = orphan->allocate(64, f);
    require(held.has_value(), "orphan allocation succeeds");
    auto id = held->actual_owner();
    std::weak_ptr<NativeHeapOwner> weak = orphan;
    orphan.reset(); orphan_parent.reset();
    require(weak.expired() && held->actual_owner() == id, "allocation owns independent state, not public wrapper");
    static_cast<unsigned char*>(held->data())[63] = 42;
    require(static_cast<unsigned char*>(held->data())[63] == 42 && held->release(f),
            "last owner storage remains usable and really frees");
}
void request_guard_lifetime() {
    auto parent = owner("request parent");
    auto child = owner("request child", parent);
    auto other = owner("request replacement");
    NativeHeapFailure f;
    auto guard = child->retain_request(f);
    require(guard && *guard && f.kind == NativeHeapError::none, "retain actual route-wide request");
    auto snapshot = child->observe();
    require(snapshot.active_requests == 1 && snapshot.live_allocations == 0 &&
            snapshot.inflight_allocations == 0 && snapshot.requested_live_allocations == 0 &&
            snapshot.actual_live_allocations == 0, "request admission is separate from allocation roles");
    require(!child->shutdown(f) && f.kind == NativeHeapError::owner_busy,
            "request alone blocks shutdown between route attempts");
    NativeHeapRequest moved = std::move(*guard);
    require(!*guard && moved && child->observe().active_requests == 1,
            "request move constructor preserves one admission");
    auto replacement = other->retain_request(f);
    require(replacement && *replacement, "independent real request");
    *replacement = std::move(moved);
    require(!moved && *replacement && other->observe().active_requests == 0 &&
            child->observe().active_requests == 1, "move assignment retires old guard exactly once");
    require(other->shutdown(f), "replaced request no longer blocks its owner");
    require(!other->retain_request(f) && f.kind == NativeHeapError::inactive_owner &&
            other->observe().active_requests == 0, "closed owner refuses new route request");
    child.reset();
    require(parent->observe().child_owners == 1 && !parent->shutdown(f),
            "request retains child state and parent registration after public owner disappears");
    require(replacement->release() && !replacement->release() && !*replacement,
            "request release is exactly once, double release returns false");
    require(parent->observe().child_owners == 0 && parent->shutdown(f),
            "last request release destroys child state outside its mutex and detaches parent");
    auto destructor_owner = owner("request destructor");
    {
        auto automatic = destructor_owner->retain_request(f);
        require(automatic && destructor_owner->observe().active_requests == 1,
                "automatic request owns real admission");
    }
    require(destructor_owner->observe().active_requests == 0 && destructor_owner->shutdown(f),
            "request destructor retires exactly once");
}
void payload_reset_preserves_owners() {
    auto wave = owner("Wave reset target");
    auto leaf = owner("persistent class identity", wave);
    auto unrelated = owner("unrelated actual owner");
    auto requested = owner("foreign requested identity");
    NativeHeapFailure f;
    const auto wave_id = wave->observe().identity;
    const auto leaf_id = leaf->observe().identity;
    auto direct = wave->allocate(64, f);
    auto in_child = requested->allocate_from(leaf, 96, f);
    auto outside = wave->allocate_from(unrelated, 128, f);
    require(direct && in_child && outside, "real reset payloads in distinct actual/requested domains");
    auto* retained_bytes = static_cast<unsigned char*>(in_child->data());
    retained_bytes[95] = 71;
    auto request = leaf->retain_request(f);
    require(request && !wave->reset_actual_subtree(f) && f.kind == NativeHeapError::owner_busy,
            "subtree reset refuses active request without partial retirement");
    require(*direct && *in_child && *outside && requested->observe().requested_live_allocations == 1,
            "failed reset preserves all backing/counters");
    require(request->release() && wave->reset_actual_subtree(f), "quiescent actual payload reset");
    require(!*direct && !direct->data() && !*in_child && !in_child->data() && *outside,
            "reset invalidates subtree handles but keeps foreign actual allocation");
    require(retained_bytes[95] == 71, "invalidated handle still owns physical backing until retirement");
    require(wave->observe().active && leaf->observe().active && wave->observe().identity == wave_id &&
            leaf->observe().identity == leaf_id && wave->observe().child_owners == 1,
            "payload reset preserves parent/child identities and live owner registration");
    require(requested->observe().live_allocations == 0 && leaf->observe().actual_live_allocations == 0 &&
            wave->observe().requested_live_allocations == 1 && unrelated->observe().actual_live_allocations == 1,
            "actual reset retires both roles exactly once, without requested-tag substitution");
    require(!in_child->release(f) && f.kind == NativeHeapError::already_released,
            "stale reset handle cannot free or decrement again");
    direct.reset(); in_child.reset();
    require(requested->observe().live_allocations == 0, "stale handle destructors do not underflow");
    auto replacement = leaf->allocate(160, f);
    require(replacement && *replacement && wave->reset_actual_subtree(f) && !*replacement &&
            wave->reset_actual_subtree(f), "same child allocates after reset; normal repeated empty reset succeeds");
    replacement.reset();
    auto foreign_request = requested->retain_request(f);
    auto routed = requested->allocate_from(leaf, 16, f);
    require(routed && !wave->reset_actual_subtree(f) && f.kind == NativeHeapError::owner_busy && *routed,
            "foreign request still using selected actual payload blocks reset");
    require(foreign_request->release() && wave->reset_actual_subtree(f) && !*routed,
            "foreign request completion allows correct actual retirement");
    routed.reset();
    require(outside->release(f) && leaf->shutdown(f) && wave->shutdown(f) &&
            unrelated->shutdown(f) && requested->shutdown(f), "reset owners remain normally destructible");
}
void host_failure_and_concurrency() {
    auto heap = owner("host failure");
    NativeHeapFailure f;
    auto huge = heap->allocate(std::numeric_limits<std::size_t>::max() - 15, f);
    require(!huge && f.kind == NativeHeapError::allocation_failed,
            "representable but impossible host allocation reports real failure");
    require(heap->observe().live_allocations == 0 && heap->observe().inflight_allocations == 0,
            "host allocation failure rolls back reservation");
    require(heap->observe().requested_live_allocations == 0 &&
            heap->observe().actual_live_allocations == 0, "host failure publishes neither role");
    auto anchor = heap->allocate(16, f);
    require(anchor.has_value(), "held ownership throughout concurrent operations");
    std::atomic<bool> begin{false};
    std::atomic<unsigned> finished{0};
    std::vector<std::thread> workers;
    for (unsigned n = 0; n < 6; ++n) workers.emplace_back([&, n] {
        while (!begin.load(std::memory_order_acquire)) std::this_thread::yield();
        for (unsigned i = 0; i < 200; ++i) {
            NativeHeapFailure local;
            auto a = heap->allocate(33 + n, local, 64);
            require(a.has_value(), "concurrent host allocation");
            auto* b = static_cast<unsigned char*>(a->data());
            b[0] = static_cast<unsigned char>(n); b[32 + n] = static_cast<unsigned char>(i);
            require(b[0] == n && b[32 + n] == static_cast<unsigned char>(i), "independent concurrent bytes");
        }
        ++finished;
    });
    begin.store(true, std::memory_order_release);
    while (finished.load() != 6) {
        require(!heap->shutdown(f) && f.kind == NativeHeapError::owner_busy,
                "shutdown cannot cross actual outstanding ownership");
        std::this_thread::yield();
    }
    for (auto& worker : workers) worker.join();
    require(heap->observe().live_allocations == 1 && heap->observe().inflight_allocations == 0,
            "concurrent transactions all retire exactly once");
    require(anchor->release(f) && heap->shutdown(f), "final release enables shutdown");
}
} // namespace
int main() {
    allocation_and_validation();
    routing_parent_and_lifetime();
    host_failure_and_concurrency();
    request_guard_lifetime();
    payload_reset_preserves_owners();
    std::cout << "PASS_NATIVE_HEAP_OWNER: 5 groups real aligned storage/roles/request guards/routing/lifetime/concurrency/subtree reset; complete retail initialization remains open\n";
}
