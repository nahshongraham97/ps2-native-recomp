#include "mka/retail_frame_work_storage.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <new>
#include <thread>
#include <vector>

namespace mka::retail {
namespace {
constexpr std::size_t initial_block_bytes=0x249f0;
constexpr std::array<std::size_t,2> initial_blocks{{3,2}};
bool fail(RetailFrameWorkFailure& f,RetailFrameWorkError kind,const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
}
namespace detail {
struct RetailFrameWorkBlock {
    memory::NativeAllocation allocation;
    explicit RetailFrameWorkBlock(memory::NativeAllocation&& a) noexcept:allocation(std::move(a)){}
};
struct RetailFrameWorkRequest;
struct RetailFrameWorkContext {
    std::weak_ptr<NativeRetailMainLifetime> lifetime;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
    const std::thread::id owner{std::this_thread::get_id()};
    struct Category {
        std::vector<std::shared_ptr<RetailFrameWorkBlock>> pool;
        std::weak_ptr<RetailFrameWorkRequest> pending;
    };
    std::array<Category,2> categories;
    std::atomic<bool> retired{false};
    bool access(RetailFrameWorkFailure& f) const noexcept {
        f={};
        if(std::this_thread::get_id()!=owner)return fail(f,RetailFrameWorkError::wrong_thread,"native frame work owner thread");
        const auto main=lifetime.lock();
        if(retired.load(std::memory_order_acquire)||!main||!main->active_on_owner_thread())
            return fail(f,RetailFrameWorkError::retired_main,"native frame work Main retired");
        return true;
    }
};
struct RetailFrameWorkRequest {
    std::weak_ptr<RetailFrameWorkContext> context;
    std::shared_ptr<RetailFrameWorkBlock> block;
    std::size_t category{},requested{},aligned{};
    bool resolved{};
};
}
using detail::RetailFrameWorkBlock;
using detail::RetailFrameWorkContext;
using detail::RetailFrameWorkRequest;
NativeRetailFrameWorkData::NativeRetailFrameWorkData(
    std::shared_ptr<RetailFrameWorkBlock> block,std::size_t size,std::size_t aligned) noexcept
    :block_(std::move(block)),size_(size),aligned_(aligned){}
std::optional<std::span<const std::byte>> NativeRetailFrameWorkData::bytes(RetailFrameWorkFailure& f) const & noexcept {
    f={};
    const auto data=block_?static_cast<const std::byte*>(block_->allocation.data()):nullptr;
    if(!data) {
        fail(f,RetailFrameWorkError::stale_allocation,"native committed frame backing revoked");return {};
    }
    return std::span<const std::byte>(data,size_);
}
NativeRetailFrameWorkReservation::NativeRetailFrameWorkReservation(
    std::shared_ptr<RetailFrameWorkRequest> request) noexcept:request_(std::move(request)){}
std::size_t NativeRetailFrameWorkReservation::size() const noexcept{return request_?request_->requested:0;}
std::optional<std::span<std::byte>> NativeRetailFrameWorkReservation::writable_bytes(RetailFrameWorkFailure& f) & noexcept {
    f={};
    if(!request_) {fail(f,RetailFrameWorkError::stale_reservation,"moved native frame reservation");return {};}
    const auto context=request_->context.lock();
    if(!context){fail(f,RetailFrameWorkError::retired_main,"native frame reservation owner retired");return {};}
    if(!context->access(f))return {};
    if(request_->resolved){fail(f,RetailFrameWorkError::stale_reservation,"native frame reservation already consumed");return {};}
    if(!request_->block->allocation){fail(f,RetailFrameWorkError::stale_allocation,"native frame reservation backing revoked");return {};}
    return std::span<std::byte>(static_cast<std::byte*>(request_->block->allocation.data()),request_->requested);
}
NativeRetailFrameWorkStorage::NativeRetailFrameWorkStorage(
    std::shared_ptr<RetailFrameWorkContext> context) noexcept:context_(std::move(context)){}
NativeRetailFrameWorkStorage::~NativeRetailFrameWorkStorage(){
    // Main admission ends, but committed native consumer leases keep their
    // actual backing. No destruction on an arbitrary GPU callback is required.
    context_->retired.store(true,std::memory_order_release);
}
std::unique_ptr<NativeRetailFrameWorkStorage> NativeRetailFrameWorkStorage::create(
    std::shared_ptr<NativeRetailMainLifetime> main,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator,RetailFrameWorkFailure& f) noexcept {
    f={};
    if(!main||!main->active_on_owner_thread()||!allocator) {
        fail(f,RetailFrameWorkError::missing_owner,"2E9930 actual Main/Permanent storage owners");return {};
    }
    try {
        auto context=std::make_shared<RetailFrameWorkContext>();
        context->lifetime=main;context->allocator=std::move(allocator);
        for(std::size_t category=0;category<initial_blocks.size();++category) {
            auto& pool=context->categories[category].pool;pool.reserve(initial_blocks[category]);
            for(std::size_t i=0;i<initial_blocks[category];++i) {
                auto storage=context->allocator->allocate(initial_block_bytes,f.allocator);
                if(!storage){fail(f,RetailFrameWorkError::allocator,"2E9930 real native initial work backing");return {};}
                std::memset(storage->data(),0,storage->requested_size());
                pool.push_back(std::make_shared<RetailFrameWorkBlock>(std::move(*storage)));
            }
        }
        return std::unique_ptr<NativeRetailFrameWorkStorage>(new NativeRetailFrameWorkStorage(std::move(context)));
    }catch(const std::bad_alloc&){fail(f,RetailFrameWorkError::allocation,"native frame work ownership");}
    catch(...){fail(f,RetailFrameWorkError::native_exception,"native frame work construction");}
    return {};
}
bool NativeRetailFrameWorkStorage::same_heap(
    const NativeRetailRendererAllocatorService& allocator,RetailFrameWorkFailure& f) const noexcept {
    if(!context_->access(f))return false;
    return allocator.owner_identity()==context_->allocator->owner_identity()||
        fail(f,RetailFrameWorkError::missing_owner,"2E9930 cannot replace actual Permanent heap");
}
bool NativeRetailFrameWorkStorage::reset_for_initialization(RetailFrameWorkFailure& f) noexcept {
    if(!context_->access(f))return false;
    for(auto& category:context_->categories) {
        if(const auto pending=category.pending.lock())pending->resolved=true;
        category.pending.reset();
    }
    return true;
}
std::optional<NativeRetailFrameWorkReservation> NativeRetailFrameWorkStorage::reserve(
    std::size_t category,std::size_t bytes,RetailFrameWorkFailure& f) noexcept {
    if(!context_->access(f))return {};
    if(category>=context_->categories.size()){fail(f,RetailFrameWorkError::invalid_category,"native frame work category");return {};}
    auto& stream=context_->categories[category];
    if(const auto pending=stream.pending.lock();pending&&!pending->resolved) {
        fail(f,RetailFrameWorkError::pending,"2E8FE0 one active reservation in this work category");return {};
    }
    if(bytes>std::numeric_limits<std::size_t>::max()-15) {
        fail(f,RetailFrameWorkError::host_capacity,"native frame work alignment overflow");return {};
    }
    const auto aligned=(bytes+15)&~std::size_t(15);
    try {
        auto request=std::make_shared<RetailFrameWorkRequest>();
        std::shared_ptr<RetailFrameWorkBlock> block;
        for(const auto& candidate:stream.pool) {
            if(candidate.use_count()==1&&candidate->allocation&&candidate->allocation.requested_size()>=aligned) {
                block=candidate;break;
            }
        }
        if(!block) {
            // Original capacities/window counts are initial warm storage only,
            // never host limits. Live consumer leases cannot be overwritten.
            auto storage=context_->allocator->allocate(std::max(initial_block_bytes,aligned),f.allocator);
            if(!storage){fail(f,RetailFrameWorkError::allocator,"native frame work growth");return {};}
            std::memset(storage->data(),0,storage->requested_size());
            block=std::make_shared<RetailFrameWorkBlock>(std::move(*storage));
            stream.pool.push_back(block);
        }
        request->context=context_;request->block=std::move(block);
        request->category=category;request->requested=bytes;request->aligned=aligned;
        stream.pending=request;
        return NativeRetailFrameWorkReservation(std::move(request));
    }catch(const std::bad_alloc&){fail(f,RetailFrameWorkError::allocation,"native frame reservation metadata");}
    catch(...){fail(f,RetailFrameWorkError::native_exception,"native frame reservation");}
    return {};
}
std::optional<NativeRetailFrameWorkData> NativeRetailFrameWorkStorage::commit(
    NativeRetailFrameWorkReservation& reservation,std::size_t written,RetailFrameWorkFailure& f) noexcept {
    if(!context_->access(f))return {};
    const auto request=reservation.request_;
    if(!request||request->context.lock()!=context_) {
        fail(f,RetailFrameWorkError::foreign_reservation,"native frame commit actual owner");return {};
    }
    if(request->resolved||context_->categories[request->category].pending.lock()!=request) {
        fail(f,RetailFrameWorkError::stale_reservation,"native frame commit consumed reservation");return {};
    }
    //2E8F80 consumes the pending flag BEFORE checking the actual byte count.
    request->resolved=true;
    context_->categories[request->category].pending.reset();
    if(written>request->requested) {
        fail(f,RetailFrameWorkError::invalid_commit,"2E8F80 commit exceeds admitted reservation");return {};
    }
    if(!request->block->allocation) {
        fail(f,RetailFrameWorkError::stale_allocation,"native frame commit backing revoked");return {};
    }
    return NativeRetailFrameWorkData(request->block,written,(written+15)&~std::size_t(15));
}
std::optional<RetailFrameWorkObservation> NativeRetailFrameWorkStorage::observe(RetailFrameWorkFailure& f) const noexcept {
    if(!context_->access(f))return {};
    RetailFrameWorkObservation result;result.heap=context_->allocator->owner_identity();
    for(std::size_t i=0;i<result.categories.size();++i) {
        const auto& category=context_->categories[i];auto& observed=result.categories[i];
        observed.native_blocks=category.pool.size();
        for(const auto& block:category.pool)if(block.use_count()>1)++observed.retained_blocks;
        const auto pending=category.pending.lock();observed.pending=pending&&!pending->resolved;
    }
    return result;
}
} // namespace mka::retail
