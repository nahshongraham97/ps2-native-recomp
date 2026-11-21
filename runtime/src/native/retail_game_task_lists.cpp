#include "mka/retail_game_task_lists.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailGameTaskListFailure& f,RetailGameTaskListError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
constexpr RetailGameTaskListRoot head_228b90[]{
    RetailGameTaskListRoot::e1a8};
constexpr RetailGameTaskListRoot first_heads_13c180[]{
    RetailGameTaskListRoot::e024,RetailGameTaskListRoot::e020};
constexpr RetailGameTaskListRoot later_heads_13c180[]{
    RetailGameTaskListRoot::e7ec,RetailGameTaskListRoot::e7e8};
constexpr RetailGameTaskListRoot heads_139a30[]{
    RetailGameTaskListRoot::dfe4,RetailGameTaskListRoot::e02c,
    RetailGameTaskListRoot::dfe0};
constexpr RetailGameTaskListRoot head_16c1b0[]{
    RetailGameTaskListRoot::e1ac};
}

NativeRetailGameTaskLists::NativeRetailGameTaskLists(
    std::shared_ptr<NativeRetailGamePool> pool) noexcept
    : pool_(std::move(pool)) {}

std::shared_ptr<NativeRetailGameTaskLists>
NativeRetailGameTaskLists::create(
    std::shared_ptr<NativeRetailGamePool> pool,
    RetailGameTaskListFailure& f) noexcept {
    f={};
    if(!pool) {
        fail(f,RetailGameTaskListError::missing_pool,
             "130350 same shared 12DA70 pool missing");
        return {};
    }
    try {
        auto result=std::shared_ptr<NativeRetailGameTaskLists>(
            new NativeRetailGameTaskLists(pool));
        for(auto& root:result->roots_)
            root=std::make_shared<NativeRetailObserverList>(pool);
        return result;
    } catch(const std::bad_alloc&) {
        fail(f,RetailGameTaskListError::allocation,
             "130350 nine retained source callback heads");
    } catch(...) {
        fail(f,RetailGameTaskListError::native_exception,
             "130350 native callback-head construction");
    }
    return {};
}

bool NativeRetailGameTaskLists::access(
    RetailGameTaskListFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailGameTaskListError::wrong_thread,
                    "130350 callback-head owner thread");
    if(!pool_)
        return fail(f,RetailGameTaskListError::missing_pool,
                    "130350 callback-head shared pool missing");
    return true;
}

bool NativeRetailGameTaskLists::same_pool(
    const std::shared_ptr<NativeRetailGamePool>& pool,
    RetailGameTaskListFailure& f) const noexcept {
    if(!access(f)) return false;
    if(pool_!=pool)
        return fail(f,RetailGameTaskListError::foreign_pool,
                    "130350 callback heads attached to another pool");
    return true;
}

std::shared_ptr<NativeRetailObserverList>
NativeRetailGameTaskLists::root(
    RetailGameTaskListRoot root_id,RetailGameTaskListFailure& f) const noexcept {
    if(!access(f)) return {};
    const auto index=static_cast<std::size_t>(root_id);
    if(index>=roots_.size()) {
        fail(f,RetailGameTaskListError::invalid_root,
             "130350 unknown source callback head");
        return {};
    }
    return roots_[index];
}

bool NativeRetailGameTaskLists::clear(
    const RetailGameTaskListRoot* heads,std::size_t count,
    RetailGameTaskListFailure& f) noexcept {
    if(!access(f)) return false;
    // Preflight all native link lifetimes before the original first store.
    // The preceding source cleanup and 12DA70 pool reset must have released
    // every link; never drop an active owned callback by replacing a root.
    for(std::size_t i=0;i<count;++i) {
        const auto index=static_cast<std::size_t>(heads[i]);
        if(index>=roots_.size())
            return fail(f,RetailGameTaskListError::invalid_root,
                        "130350 callback-head clear index");
        const auto observed=roots_[index]->observe(f.list);
        if(!observed || observed->linked || observed->pending)
            return fail(f,RetailGameTaskListError::observer_list,
                        "130350 callback-head clear preflight");
    }
    for(std::size_t i=0;i<count;++i) {
        if(!roots_[static_cast<std::size_t>(heads[i])]
                ->reset_empty_head_from_source(f.list))
            return fail(f,RetailGameTaskListError::observer_list,
                        "130350 source-ordered callback-head clear");
    }
    return true;
}

bool NativeRetailGameTaskLists::clear_from_228b90(
    RetailGameTaskListFailure& f) noexcept {
    return clear(head_228b90,1,f);
}
bool NativeRetailGameTaskLists::clear_first_pair_from_13c180(
    RetailGameTaskListFailure& f) noexcept {
    return clear(first_heads_13c180,2,f);
}
bool NativeRetailGameTaskLists::clear_later_pair_from_13c180(
    RetailGameTaskListFailure& f) noexcept {
    return clear(later_heads_13c180,2,f);
}
bool NativeRetailGameTaskLists::clear_from_139a30(
    RetailGameTaskListFailure& f) noexcept {
    return clear(heads_139a30,3,f);
}
bool NativeRetailGameTaskLists::clear_from_16c1b0(
    RetailGameTaskListFailure& f) noexcept {
    return clear(head_16c1b0,1,f);
}
} // namespace mka::retail
