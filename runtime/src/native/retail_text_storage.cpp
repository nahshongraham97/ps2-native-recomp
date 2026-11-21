#include "mka/retail_text_storage.hpp"
#include <bit>
#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailTextStorageFailure& f,RetailTextStorageError kind,const char* operation) noexcept {
    f.kind=kind; f.operation=operation; return false;
}
}
NativeRetailTextStorage::NativeRetailTextStorage(
    std::shared_ptr<memory::NativeWaveAllocator> wave,
    std::shared_ptr<NativeRetailGamePool> pool) noexcept
    : wave_(std::move(wave)),pool_(std::move(pool)) {}
bool NativeRetailTextStorage::access(RetailTextStorageFailure& f) const noexcept {
    return std::this_thread::get_id()==owner_thread_ ||
        fail(f,RetailTextStorageError::wrong_thread,"native text storage owner thread");
}
std::shared_ptr<NativeRetailTextStorage> NativeRetailTextStorage::create(
    std::shared_ptr<memory::NativeWaveAllocator> wave,
    std::shared_ptr<NativeRetailGamePool> pool,RetailTextStorageFailure& f) noexcept {
    f={};
    if(!wave || !pool) {
        fail(f,RetailTextStorageError::missing_owner,"16FD80 actual Wave and shared game pool");
        return {};
    }
    if(!pool->observe(f.pool)) {
        fail(f,RetailTextStorageError::pool,"16FD80 actual pool owner admission"); return {};
    }
    try { return std::shared_ptr<NativeRetailTextStorage>(new NativeRetailTextStorage(std::move(wave),std::move(pool))); }
    catch(const std::bad_alloc&) { fail(f,RetailTextStorageError::allocation,"16FD80 native text allocation service"); }
    catch(...) { fail(f,RetailTextStorageError::native_exception,"16FD80 native text allocation service"); }
    return {};
}
bool NativeRetailTextStorage::same_bindings(
    const std::shared_ptr<memory::NativeWaveAllocator>& wave,
    const std::shared_ptr<NativeRetailGamePool>& pool,RetailTextStorageFailure& f) const noexcept {
    f={};
    if(!access(f)) return false;
    return (wave==wave_ && pool==pool_) ||
        fail(f,RetailTextStorageError::foreign_owner,"16FD80 cannot replace retained Wave/pool ownership");
}
std::optional<memory::NativeAllocation> NativeRetailTextStorage::allocate_text(
    std::size_t bytes,RetailTextStorageFailure& f) noexcept {
    f={}; if(!access(f)) return {};
    auto allocation=wave_->allocate(bytes,f.wave);
    if(!allocation) fail(f,RetailTextStorageError::wave,"12DA40 native Wave allocation");
    return allocation;
}
std::optional<RetailStorageReleaseRoute> NativeRetailTextStorage::retire_source_storage(
    memory::NativeAllocation& allocation,std::uint32_t phase,std::uint32_t count,
    RetailTextStorageFailure& f) noexcept {
    f={}; if(!access(f)) return {};
    if(!allocation) {
        fail(f,RetailTextStorageError::stale_allocation,"12D9C0 allocation absent or reset"); return {};
    }
    if(phase!=0) {
        if(!allocation.release(f.heap)) {
            fail(f,RetailTextStorageError::heap,"12D9C0 immediate native release"); return {};
        }
        ++counters_.immediate_releases;
        return RetailStorageReleaseRoute::immediate_phase;
    }
    // Stage host metadata before consuming the caller's allocation. A source
    // 12FF70 record-allocation failure has an explicit immediate-free branch.
    try {
        std::list<Entry> staged;
        staged.emplace_front();
        auto link=pool_->acquire_observer_link(count,f.pool);
        if(!link) {
            if(f.pool.kind!=RetailGamePoolError::allocation && f.pool.kind!=RetailGamePoolError::host_capacity) {
                fail(f,RetailTextStorageError::pool,"12D9C0 actual shared observer record"); return {};
            }
        } else {
            staged.front().pool_link=std::move(*link);
            staged.front().remaining=count;
            staged.front().allocation=std::move(allocation);
            pending_.splice(pending_.begin(),staged);
            return RetailStorageReleaseRoute::deferred;
        }
    } catch(const std::bad_alloc&) {
        // Same source missing-record branch; no interpreter/device alternative.
    } catch(...) {
        fail(f,RetailTextStorageError::native_exception,"12D9C0 native deferred metadata"); return {};
    }
    if(!allocation.release(f.heap)) {
        fail(f,RetailTextStorageError::heap,"12D9C0 missing-record immediate native release"); return {};
    }
    ++counters_.record_exhaustion_releases;
    return RetailStorageReleaseRoute::immediate_record_exhaustion;
}
bool NativeRetailTextStorage::advance_frame_from_12d930(RetailTextStorageFailure& f) noexcept {
    f={}; if(!access(f)) return false;
    for(auto it=pending_.begin();it!=pending_.end();) {
        // Original ADDIU sign-extends the wrapped32-bit subtraction; BGTZ is
        // signed. Do not substitute Ghidra's unsigned-looking count==1 form.
        const auto decremented=it->remaining-std::uint32_t{1};
        if(std::bit_cast<std::int32_t>(decremented)>0) {
            it->remaining=decremented; ++it; continue;
        }
        if(!it->allocation.release(f.heap))
            return fail(f,RetailTextStorageError::heap,"12D930 deferred allocation already invalidated");
        it->remaining=1;
        if(!it->pool_link.release(f.pool))
            return fail(f,RetailTextStorageError::pool,"12D930 shared record retirement");
        ++counters_.deferred_releases;
        it=pending_.erase(it);
    }
    // The exact2E1730 tail is JR RA / NOP, not an omitted renderer call.
    return true;
}
std::optional<RetailTextStorageObservation> NativeRetailTextStorage::observe(
    RetailTextStorageFailure& f) const noexcept {
    f={}; if(!access(f)) return {};
    auto result=counters_; result.pending=pending_.size(); return result;
}
} // namespace mka::retail
