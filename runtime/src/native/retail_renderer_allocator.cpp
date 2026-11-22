#include "mka/retail_renderer_allocator.hpp"
#include <algorithm>
#include <cstring>
#include <new>
namespace mka::retail {
std::shared_ptr<NativeRetailRendererAllocatorService> NativeRetailRendererAllocatorService::create(
    NativeRetailMemoryOwners& memory,RetailRendererAllocatorFailure& f) noexcept {
    f={};
    //3113FC publishes the actual Permanent role,not a new host heap.
    try {
        auto owner=memory.owners_[1];
        if(!owner) {f={RetailRendererAllocatorError::permanent_owner,"220900 native Permanent owner missing"};return {};}
        const auto observed=owner->observe();
        if(!observed.active) {f={RetailRendererAllocatorError::permanent_owner,"220900 existing native Permanent owner inactive"};return {};}
        return std::shared_ptr<NativeRetailRendererAllocatorService>(new NativeRetailRendererAllocatorService(std::move(owner),observed.identity));
    }
    catch(const std::bad_alloc&) {f={RetailRendererAllocatorError::allocation,"220900 native allocation service construction"};return {};}
    catch(...) {f={RetailRendererAllocatorError::native_exception,"220900 native allocation service construction"};return {};}
}
std::optional<memory::NativeAllocation> NativeRetailRendererAllocatorService::allocate(
    std::size_t bytes,RetailRendererAllocatorFailure& f) noexcept {
    f={};auto allocation=heap_->allocate(bytes,f.heap,16);
    if(!allocation) {f.kind=RetailRendererAllocatorError::heap;f.operation="2209B0 actual native Permanent allocation";}
    return allocation;
}
std::optional<memory::NativeAllocation> NativeRetailRendererAllocatorService::allocate_zeroed_source_words(
    std::uint32_t count,std::uint32_t bytes,RetailRendererAllocatorFailure& f) noexcept {
    const std::uint32_t product=count*bytes;
    const std::uint32_t requested=(product+15u)&~std::uint32_t{15};
    auto allocation=allocate(requested,f);
    if(allocation&&requested) std::memset(allocation->data(),0,requested);
    return allocation;
}
bool NativeRetailRendererAllocatorService::release(memory::NativeAllocation& allocation,
    RetailRendererAllocatorFailure& f) noexcept {
    f={};
    if(!allocation&&allocation.requested_owner()==memory::NativeHeapIdentity{}) return true;
    if(!allocation.release(f.heap)) {
        f.kind=RetailRendererAllocatorError::stale_allocation;
        f.operation="2209A0 native allocation lifetime";return false;
    }
    return true;
}
bool NativeRetailRendererAllocatorService::reallocate_in_place(memory::NativeAllocation& old,
    std::size_t bytes,RetailRendererAllocatorFailure& f) noexcept {
    f={};
    if(!old&&old.requested_owner()!=memory::NativeHeapIdentity{}) {
        f.kind=RetailRendererAllocatorError::stale_allocation;
        f.operation="220950 stale native old allocation";return false;
    }
    if(old&&bytes==0) return release(old,f);
    auto replacement=allocate(bytes,f);
    if(!replacement) return false;
    if(old) {
        std::memcpy(replacement->data(),old.data(),std::min(bytes,old.rounded_size()));
        if(!release(old,f)) return false;
    }
    old=std::move(*replacement);return true;
}
}
