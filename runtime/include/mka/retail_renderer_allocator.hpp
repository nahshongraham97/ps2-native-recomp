#pragma once
#include "mka/retail_memory_owners.hpp"
#include <memory>
#include <optional>
namespace mka::retail {
enum class RetailRendererAllocatorError {none,permanent_owner,allocation,heap,stale_allocation,native_exception};
struct RetailRendererAllocatorFailure {
    RetailRendererAllocatorError kind{RetailRendererAllocatorError::none};
    const char* operation{""};
    memory::NativeHeapFailure heap;
};
//220900's native allocation service. Retains the existing Permanent heap,
//never a raw callback address,arena,guest pointer map or renderer-pool ceiling.
class NativeRetailRendererAllocatorService final {
public:
    NativeRetailRendererAllocatorService(const NativeRetailRendererAllocatorService&)=delete;
    NativeRetailRendererAllocatorService& operator=(const NativeRetailRendererAllocatorService&)=delete;
    NativeRetailRendererAllocatorService(NativeRetailRendererAllocatorService&&)=delete;
    NativeRetailRendererAllocatorService& operator=(NativeRetailRendererAllocatorService&&)=delete;
    static std::shared_ptr<NativeRetailRendererAllocatorService> create(
        NativeRetailMemoryOwners&,RetailRendererAllocatorFailure&) noexcept;
    memory::NativeHeapIdentity owner_identity() const noexcept {return identity_;}
    std::optional<memory::NativeAllocation> allocate(std::size_t,RetailRendererAllocatorFailure&) noexcept;
    // Source220970 compatibility arguments are32-bit game words. Their product
    // and rounding wrap as originally;this is not a native heap capacity limit.
    std::optional<memory::NativeAllocation> allocate_zeroed_source_words(
        std::uint32_t count,std::uint32_t bytes,RetailRendererAllocatorFailure&) noexcept;
    // Empty allocation is sourceNULL. Size0 frees a nonempty old allocation;
    // allocation failure preserves old. Migration uses the real native deleter.
    bool reallocate_in_place(memory::NativeAllocation&,std::size_t,RetailRendererAllocatorFailure&) noexcept;
    bool release(memory::NativeAllocation&,RetailRendererAllocatorFailure&) noexcept;
private:
    explicit NativeRetailRendererAllocatorService(std::shared_ptr<memory::NativeHeapOwner> heap,memory::NativeHeapIdentity identity) noexcept:heap_(std::move(heap)),identity_(identity){}
    std::shared_ptr<memory::NativeHeapOwner> heap_;
    memory::NativeHeapIdentity identity_;
};
}
