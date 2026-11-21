#include "mka/retail_text_storage.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>

using namespace mka::retail;
namespace {
void check(bool okay,const char* label) {
    if(!okay) { std::fprintf(stderr,"native text storage: %s\n",label); std::abort(); }
}
struct Diagnostics final : mka::memory::WaveAllocationDiagnostics {
    bool report(const mka::memory::WaveAllocationDiagnostic&) override { return true; }
};
}
int main() {
    using namespace mka::memory;
    NativeHeapFailure hf;
    auto heap=NativeHeapOwner::create("real text Wave",{},hf);
    auto overflow=NativeHeapOwner::create("real overflow",{},hf);
    WaveAllocationFailure wf;
    WaveAllocationBindings bindings;
    bindings.wave=heap;
    bindings.alternate_state=WaveAlternateState::absent;
    bindings.overflow_state=WaveOverflowState::ordinary_owner;
    bindings.overflow=overflow;
    bindings.diagnostics=std::make_shared<Diagnostics>();
    std::shared_ptr<NativeWaveAllocator> wave=NativeWaveAllocator::create(std::move(bindings),wf);
    RetailGamePoolFailure pf;
    std::shared_ptr<NativeRetailGamePool> pool=NativeRetailGamePool::initialize(1,pf);
    RetailTextStorageFailure f;
    auto storage=NativeRetailTextStorage::create(wave,pool,f);
    check(bool(storage)&&storage->same_bindings(wave,pool,f),"real Wave/pool service publication");
    auto text=storage->allocate_text(91,f);
    check(text&&*text&&heap->observe().actual_live_allocations==1,"12DA40 actual native writable storage");
    static_cast<unsigned char*>(text->data())[90]=71;
    auto route=storage->retire_text(*text,0,f);
    check(route==RetailStorageReleaseRoute::deferred&&!*text&&pool->observe(pf)->active_records==1,
          "16FDA0 count4 consumes allocation into same real pool deferred list");
    for(int n=0;n<3;++n)
        check(storage->advance_frame_from_12d930(f)&&heap->observe().actual_live_allocations==1,
              "source count4 retains actual storage through first three frames");
    check(storage->advance_frame_from_12d930(f)&&heap->observe().actual_live_allocations==0&&
          pool->observe(pf)->active_records==0,"fourth frame really frees allocation then pool record");
    for(const auto phase:{1u,2u,0xffffffffu}) {
        auto allocation=storage->allocate_text(16,f);
        check(allocation&&storage->retire_text(*allocation,phase,f)==RetailStorageReleaseRoute::immediate_phase&&
              !*allocation&&heap->observe().actual_live_allocations==0,
              "every nonzero source phase uses immediate real free");
    }
    // Original signed ADDIU/BGTZ edges, not the decompiler's unsigned form.
    for(const auto count:{0u,1u,0xffffffffu,0x80000001u}) {
        auto allocation=storage->allocate_text(17,f);
        check(allocation&&storage->retire_source_storage(*allocation,0,count,f)==RetailStorageReleaseRoute::deferred&&
              storage->advance_frame_from_12d930(f)&&heap->observe().actual_live_allocations==0,
              "signed zero/negative wrapped decrement frees on first frame");
    }
    std::array<NativeAllocation,4> source;
    for(auto& allocation:source) {
        auto next=storage->allocate_text(19,f); check(bool(next),"actual pending allocation"); allocation=std::move(*next);
        check(storage->retire_source_storage(allocation,0,2,f)==RetailStorageReleaseRoute::deferred,
              "deferred records grow beyond source initial reserve");
    }
    check(pool->observe(pf)->active_records==4&&pool->observe(pf)->native_records>=4,
          "no original game-pool capacity restriction");
    check(storage->advance_frame_from_12d930(f)&&storage->advance_frame_from_12d930(f)&&
          storage->observe(f)->pending==0&&heap->observe().live_allocations==0&&pool->observe(pf)->active_records==0,
          "one head traversal frees every due allocation and returns all real pool records");
    auto other_pool=std::shared_ptr<NativeRetailGamePool>(NativeRetailGamePool::initialize(1,pf));
    check(!storage->same_bindings(wave,other_pool,f)&&f.kind==RetailTextStorageError::foreign_owner,
          "repeated16FD80 cannot silently replace pool identity");
    auto wrap_positive=storage->allocate_text(16,f);
    check(wrap_positive&&storage->retire_source_storage(*wrap_positive,0,0x80000000u,f)==RetailStorageReleaseRoute::deferred&&
          storage->advance_frame_from_12d930(f)&&storage->observe(f)->pending==1&&
          heap->observe().live_allocations==1,"INT32_MIN decrement wraps positive and remains pending");
    storage.reset();
    check(heap->observe().live_allocations==0&&pool->observe(pf)->active_records==0,
          "native service owner teardown releases backing and its pool ownership");
    std::puts("PASS_NATIVE_RETAIL_TEXT_STORAGE: real Wave allocation, exact4-frame/signed countdown, nonzero phase, shared growing pool; game text producer integration open");
}
