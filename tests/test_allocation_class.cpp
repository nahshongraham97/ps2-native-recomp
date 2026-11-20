#include "mka/allocation_class.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

using namespace mka::memory;
namespace {
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
auto heap(const char* name, std::shared_ptr<NativeHeapOwner> parent = {}) {
    NativeHeapFailure f; auto result = NativeHeapOwner::create(name, std::move(parent), f);
    check(bool(result), "actual native owner"); return result;
}
auto make_class(std::shared_ptr<NativeHeapOwner> owner, std::size_t payload, std::size_t alignment,
                AllocationClassRole role = AllocationClassRole::pfx) {
    AllocationClassFailure f;
    auto result = NativeAllocationClass::create({std::move(owner), role, AllocationClassSchema::source_kind2_payload, payload, alignment}, f);
    check(result && f.kind == AllocationClassError::none, "actual explicitly bound class"); return result;
}
void payload_and_fit() {
    auto parent = heap("Wave"); auto owner = heap("actual PFX", parent);
    auto allocation_class = make_class(owner, 0x310, 64);
    AllocationClassFailure f;
    auto configuration = allocation_class->observe(f);
    check(configuration && configuration->configured_payload_bytes == 0x310 &&
          configuration->payload_bytes == 0x310 && configuration->alignment == 64 &&
          configuration->owner.parent == parent->observe().identity,
          "actual immutable configuration and native parent query");
    auto zero = allocation_class->allocate(owner, 0, 16, f);
    check(zero && zero->requested_size() == 0 && zero->normalized_request_size() == 16 &&
          zero->payload_size() == 0x310 && zero->storage_size() == 0x340 &&
          reinterpret_cast<std::uintptr_t>(zero->data()) % 64 == 0,
          "zero owns full class-sized payload, separate logical and padded metadata");
    auto* bytes = static_cast<unsigned char*>(zero->data()); bytes[0] = 21; bytes[0x30f] = 99;
    check(bytes[0] == 21 && bytes[0x30f] == 99, "entire configured class payload writable despite zero caller request");
    auto exact = allocation_class->allocate(owner, 0x310, 64, f);
    check(exact && exact->requested_alignment() == 64, "exact class fit and alignment");
    check(!allocation_class->allocate(owner, 0x311, 16, f) && f.kind == AllocationClassError::class_fit,
          "rounded request beyond class rejected");
    check(!allocation_class->allocate(owner, 12, 128, f) && f.kind == AllocationClassError::request_alignment,
          "request alignment cannot exceed logical class alignment");
    check(!allocation_class->allocate(owner, 12, 24, f) && f.kind == AllocationClassError::invalid_alignment,
          "explicit byte alignment validated, not raw flags");
    check(!allocation_class->allocate(owner, std::numeric_limits<std::size_t>::max(), 16, f) &&
          f.kind == AllocationClassError::size_overflow, "request round16 checked");
    ClassAllocation moved = std::move(*zero);
    check(!*zero && moved && moved.requested_size() == 0 && moved.normalized_request_size() == 16,
          "move transfers class and backing ownership");
    moved = std::move(*exact);
    check(!*exact && owner->observe().actual_live_allocations == 1,
          "move assignment really retires prior class storage once");
    allocation_class.reset(); owner.reset(); parent.reset();
    static_cast<unsigned char*>(moved.data())[0x30f] = 76;
    check(static_cast<unsigned char*>(moved.data())[0x30f] == 76 && moved.payload_size() == 0x310,
          "class state and real storage survive wrapper destruction");
    NativeHeapFailure release; check(moved.release(release), "retained class storage really frees");
    auto one_owner = heap("alignment one"); auto one = make_class(one_owner, 1, 1);
    auto minimal = one->allocate(one_owner, 0, 1, f);
    check(minimal && minimal->payload_size() == 16 && minimal->storage_size() == 16 &&
          reinterpret_cast<std::uintptr_t>(minimal->data()) % 16 == 0,
          "logical alignment one uses valid minimum host alignment sixteen");
    check(minimal->release(release), "alignment-one real release");
}
void identity_and_game_count() {
    auto owner = heap("actual PFX"); auto requested = heap("requested tier"); auto other = heap("actual other");
    auto allocation_class = make_class(owner, 0x310, 16);
    AllocationClassFailure f; NativeHeapFailure hf;
    std::vector<ClassAllocation> live;
    for (unsigned i = 0; i < 2; ++i) {
        auto result = allocation_class->allocate(requested, 12, 16, f);
        check(result && result->requested_owner() == requested->observe().identity &&
              result->actual_owner() == owner->observe().identity, "actual class owner distinct from requested route");
        live.push_back(std::move(*result));
    }
    auto five = allocation_class->pfx_headroom(7, f);
    check(five && *five == 5 && *five >= 5, "exact signed headroom five admits source predicate");
    auto third = allocation_class->allocate(requested, 12, 16, f); check(third.has_value(), "third actual class payload");
    live.push_back(std::move(*third));
    auto four = allocation_class->pfx_headroom(7, f);
    check(four && *four == 4 && !(*four >= 5), "headroom four fails source predicate");
    auto routed_away = owner->allocate_from(other, 16, hf);
    check(routed_away && allocation_class->pfx_headroom(7, f) == four &&
          owner->observe().requested_live_allocations == 1 && owner->observe().actual_live_allocations == 3,
          "PFX request routed elsewhere does not count as actual PFX");
    for (unsigned i = 0; i < 9; ++i) {
        auto result = allocation_class->allocate(requested, 1, 16, f);
        check(result.has_value(), "dynamic class has no configured game-count slot cap");
        static_cast<unsigned char*>(result->data())[0x30f] = static_cast<unsigned char>(i);
        live.push_back(std::move(*result));
    }
    auto negative = allocation_class->pfx_headroom(2, f);
    check(negative && *negative == -10 && *negative < 5 && requested->observe().requested_live_allocations == 12 &&
          requested->observe().actual_live_allocations == 0, "negative game headroom preserved without limiting real allocations");
    live.clear();
    check(owner->observe().actual_live_allocations == 0 && requested->observe().requested_live_allocations == 0 &&
          owner->observe().requested_live_allocations == 1, "actual class frees independent of remaining requested-only allocation");
    check(routed_away->release(hf), "actual other storage free");
    check(allocation_class->pfx_headroom(2, f) == std::optional<std::int64_t>{2}, "headroom observes actual retirement");
}
void validation_and_signed_range() {
    auto owner = heap("class"); AllocationClassFailure f;
    AllocationClassConfig c{owner, AllocationClassRole::pfx, AllocationClassSchema::source_kind2_payload, 64, 16};
    auto bad = c; bad.owner.reset(); check(!NativeAllocationClass::create(bad, f) && f.kind == AllocationClassError::invalid_owner, "missing actual owner rejected");
    bad = c; bad.schema = AllocationClassSchema::unbound; check(!NativeAllocationClass::create(bad, f) && f.kind == AllocationClassError::unbound_schema, "unknown schema not fake readiness");
    bad = c; bad.role = static_cast<AllocationClassRole>(999); check(!NativeAllocationClass::create(bad, f) && f.kind == AllocationClassError::unbound_role, "unknown role rejected");
    bad = c; bad.payload_bytes = 0; check(!NativeAllocationClass::create(bad, f) && f.kind == AllocationClassError::invalid_payload, "zero class configuration rejected");
    bad = c; bad.payload_bytes = std::numeric_limits<std::size_t>::max(); check(!NativeAllocationClass::create(bad, f) && f.kind == AllocationClassError::size_overflow, "class payload rounding overflow");
    auto allocation_class = make_class(owner, 64, 16, AllocationClassRole::size_64);
    check(!allocation_class->pfx_headroom(10, f) && f.kind == AllocationClassError::wrong_role, "nonPFX count cannot impersonate PFX");
    check(!allocation_class->allocate({}, 1, 16, f) && f.kind == AllocationClassError::invalid_owner, "null requested owner rejected");
    NativeHeapFailure hf; check(owner->shutdown(hf), "idle class binding permits explicit later lifecycle revocation");
    check(!allocation_class->allocate(owner, 1, 16, f) && f.kind == AllocationClassError::heap &&
          f.heap.kind == NativeHeapError::inactive_owner && owner->observe().active_requests == 0,
          "revoked class cannot allocate or strand admission");
    check(!NativeAllocationClass::create(c, f) && f.heap.kind == NativeHeapError::inactive_owner, "inactive owner cannot publish fresh class");
    auto pfx_owner = heap("revoked PFX binding");
    auto pfx_class = make_class(pfx_owner, 0x310, 16);
    check(pfx_class->pfx_headroom(128, f) == std::optional<std::int64_t>{128}, "live PFX semantic query");
    check(pfx_owner->shutdown(hf), "revoke actual idle PFX class owner");
    auto diagnostic = pfx_class->observe(f);
    check(diagnostic && !diagnostic->owner.active, "diagnostic observation preserves inactive identity");
    check(!pfx_class->pfx_headroom(128, f) && f.kind == AllocationClassError::heap &&
          f.heap.kind == NativeHeapError::inactive_owner && pfx_owner->observe().active_requests == 0 &&
          pfx_owner->observe().live_allocations == 0 && pfx_owner->observe().inflight_allocations == 0,
          "revoked PFX cannot publish a valid game budget or leak admission");
    constexpr auto hi = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    check(checked_pfx_headroom(0, hi + 1, f) == std::optional{std::numeric_limits<std::int64_t>::min()}, "exact INT64_MIN difference fits");
    check(checked_pfx_headroom(hi, 0, f) == std::optional{std::numeric_limits<std::int64_t>::max()}, "exact INT64_MAX difference fits");
    check(!checked_pfx_headroom(hi + 1, 0, f) && f.kind == AllocationClassError::host_range, "positive headroom overflow named");
    check(!checked_pfx_headroom(0, hi + 2, f) && f.kind == AllocationClassError::host_range, "negative headroom overflow named");
    check(checked_pfx_headroom(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max(), f) == std::optional<std::int64_t>{0},
          "large individual operands accepted when exact difference fits");
}
} // namespace
int main() {
    payload_and_fit(); identity_and_game_count(); validation_and_signed_range();
    std::puts("PASS_NATIVE_ALLOCATION_CLASS: 3 groups actual class payload/ownership/fit/signed PFX predicate; no callback/object/reset or retail startup claim");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
