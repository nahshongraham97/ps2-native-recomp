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
    require(!requested->shutdown(f) && !actual->shutdown(f), "neither owner can close routed allocation");
    auto* pointer = allocation->data();
    require(!allocation->release_from(*requested, f) && f.kind == NativeHeapError::foreign_owner &&
            allocation->data() == pointer, "requested owner cannot spoof actual free owner");
    require(!allocation->release_from(*foreign, f) && f.kind == NativeHeapError::foreign_owner,
            "foreign identity rejected without mutation");
    require(allocation->release_from(*actual, f), "actual owner releases routed allocation");
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
void host_failure_and_concurrency() {
    auto heap = owner("host failure");
    NativeHeapFailure f;
    auto huge = heap->allocate(std::numeric_limits<std::size_t>::max() - 15, f);
    require(!huge && f.kind == NativeHeapError::allocation_failed,
            "representable but impossible host allocation reports real failure");
    require(heap->observe().live_allocations == 0 && heap->observe().inflight_allocations == 0,
            "host allocation failure rolls back reservation");
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
    std::cout << "PASS_NATIVE_HEAP_OWNER: 3 groups real aligned storage/routing/lifetime/concurrency; no retail initialization or source callback claim\n";
}
