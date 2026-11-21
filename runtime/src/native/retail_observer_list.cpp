#include "mka/retail_observer_list.hpp"
#include "retail_main_control.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace mka::retail {
namespace detail {
struct RetailObserverLinkState {
    const NativeRetailObserverList* owner{};
    std::shared_ptr<NativeRetailObservedObject> object;
    std::optional<NativeRetailGamePoolLink> lease;
    std::shared_ptr<RetailObserverLinkState> next;
    std::weak_ptr<RetailObserverLinkState> previous;
    std::uint32_t cached_cookie{};
    std::uint8_t flags{1}; // 12FF70 sets bit 0 at observer-link birth.
    bool linked{}, retired{}, retirement_active{}, transfer_pending{};
};
}
namespace {
bool fail(RetailObserverListFailure& f,RetailObserverListError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

NativeRetailObserverList::NativeRetailObserverList(
    std::shared_ptr<NativeRetailGamePool> pool) noexcept
    : pool_(std::move(pool)) {}

bool NativeRetailObserverList::access(RetailObserverListFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailObserverListError::wrong_thread,
                    "12F4E0 observer owner thread");
    if(after_effect_unknown_)
        return fail(f,RetailObserverListError::after_effect_unknown,
                    "previous observer callback failed after effects");
    if(!pool_)
        return fail(f,RetailObserverListError::pool,
                    "12F4E0 actual shared game pool missing");
    return true;
}

std::shared_ptr<NativeRetailObserverLink>
NativeRetailObserverList::insert_head_from_12f870(
    std::shared_ptr<NativeRetailObservedObject> object,
    RetailObserverListFailure& f) {
    if(!access(f)) return {};
    if(!object) {
        fail(f,RetailObserverListError::missing_object,
             "12FF70 real observed object missing");
        return {};
    }
    try {
        auto node=std::make_shared<detail::RetailObserverLinkState>();
        auto handle=std::shared_ptr<NativeRetailObserverLink>(
            new NativeRetailObserverLink(node));
        auto lease=pool_->acquire_observer_link(object->source_cookie(),f.pool);
        if(!lease) {
            fail(f,RetailObserverListError::pool,
                 "12FF70 shared-pool observer link allocation");
            return {};
        }
        node->owner=this;
        node->object=std::move(object);
        node->cached_cookie=node->object->source_cookie();
        node->lease=std::move(*lease);
        node->next=head_;
        node->linked=true;
        if(head_) head_->previous=node;
        head_=std::move(node);
        f={};
        return handle;
    } catch(const std::bad_alloc&) {
        fail(f,RetailObserverListError::allocation,
             "12FF70 native observer link allocation");
    } catch(...) {
        fail(f,RetailObserverListError::native_exception,
             "12FF70 native observer link construction");
    }
    return {};
}

bool NativeRetailObserverList::retire(
    const std::shared_ptr<detail::RetailObserverLinkState>& node,
    RetailObserverListFailure& f) {
    if(!node || node->owner!=this)
        return fail(f,RetailObserverListError::foreign_link,
                    "12FA00 foreign observer link");
    if(node->retired)
        return fail(f,RetailObserverListError::already_retired,
                    "12FA00 already retired observer link");
    if(node->transfer_pending)
        return fail(f,RetailObserverListError::pending_transfer,
                    "12FA00 cleanup transferred after unlink");
    if(node->retirement_active)
        return fail(f,RetailObserverListError::reentrant_retirement,
                    "12FA00 recursive observer retirement");
    if(node->flags&2u) return true; // Source bit-1 callback guard retains link.
    try {
        pending_.push_back(node); // Secure lifetime before source unlink.
    } catch(const std::bad_alloc&) {
        return fail(f,RetailObserverListError::allocation,
                    "12FA00 pending native link retention");
    }
    node->retirement_active=true;
    if(node->linked) {
        auto previous=node->previous.lock();
        auto next=node->next;
        if(previous) previous->next=next;
        else head_=next;
        if(next) next->previous=previous;
        node->previous.reset();
        node->next.reset();
        node->linked=false;
    }
    // 12FA00 calls object virtual +0xC only when bit 0 is clear and the
    // cached and live nonzero identities still match. It does so after unlink.
    if(node->object && !(node->flags&1u) && node->cached_cookie!=0 &&
       node->cached_cookie==node->object->source_cookie()) {
        try {
            node->object->cleanup_from_12fa00();
        } catch(const detail::NativeMainReentry&) {
            node->transfer_pending=true;
            node->retirement_active=false;
            throw;
        } catch(const std::bad_alloc&) {
            node->retirement_active=false;
            after_effect_unknown_=true;
            return fail(f,RetailObserverListError::allocation,
                        "12FA00 observed-object cleanup after unlink");
        } catch(...) {
            node->retirement_active=false;
            after_effect_unknown_=true;
            return fail(f,RetailObserverListError::native_exception,
                        "12FA00 observed-object cleanup after unlink");
        }
    }
    node->cached_cookie=1;
    if(node->lease && !node->lease->release(f.pool)) {
        node->retirement_active=false;
        after_effect_unknown_=true;
        return fail(f,RetailObserverListError::pool,
                    "12FA00 shared-pool observer recycle after unlink");
    }
    node->lease.reset();
    node->object.reset();
    node->retired=true;
    node->retirement_active=false;
    ++retired_;
    const auto pending=std::find(pending_.begin(),pending_.end(),node);
    if(pending!=pending_.end()) pending_.erase(pending);
    f={};
    return true;
}

bool NativeRetailObserverList::retire_link_from_12fa00(
    const std::shared_ptr<NativeRetailObserverLink>& handle,
    RetailObserverListFailure& f) {
    if(!access(f)) return false;
    if(!handle)
        return fail(f,RetailObserverListError::foreign_link,
                    "12FA00 missing observer link handle");
    return retire(handle->state_,f);
}

bool NativeRetailObserverList::visit_from_12f4e0(
    NativeRetailObserverCallback& callback,
    RetailObserverListFailure& f) {
    if(!access(f)) return false;
    auto node=head_;
    while(node) {
        const auto object=node->object;
        if(object && node->cached_cookie==object->source_cookie()) {
            const auto previous_bit=node->flags&2u;
            node->flags=static_cast<std::uint8_t>((node->flags&~2u)|2u);
            try {
                callback.invoke(object);
            } catch(const detail::NativeMainReentry&) {
                // The original nonlocal transfer skips the restore store.
                throw;
            } catch(const std::bad_alloc&) {
                after_effect_unknown_=true;
                return fail(f,RetailObserverListError::allocation,
                            "12F4E0 callback after link flag publication");
            } catch(...) {
                after_effect_unknown_=true;
                return fail(f,RetailObserverListError::native_exception,
                            "12F4E0 callback after link flag publication");
            }
            node->flags=static_cast<std::uint8_t>((node->flags&~2u)|previous_bit);
            node=node->next; // Source reloads next after callback effects.
        } else {
            auto next=node->next; // Stale route saves next before retirement.
            node->cached_cookie=1;
            if(!(node->flags&2u) && !retire(node,f)) return false;
            node=std::move(next);
        }
    }
    f={};
    return true;
}

bool NativeRetailObserverList::visit_from_13a7a0(
    NativeRetailObserverCallback& callback,
    RetailObserverListFailure& f) {
    if(!access(f)) return false;
    auto node=head_;
    while(node) {
        const auto object=node->object;
        if(object && node->cached_cookie==object->source_cookie()) {
            try {
                callback.invoke(object);
            } catch(const detail::NativeMainReentry&) {
                throw;
            } catch(const std::bad_alloc&) {
                after_effect_unknown_=true;
                return fail(f,RetailObserverListError::allocation,
                            "13A7A0 actor callback after effect");
            } catch(...) {
                after_effect_unknown_=true;
                return fail(f,RetailObserverListError::native_exception,
                            "13A7A0 actor callback after effect");
            }
            node=node->next; // Source reads next after the live actor effect.
        } else {
            auto next=node->next;
            node->cached_cookie=1;
            if(!(node->flags&2u) && !retire(node,f)) return false;
            node=std::move(next);
        }
    }
    f={};
    return true;
}

bool NativeRetailObserverList::drain_from_130ac0(
    RetailObserverListFailure& f) {
    if(!access(f)) return false;
    while(head_) {
        bool saw_live=false;
        bool made_progress=false;
        auto node=head_;
        while(node) {
            const auto object=node->object;
            if(object && node->cached_cookie==object->source_cookie()) {
                saw_live=true;
                if(node->flags&1u) {
                    if(node->cached_cookie!=1) made_progress=true;
                    node->cached_cookie=1;
                } else {
                    const auto before_cookie=object->source_cookie();
                    const auto previous_bit=node->flags&2u;
                    node->flags=static_cast<std::uint8_t>((node->flags&~2u)|2u);
                    try {
                        if(before_cookie!=0) object->cleanup_from_12fa00();
                    } catch(const detail::NativeMainReentry&) {
                        throw;
                    } catch(const std::bad_alloc&) {
                        after_effect_unknown_=true;
                        return fail(f,RetailObserverListError::allocation,
                                    "130AC0 object cleanup after flag publication");
                    } catch(...) {
                        after_effect_unknown_=true;
                        return fail(f,RetailObserverListError::native_exception,
                                    "130AC0 object cleanup after flag publication");
                    }
                    node->flags=static_cast<std::uint8_t>(
                        (node->flags&~2u)|previous_bit);
                    if(object->source_cookie()!=before_cookie || !node->linked)
                        made_progress=true;
                }
                node=node->next; // Source reloads next after live cleanup.
            } else {
                auto next=node->next;
                node->cached_cookie=1;
                if(!(node->flags&2u)) {
                    if(!retire(node,f)) return false;
                    made_progress=true;
                }
                node=std::move(next);
            }
        }
        if(!saw_live) {
            if(head_) {
                after_effect_unknown_=true;
                return fail(f,RetailObserverListError::active_links,
                            "130AC0 retained guarded stale links");
            }
            break;
        }
        if(!made_progress) {
            after_effect_unknown_=true;
            return fail(f,RetailObserverListError::cleanup_no_progress,
                        "130AC0 live cleanup retained unchanged cookie/link");
        }
    }
    f={};
    return true;
}

std::shared_ptr<NativeRetailGamePool>
NativeRetailObserverList::retained_pool(
    RetailObserverListFailure& f) const noexcept {
    if(!access(f)) return {};
    return pool_;
}

bool NativeRetailObserverList::reset_empty_head_from_source(
    RetailObserverListFailure& f) noexcept {
    if(!access(f)) return false;
    if(head_ || !pending_.empty())
        return fail(f,RetailObserverListError::active_links,
                    "source callback-list head reset with retained links");
    head_.reset();
    return true;
}

std::optional<RetailObserverListObservation> NativeRetailObserverList::observe(
    RetailObserverListFailure& f) const noexcept {
    if(!access(f)) return {};
    std::size_t linked=0;
    for(auto node=head_;node;node=node->next) ++linked;
    return RetailObserverListObservation{
        linked,pending_.size(),retired_,after_effect_unknown_};
}
} // namespace mka::retail
