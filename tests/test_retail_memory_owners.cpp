#include "mka/retail_memory_owners.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

using namespace mka::retail;
namespace {
void check(bool value, const char* text) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", text); std::abort(); }
}
auto group() {
    RetailMemoryOwnerFailure f;
    auto result = NativeRetailMemoryOwners::create(f);
    check(result && f.kind == RetailMemoryOwnerError::none, "actual all-or-nothing native owner group");
    return result;
}
struct Diagnostics final : mka::memory::WaveAllocationDiagnostics {
    unsigned calls{};
    bool report(const mka::memory::WaveAllocationDiagnostic&) override { ++calls; return true; }
};
void ordered_identity() {
    auto owners = group();
    RetailMemoryOwnerFailure f;
    const std::array roles{RetailMemoryRole::system, RetailMemoryRole::permanent, RetailMemoryRole::section,
        RetailMemoryRole::mw_file, RetailMemoryRole::wave, RetailMemoryRole::mpeg,
        RetailMemoryRole::section_table, RetailMemoryRole::overflow};
    const char* names[]{"System", "Permanent", "Section", "mwFile", "Wave", "MPEG", "SECTION_TABLE", "OVERFLOW"};
    std::array<mka::memory::NativeHeapIdentity, 8> ids{};
    for (std::size_t i = 0; i < roles.size(); ++i) {
        auto item = owners->observe(roles[i], f);
        check(item && item->heap.active && std::strcmp(item->name, names[i]) == 0,
              "source ordered native name and active identity");
        ids[i] = item->heap.identity;
        check(ids[i].generation == 1 && (i == 0 || ids[i - 1].value < ids[i].value),
              "actual creation identity order, no source heap IDs");
        check(item->heap.live_allocations == 0 && item->heap.inflight_allocations == 0 &&
              item->heap.active_requests == 0, "logical group creation carves no payload arena");
        if (i == 0) check(!item->heap.parent && !item->overflow_enabled, "root has no parent or guessed overflow policy");
        else check(item->heap.parent == ids[i == 6 ? 1 : 0] && item->overflow_enabled == true,
                   "source parent and known child overflow metadata");
    }
    check(owners->observe(RetailMemoryRole::system, f)->heap.child_owners == 6 &&
          owners->observe(RetailMemoryRole::permanent, f)->heap.child_owners == 1,
          "SECTION_TABLE belongs to Permanent; six ordinary logical root children");
    check(owners->default_owner(f)->heap.identity == ids[1] &&
          owners->overflow_owner(f)->heap.identity == ids[7] && ids[0] != ids[1] && ids[1] != ids[7],
          "root default and overflow are distinct actual references");
    for (auto role : {RetailMemoryRole::disabled_first, RetailMemoryRole::disabled_second})
        check(!owners->observe(role, f) && f.kind == RetailMemoryOwnerError::absent_role,
              "source skipped slots are absent, not fake created owners");
    check(!owners->observe(static_cast<RetailMemoryRole>(999), f) &&
          f.kind == RetailMemoryOwnerError::invalid_role, "unknown role fails by name");
    auto second = group();
    check(second->default_owner(f)->heap.identity != ids[1], "independent groups do not share global owners");
}
void policies_fail_closed() {
    auto owners = group();
    RetailMemoryOwnerFailure f;
    for (auto role : {RetailMemoryRole::system, RetailMemoryRole::wave, RetailMemoryRole::mpeg, RetailMemoryRole::section_table}) {
        check(!owners->allocate_ordinary(role, 64, f) && f.kind == RetailMemoryOwnerError::unbound_allocator_policy,
              "root callback and fixed policies cannot masquerade as direct ordinary storage");
        check(owners->observe(role, f)->heap.live_allocations == 0, "unbound policy admits no payload");
    }
    check(!owners->wave_allocator(f) && f.kind == RetailMemoryOwnerError::wave_unbound,
          "Wave route remains unbound after identity creation");
    auto sink = std::make_shared<Diagnostics>();
    check(!owners->bind_wave_initial(mka::memory::WaveAlternateState::unbound, {}, sink, f) &&
          f.kind == RetailMemoryOwnerError::wave_binding &&
          f.wave.kind == mka::memory::WaveAllocationError::unbound_alternate,
          "unknown alternate is not silently absent");
    check(!owners->bind_wave_initial(mka::memory::WaveAlternateState::ordinary_owner, {}, sink, f) &&
          f.wave.kind == mka::memory::WaveAllocationError::invalid_alternate, "actual alternate required");
    check(!owners->bind_wave_initial(mka::memory::WaveAlternateState::absent, {}, {}, f) &&
          f.wave.kind == mka::memory::WaveAllocationError::unbound_diagnostics, "diagnostics required before binding");
    check(sink->calls == 0 && owners->observe(RetailMemoryRole::wave, f)->heap.active_requests == 0 &&
          !owners->wave_allocator(f), "invalid binding does no allocation/sink work and restores admission");
    auto allocation = owners->allocate_ordinary(RetailMemoryRole::permanent, 79, f, 64);
    check(allocation && allocation->requested_size() == 79 &&
          allocation->actual_owner() == owners->default_owner(f)->heap.identity,
          "supported direct ordinary leaf uses actual Permanent identity");
    mka::memory::NativeHeapFailure release;
    check(allocation->release(release), "real direct leaf free");
}
void inactive_alternate_retry() {
    auto owners = group();
    auto sink = std::make_shared<Diagnostics>();
    mka::memory::NativeHeapFailure hf;
    auto closed = mka::memory::NativeHeapOwner::create("closed alternate", {}, hf);
    check(closed && closed->shutdown(hf), "actual alternate closed before binding");
    RetailMemoryOwnerFailure f;
    check(!owners->bind_wave_initial(mka::memory::WaveAlternateState::ordinary_owner, closed, sink, f) &&
          f.kind == RetailMemoryOwnerError::heap && f.heap.kind == mka::memory::NativeHeapError::inactive_owner &&
          f.wave.kind == mka::memory::WaveAllocationError::heap &&
          f.wave.stage == mka::memory::WaveAllocationStage::alternate &&
          f.wave.heap.kind == mka::memory::NativeHeapError::inactive_owner,
          "inactive actual alternate is rejected before permanent binding");
    check(!owners->wave_allocator(f) && f.kind == RetailMemoryOwnerError::wave_unbound &&
          sink->calls == 0 && closed->observe().active_requests == 0,
          "failed admission publishes no Wave and retains no request");
    auto live = mka::memory::NativeHeapOwner::create("live alternate", {}, hf);
    check(live && owners->bind_wave_initial(mka::memory::WaveAlternateState::ordinary_owner, live, sink, f),
          "same group retries initial binding with real live alternate");
    check(live->observe().active_requests == 0, "initial alternate admission retires after publication");
    auto wave = owners->wave_allocator(f);
    mka::memory::WaveAllocationFailure wf;
    auto allocation = wave->allocate(97, wf);
    check(allocation && allocation->actual_owner() == owners->observe(RetailMemoryRole::wave, f)->heap.identity &&
          live->observe().live_allocations == 0 && sink->calls == 0,
          "successful retry allocates actual private Wave first, not arbitrary alternate");
    check(allocation->release(hf), "retry's real storage releases");
    check(live->shutdown(hf), "later alternate revocation remains an independent lifecycle gate");
    allocation = wave->allocate(97, wf);
    check(!allocation && wf.kind == mka::memory::WaveAllocationError::heap &&
          wf.heap.kind == mka::memory::NativeHeapError::inactive_owner,
          "later lifecycle revocation is named, not implied overlay producer readiness");
}
void bound_wave_and_retained_storage() {
    auto owners = group();
    RetailMemoryOwnerFailure f;
    auto sink = std::make_shared<Diagnostics>();
    check(owners->bind_wave_initial(mka::memory::WaveAlternateState::absent, {}, sink, f),
          "explicit known absent alternate binds actual private Wave and OVERFLOW");
    auto wave = owners->wave_allocator(f);
    check(wave && wave == owners->wave_allocator(f), "retained exact once-bound Wave policy");
    check(!owners->bind_wave_initial(mka::memory::WaveAlternateState::absent, {}, sink, f) &&
          f.kind == RetailMemoryOwnerError::wave_already_bound && wave == owners->wave_allocator(f),
          "overlay overwrite denied without inventing retirement");
    const auto wave_id = owners->observe(RetailMemoryRole::wave, f)->heap.identity;
    mka::memory::WaveAllocationFailure wf;
    constexpr std::size_t fixture_bytes = 3 * 1024 * 1024 + 17;
    auto allocation = wave->allocate(fixture_bytes, wf);
    check(allocation && wf.kind == mka::memory::WaveAllocationError::none &&
          allocation->requested_owner() == wave_id && allocation->actual_owner() == wave_id,
          "real Wave storage exceeds old source Wave budget without a native ceiling");
    auto state = owners->observe(RetailMemoryRole::wave, f)->heap;
    check(state.live_allocations == 1 && state.requested_live_allocations == 1 &&
          state.actual_live_allocations == 1 && state.active_requests == 0 && sink->calls == 0,
          "actual Wave roles and transaction completed without fabricated diagnostics");
    auto* bytes = static_cast<unsigned char*>(allocation->data());
    bytes[0] = 31; bytes[fixture_bytes - 1] = 93;
    owners.reset(); wave.reset();
    check(bytes[0] == 31 && bytes[fixture_bytes - 1] == 93 && allocation->actual_owner() == wave_id,
          "allocation retains actual owner State and parent after group/policy wrapper destruction");
    mka::memory::NativeHeapFailure release;
    check(allocation->release(release), "retained storage frees after public owner group disappears");
}
} // namespace
int main() {
    ordered_identity(); policies_fail_closed(); inactive_alternate_retry(); bound_wave_and_retained_storage();
    std::puts("PASS_NATIVE_RETAIL_MEMORY_OWNERS: 4 groups ordered identities/policies/real storage; owner-group phase only, no full retail main");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
