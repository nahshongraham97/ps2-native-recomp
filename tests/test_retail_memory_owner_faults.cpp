#include "mka/retail_memory_owners.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <new>

namespace {
// Only this executable replaces ordinary host allocation. Production has no hook.
bool armed{};
std::size_t allocation_calls{}, fail_at{};
bool injected{};
void arm(std::size_t index) noexcept {
    allocation_calls = 0; fail_at = index; injected = false; armed = true;
}
void disarm() noexcept { armed = false; }
void check(bool value, const char* message) {
    if (!value) { disarm(); std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
}
void* operator new(std::size_t bytes) {
    if (armed) {
        ++allocation_calls;
        if (fail_at && allocation_calls == fail_at) { injected = true; throw std::bad_alloc{}; }
    }
    if (auto pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc{};
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

using namespace mka::retail;
namespace {
auto create_group() {
    RetailMemoryOwnerFailure f;
    auto group = NativeRetailMemoryOwners::create(f);
    check(group && f.kind == RetailMemoryOwnerError::none, "actual group creation/recovery");
    return group;
}
void verify_group(NativeRetailMemoryOwners& group) {
    RetailMemoryOwnerFailure f;
    auto root = group.observe(RetailMemoryRole::system, f);
    auto permanent = group.observe(RetailMemoryRole::permanent, f);
    auto table = group.observe(RetailMemoryRole::section_table, f);
    check(root && permanent && table && root->heap.child_owners == 6 &&
          permanent->heap.child_owners == 1 && table->heap.child_owners == 0 &&
          permanent->heap.parent == root->heap.identity && table->heap.parent == permanent->heap.identity,
          "recovered actual parent registrations have exact source hierarchy");
    check(group.default_owner(f)->heap.identity == permanent->heap.identity &&
          group.overflow_owner(f)->heap.parent == root->heap.identity,
          "recovery publishes actual default and overflow references");
    for (auto role : {RetailMemoryRole::system, RetailMemoryRole::permanent, RetailMemoryRole::section,
                     RetailMemoryRole::mw_file, RetailMemoryRole::wave, RetailMemoryRole::mpeg,
                     RetailMemoryRole::section_table, RetailMemoryRole::overflow}) {
        auto state = group.observe(role, f);
        check(state && state->heap.active && state->heap.live_allocations == 0 &&
              state->heap.inflight_allocations == 0 && state->heap.active_requests == 0 &&
              state->heap.requested_live_allocations == 0 && state->heap.actual_live_allocations == 0,
              "recovered owners carry no leaked visible transaction or payload role");
    }
}
struct Diagnostics final : mka::memory::WaveAllocationDiagnostics {
    unsigned calls{};
    bool report(const mka::memory::WaveAllocationDiagnostic&) override { ++calls; return true; }
};
auto alternate_owner() {
    mka::memory::NativeHeapFailure f;
    auto owner = mka::memory::NativeHeapOwner::create("live alternate", {}, f);
    check(owner && f.kind == mka::memory::NativeHeapError::none, "actual live alternate");
    return owner;
}
std::size_t creation_faults() {
    RetailMemoryOwnerFailure f;
    arm(0);
    auto baseline = NativeRetailMemoryOwners::create(f);
    disarm();
    const auto measured = allocation_calls;
    check(baseline && measured > 0 && !injected, "measure actual successful group ordinary allocations");
    verify_group(*baseline);
    baseline.reset();
    for (std::size_t i = 1; i <= measured; ++i) {
        arm(i);
        auto failed = NativeRetailMemoryOwners::create(f);
        disarm();
        check(injected && allocation_calls >= i && !failed, "each measured create allocation fails unpublished");
        check(f.kind == RetailMemoryOwnerError::allocation_failed ||
              (f.kind == RetailMemoryOwnerError::heap && f.heap.kind == mka::memory::NativeHeapError::allocation_failed),
              "actual group creation failure is named");
        auto recovered = create_group();
        verify_group(*recovered);
    }
    return measured;
}
std::size_t binding_faults() {
    auto baseline = create_group();
    auto alternate = alternate_owner();
    auto diagnostics = std::make_shared<Diagnostics>();
    RetailMemoryOwnerFailure f;
    arm(0);
    const bool bound = baseline->bind_wave_initial(mka::memory::WaveAlternateState::ordinary_owner,
                                                  alternate, diagnostics, f);
    disarm();
    const auto measured = allocation_calls;
    check(bound && measured > 0 && !injected, "measure actual successful initial binding allocations");
    check(alternate->observe().active_requests == 0, "successful measurement retires admission");
    baseline.reset();
    bool candidate_failure = false, publication_failure = false;
    for (std::size_t i = 1; i <= measured; ++i) {
        auto group = create_group();
        arm(i);
        const bool result = group->bind_wave_initial(mka::memory::WaveAlternateState::ordinary_owner,
                                                     alternate, diagnostics, f);
        disarm();
        check(injected && allocation_calls >= i && !result, "each measured binding allocation fails before publication");
        check(f.kind == RetailMemoryOwnerError::allocation_failed ||
              (f.kind == RetailMemoryOwnerError::wave_binding &&
               f.wave.kind == mka::memory::WaveAllocationError::native_exception),
              "candidate or shared publication failure is named");
        candidate_failure |= f.kind == RetailMemoryOwnerError::wave_binding;
        publication_failure |= f.kind == RetailMemoryOwnerError::allocation_failed;
        check(!group->wave_allocator(f) && f.kind == RetailMemoryOwnerError::wave_unbound,
              "failed binding leaves neither published policy nor stuck binding gate");
        check(alternate->observe().active_requests == 0 && diagnostics->calls == 0,
              "failed candidate/publication releases alternate admission without diagnostics");
        verify_group(*group);
        check(group->bind_wave_initial(mka::memory::WaveAlternateState::ordinary_owner, alternate, diagnostics, f),
              "same actual group successfully retries binding after every allocation cut");
        auto wave = group->wave_allocator(f);
        mka::memory::WaveAllocationFailure wf;
        auto allocation = wave->allocate(113, wf);
        check(allocation && wf.kind == mka::memory::WaveAllocationError::none &&
              allocation->actual_owner() == group->observe(RetailMemoryRole::wave, f)->heap.identity,
              "retry uses actual private Wave host storage");
        static_cast<unsigned char*>(allocation->data())[112] = 77;
        check(static_cast<unsigned char*>(allocation->data())[112] == 77, "retry result is writable real storage");
        mka::memory::NativeHeapFailure release;
        check(allocation->release(release), "retry real host storage frees");
        verify_group(*group);
        check(alternate->observe().active_requests == 0 && diagnostics->calls == 0,
              "retry leaves all visible request ownership retired");
    }
    check(candidate_failure && publication_failure,
          "measured cuts actually reach both candidate construction and shared-control publication failures");
    return measured;
}
} // namespace
int main() {
    const auto create_count = creation_faults();
    const auto bind_count = binding_faults();
    std::printf("PASS_NATIVE_RETAIL_MEMORY_OWNER_FAULTS: create=%zu bind=%zu forced ordinary allocation cuts; owner-group/binding rollback only, no retail startup/game\n",
                create_count, bind_count);
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
