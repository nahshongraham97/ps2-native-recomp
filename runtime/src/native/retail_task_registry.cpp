#include "mka/retail_task_registry.hpp"
#include "retail_main_control.hpp"

#include <bit>
#include <iterator>
#include <new>

namespace mka::retail {
namespace {
bool fail(RetailTaskRegistryFailure& f, RetailTaskRegistryError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

std::unique_ptr<NativeRetailTaskRegistry> NativeRetailTaskRegistry::create(
    RetailTaskRegistryFailure& f) noexcept {
    f={};
    try {
        return std::unique_ptr<NativeRetailTaskRegistry>(new NativeRetailTaskRegistry);
    } catch (const std::bad_alloc&) {
        fail(f,RetailTaskRegistryError::allocation,"retail task registry allocation");
    } catch (...) {
        fail(f,RetailTaskRegistryError::native_exception,"retail task registry creation");
    }
    return {};
}

std::unique_ptr<NativeRetailTaskRegistry> NativeRetailTaskRegistry::create(
    std::shared_ptr<NativeRetailGamePool> pool,
    RetailTaskRegistryFailure& f) noexcept {
    f={};
    if(!pool) {
        fail(f,RetailTaskRegistryError::game_pool,"retail task source pool absent");
        return {};
    }
    if(!pool->observe(f.pool)) {
        fail(f,RetailTaskRegistryError::game_pool,"retail task source pool unavailable");
        return {};
    }
    auto result=create(f);
    if(result) result->pool_=std::move(pool);
    return result;
}

bool NativeRetailTaskRegistry::release_pool_link(
    Link& link, RetailTaskRegistryFailure& f) noexcept {
    if(!link.pool_lease) return true;
    if(!link.pool_lease->release(f.pool)) {
        after_effect_unknown_=true;
        return fail(f,RetailTaskRegistryError::game_pool,
                    "12F8D0 native source link recycle");
    }
    link.pool_lease.reset();
    return true;
}

bool NativeRetailTaskRegistry::owner_thread(RetailTaskRegistryFailure& f) const noexcept {
    f={};
    return std::this_thread::get_id()==owner_thread_ ||
        fail(f,RetailTaskRegistryError::wrong_thread,"retail task registry thread");
}
bool NativeRetailTaskRegistry::access(RetailTaskRegistryFailure& f) const noexcept {
    if(!owner_thread(f)) return false;
    if(in_callout_)
        return fail(f,RetailTaskRegistryError::callout_active,"retail task callback mutation");
    if(after_effect_unknown_)
        return fail(f,RetailTaskRegistryError::after_effect_unknown,
                    "retail task cleanup had an unknown partial effect");
    return true;
}
std::list<NativeRetailTaskRegistry::Link>::iterator
NativeRetailTaskRegistry::find_active(
    const std::shared_ptr<NativeRetailTaskObject>& task) noexcept {
    for(auto it=active_.begin();it!=active_.end();++it)
        if(it->object==task) return it;
    return active_.end();
}
bool NativeRetailTaskRegistry::owns(
    const std::shared_ptr<NativeRetailTaskObject>& task) const noexcept {
    if(!task || task->retired_) return false;
    for(const auto& link:active_) if(link.object==task) return true;
    for(const auto& link:retained_) if(link.object==task) return true;
    for(const auto& link:pending_) if(link.object==task) return true;
    return false;
}

bool NativeRetailTaskRegistry::drain_task_observers_from_12ee70(
    NativeRetailTaskObject& task, RetailTaskRegistryFailure& f) {
    if(!pool_) return true; // CPU-only test registry has no retail link pool.
    if(!task.observers_e8_ || !task.observers_ec_)
        return fail(f,RetailTaskRegistryError::observer_list,
                    "12EE70 task nested observer roots absent");
    // Original 12EE70 calls 130AC0(task+E8) then 130AC0(task+EC).
    // These are two distinct source list identities, not one merged list.
    if(!task.observers_e8_->drain_from_130ac0(f.observer))
        return fail(f,RetailTaskRegistryError::observer_list,
                    "12EE70 first nested observer root cleanup");
    if(!task.observers_ec_->drain_from_130ac0(f.observer))
        return fail(f,RetailTaskRegistryError::observer_list,
                    "12EE70 second nested observer root cleanup");
    return true;
}

bool NativeRetailTaskRegistry::retire_active(
    std::list<Link>::iterator it, RetailTaskRegistryFailure& f) {
    // Source 12FA00 flag bit1 returns without unlink or recycle.
    if(it->flags&2u) return true;
    pending_.splice(pending_.end(),active_,it);
    auto pending=std::prev(pending_.end());
    auto task=pending->object;
    task->linked_=false;
    // The stale caller has already written cached cookie 1. A real object
    // cookie of 1 can therefore still reach virtual cleanup.
    const bool invoke=!(pending->flags&1u) &&
        pending->cached_cookie==task->cookie_ && task->cookie_!=0;
    if(!invoke) {
        if(task->retired_ && task->cookie_==0 && !task->body_) {
            // Direct virtual cleanup already released the native payload.
            // Source 12FA00 recycles this later stale link without replay.
            pending->object.reset();
            pending->cached_cookie=1;
            pending->flags=0;
            if(!release_pool_link(*pending,f)) return false;
            recycled_.splice(recycled_.begin(),pending_,pending);
            return true;
        }
        if(!release_pool_link(*pending,f)) return false;
        retained_.splice(retained_.end(),pending_,pending);
        return true;
    }
    const bool release_allowed=current_!=task && !task->retain_flag2_;
    auto body=task->body_; // Retain real nested owner across a transfer.
    if(release_allowed) {
        // Fixed 12EE70 stores object+4=0 before stack/nested release.
        task->cookie_=0;
        task->retired_=true;
    }
    // Unlink precedes the callback in 12FA00. An exception may escape after
    // any part of that callback; retain the object and refuse later mutation.
    after_effect_unknown_=in_callout_=true;
    try {
        body->cleanup(release_allowed);
        if(release_allowed &&
           !drain_task_observers_from_12ee70(*task,f)) {
            in_callout_=false;
            return false;
        }
    } catch(const detail::NativeMainReentry&) {
        // Original nonlocal main transfer resumes after this callback; the
        // source unlink has happened. Keep the detached object owned, do not
        // replay cleanup, and allow the next source operation to proceed.
        in_callout_=after_effect_unknown_=false;
        throw;
    } catch (...) {
        in_callout_=false;
        throw;
    }
    in_callout_=after_effect_unknown_=false;
    if(!release_allowed) {
        // 12EE70 returned under its object guard. The source link has still
        // been unlinked; retain the native object for a later source effect.
        if(!release_pool_link(*pending,f)) return false;
        retained_.splice(retained_.end(),pending_,pending);
        return true;
    }
    task->body_.reset();
    task->continuation_.reset();
    task->wait_resume_.reset();
    pending->object.reset();
    pending->cached_cookie=1;
    pending->flags=0;
    if(!release_pool_link(*pending,f)) return false;
    recycled_.splice(recycled_.begin(),pending_,pending);
    return true;
}

std::shared_ptr<NativeRetailTaskObject> NativeRetailTaskRegistry::publish_task(
    std::uint32_t kind, std::int32_t priority, std::uint32_t source_cookie,
    std::shared_ptr<NativeRetailTaskBody> body,
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return {};
    std::shared_ptr<NativeRetailObserverList> observers_e8,observers_ec;
    try {
        if(pool_) {
            observers_e8=std::make_shared<NativeRetailObserverList>(pool_);
            observers_ec=std::make_shared<NativeRetailObserverList>(pool_);
        }
    } catch(const std::bad_alloc&) {
        fail(f,RetailTaskRegistryError::allocation,
             "12EC50 per-task observer root allocation");
        return {};
    } catch(...) {
        fail(f,RetailTaskRegistryError::native_exception,
             "12EC50 per-task observer root construction");
        return {};
    }
    return publish_task(kind,priority,source_cookie,std::move(body),
                        std::move(observers_e8),std::move(observers_ec),f);
}

std::shared_ptr<NativeRetailTaskObject> NativeRetailTaskRegistry::publish_task(
    std::uint32_t kind, std::int32_t priority, std::uint32_t source_cookie,
    std::shared_ptr<NativeRetailTaskBody> body,
    std::shared_ptr<NativeRetailObserverList> observers_e8,
    std::shared_ptr<NativeRetailObserverList> observers_ec,
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return {};
    if(!body) {
        fail(f,RetailTaskRegistryError::invalid_body,
             "retail task publication needs executable body and cleanup");
        return {};
    }
    if(pool_) {
        if(!observers_e8 || !observers_ec ||
           observers_e8==observers_ec ||
           observers_e8->retained_pool(f.observer)!=pool_ ||
           observers_ec->retained_pool(f.observer)!=pool_) {
            fail(f,RetailTaskRegistryError::observer_list,
                 "12EC50 distinct task observer roots must retain same source pool");
            return {};
        }
    } else if(observers_e8 || observers_ec) {
        fail(f,RetailTaskRegistryError::observer_list,
             "CPU-only task registry cannot publish foreign observer root");
        return {};
    }
    std::shared_ptr<NativeRetailTaskObject> task;
    std::optional<NativeRetailGamePoolLink> pool_lease;
    std::list<Link> staged;
    try {
        task=std::shared_ptr<NativeRetailTaskObject>(
            new NativeRetailTaskObject(kind,priority,source_cookie,body));
        task->observers_e8_=std::move(observers_e8);
        task->observers_ec_=std::move(observers_ec);
        if(pool_) {
            pool_lease=pool_->acquire_task_link(source_cookie,f.pool);
            if(!pool_lease) {
                fail(f,RetailTaskRegistryError::game_pool,
                     "12FFE0 real source pool link allocation");
                return {};
            }
        }
        // 12FFE0 clears the recycled link flag byte at publication.
        staged.push_back(Link{task,source_cookie,0,std::move(pool_lease)});
    } catch(const std::bad_alloc&) {
        fail(f,RetailTaskRegistryError::allocation,"retail task object/link allocation");
        return {};
    } catch(...) {
        fail(f,RetailTaskRegistryError::native_exception,"retail task object/link creation");
        return {};
    }
    // Source 12F260 uses signed slt and retires stale predecessors during its
    // scan. All allocations above precede the first possible callback effect.
    auto position=active_.end();
    for(auto it=active_.begin();it!=active_.end();) {
        auto next=std::next(it);
        if(it->cached_cookie!=it->object->cookie_) {
            it->cached_cookie=1;
            if(!retire_active(it,f)) return {};
            it=next;
            continue;
        }
        if(priority<it->object->priority_) { position=it; break; }
        it=next;
    }
    active_.splice(position,staged,staged.begin());
    task->linked_=true;
    return task;
}

bool NativeRetailTaskRegistry::cancel_first_root_from_12f030(
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return false;
    // Source 12F030 clears these two scalars before walking the first root.
    selected_.reset();
    current_.reset();
    for(auto it=active_.begin();it!=active_.end();) {
        auto next=std::next(it);
        if(it->cached_cookie==it->object->cookie_)
            it->object->retain_flag2_=false;
        else {
            it->cached_cookie=1;
            if(!retire_active(it,f)) return false;
        }
        it=next;
    }
    // 130AC0 completes a pass over live links before restarting to recycle
    // links made stale by virtual cleanup. Recycling each link immediately
    // after its callback would change the source free-list/callback order.
    while(!active_.empty()) {
        bool visited_live=false;
        for(auto it=active_.begin();it!=active_.end();) {
            const auto next=std::next(it);
            const auto task=it->object;
            if(!task) {
                after_effect_unknown_=true;
                return fail(f,RetailTaskRegistryError::invalid_body,
                            "130AC0 first-root link missing object");
            }
            if(it->cached_cookie!=task->cookie_) {
                it->cached_cookie=1;
                if(!retire_active(it,f)) return false;
                it=next;
                continue;
            }
            visited_live=true;
            if(it->flags&1u) {
                // 130AC0 marks a bit-0 link stale without virtual cleanup.
                it->cached_cookie=1;
                it=next;
                continue;
            }
            if(task->cookie_==0 || !task->body_) {
                after_effect_unknown_=true;
                return fail(f,RetailTaskRegistryError::unsupported_source_branch,
                            "130AC0 live object without cleanup capability");
            }
            auto body=task->body_;
            const auto prior_bit1=it->flags&2u;
            it->flags|=2u; // Temporary source callback guard.
            // The real task vtable's 12EE70 invalidates +4 before nested
            // cleanup; retain its body until the call returns or transfers.
            task->cookie_=0;
            task->retired_=true;
            in_callout_=after_effect_unknown_=true;
            try {
                body->cleanup(true);
                if(!drain_task_observers_from_12ee70(*task,f)) {
                    in_callout_=false;
                    return false;
                }
            } catch(const detail::NativeMainReentry&) {
                in_callout_=after_effect_unknown_=false;
                throw;
            } catch(...) {
                in_callout_=false;
                throw;
            }
            in_callout_=after_effect_unknown_=false;
            it->flags=static_cast<std::uint8_t>((it->flags&~2u)|prior_bit1);
            task->body_.reset();
            task->continuation_.reset();
            task->wait_resume_.reset();
            it=next;
        }
        if(!visited_live) break;
    }
    return true;
}

bool NativeRetailTaskRegistry::dispatch_first_root_zero_flags_from_12e670(
    bool source_bit5_inhibited, bool source_root_wait_inhibited,
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return false;
    NativeRetailTaskExecution execution(*this);
    for(auto it=active_.begin();it!=active_.end();) {
        // Source 12E670 saves next before any callback or stale retirement.
        auto next=std::next(it);
        const bool has_next=next!=active_.end();
        const auto* saved_next=has_next ? std::addressof(*next) : nullptr;
        const auto saved_next_object=has_next ? next->object
                                              : std::shared_ptr<NativeRetailTaskObject>{};
        const auto advance_to_saved_next=[&]() {
            if(!has_next) return active_.end();
            // A body can retire or move the saved link. A list iterator from
            // another list cannot be used as the next active iterator.
            for(auto candidate=active_.begin();candidate!=active_.end();++candidate)
                if(std::addressof(*candidate)==saved_next &&
                   candidate->object==saved_next_object) return candidate;
            return active_.end();
        };
        auto task=it->object;
        if(!task)
            return fail(f,RetailTaskRegistryError::invalid_body,
                        "12E670 active link missing real object");
        if(it->cached_cookie!=task->cookie_) {
            it->cached_cookie=1;
            if(!retire_active(it,f)) return false;
            it=advance_to_saved_next();
            if(has_next && it==active_.end()) {
                after_effect_unknown_=true;
                return fail(f,RetailTaskRegistryError::after_effect_unknown,
                            "12E670 saved next link retired during callback");
            }
            continue;
        }
        current_.reset();
        selected_=task;
        if(it->flags!=0 || task->retain_flag2_) {
            return fail(f,RetailTaskRegistryError::unsupported_source_branch,
                        "12E670 nonzero source task/link flags");
        }
        // The admitted 12EC50(0) births have task bit4/bit3 clear, no
        // script, prebody callback, or context. Later producers must expose
        // their real flags and effects before reaching this dispatcher.
        if(!source_bit5_inhibited && !source_root_wait_inhibited) {
            task->stored_wait_-=1.0f;
            const auto bits=std::bit_cast<std::uint32_t>(task->stored_wait_);
            const auto exponent=bits&0x7f800000u;
            const auto fraction=bits&0x007fffffu;
            if(exponent==0x7f800000u || (exponent==0 && fraction!=0)) {
                after_effect_unknown_=true;
                return fail(f,RetailTaskRegistryError::unsupported_source_branch,
                            "12E670 special float wait behavior unproved");
            }
            if(task->stored_wait_<=0.0f) {
                const auto outcome=execution.invoke_virtual_10_initial(task,f.execution);
                if(!outcome)
                    return fail(f,RetailTaskRegistryError::execution,
                                "12E670 real task virtual+10");
            }
        }
        it=advance_to_saved_next();
        if(has_next && it==active_.end()) {
            after_effect_unknown_=true;
            return fail(f,RetailTaskRegistryError::after_effect_unknown,
                        "12E670 saved next link retired during callback");
        }
    }
    // Source 12E4E0 clears selected after its second-root pass. That root is
    // BSS-empty for the admitted first task branch; a live producer remains
    // an explicit separate binding rather than a fabricated empty callback.
    selected_.reset();
    return true;
}

std::optional<std::shared_ptr<NativeRetailTaskObject>>
NativeRetailTaskRegistry::find_first_live(std::uint32_t kind,
                                          RetailTaskRegistryFailure& f) {
    if(!access(f)) return {};
    for(auto it=active_.begin();it!=active_.end();) {
        auto next=std::next(it);
        if(it->cached_cookie!=it->object->cookie_) {
            it->cached_cookie=1;
            if(!retire_active(it,f)) return {};
            it=next;
            continue;
        }
        if(it->object->kind_==kind) return it->object;
        it=next;
    }
    return std::shared_ptr<NativeRetailTaskObject>{};
}

std::optional<std::shared_ptr<NativeRetailInputTaskArgument>>
NativeRetailTaskRegistry::input_argument_from_12e930(
    const std::shared_ptr<NativeRetailTaskObject>& task,
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return {};
    if(!task || !owns(task)) {
        fail(f,RetailTaskRegistryError::foreign_task,"12E930 native task owner");
        return {};
    }
    if(task->retired_) {
        fail(f,RetailTaskRegistryError::retired_task,"12E930 retired native task");
        return {};
    }
    if(!task->observers_e8_) return std::shared_ptr<NativeRetailInputTaskArgument>{};
    const auto found=task->observers_e8_->first_live_from_130860(f.observer);
    if(!found) {
        fail(f,RetailTaskRegistryError::observer_list,"12E930 effectful first-live argument lookup");
        return {};
    }
    if(!*found) return std::shared_ptr<NativeRetailInputTaskArgument>{};
    auto argument=std::dynamic_pointer_cast<NativeRetailInputTaskArgument>(*found);
    if(!argument) {
        fail(f,RetailTaskRegistryError::unsupported_source_branch,
             "12E930 reached native object has no implemented source virtual+4 argument getter");
        return {};
    }
    // Proven59FE40/12D860 returns the same native argument object. Other
    // object getters must be implemented explicitly, never reinterpreted.
    auto self=argument->self_from_12d860(f.argument);
    if(!self) {
        fail(f,RetailTaskRegistryError::input_argument,"12D860 retained input argument getter");
        return {};
    }
    return self;
}

bool NativeRetailTaskRegistry::store_object_cookie_from_source(
    const std::shared_ptr<NativeRetailTaskObject>& task, std::uint32_t cookie,
    RetailTaskRegistryFailure& f) noexcept {
    if(!access(f)) return false;
    if(!owns(task)) return fail(f,RetailTaskRegistryError::foreign_task,
                               "retail task cookie owner");
    task->cookie_=cookie;
    return true;
}
bool NativeRetailTaskRegistry::store_link_flags_from_source(
    const std::shared_ptr<NativeRetailTaskObject>& task, std::uint8_t flags,
    RetailTaskRegistryFailure& f) noexcept {
    if(!access(f)) return false;
    auto it=find_active(task);
    if(it==active_.end()) return fail(f,RetailTaskRegistryError::foreign_task,
                                      "retail task active link flags");
    it->flags=flags;
    return true;
}
bool NativeRetailTaskRegistry::store_object_retention_flag2_from_source(
    const std::shared_ptr<NativeRetailTaskObject>& task, bool retain,
    RetailTaskRegistryFailure& f) noexcept {
    if(!access(f)) return false;
    if(!owns(task)) return fail(f,RetailTaskRegistryError::foreign_task,
                               "retail task retention owner");
    task->retain_flag2_=retain;
    return true;
}
bool NativeRetailTaskRegistry::set_current_task(
    const std::shared_ptr<NativeRetailTaskObject>& task,
    RetailTaskRegistryFailure& f) noexcept {
    if(!access(f)) return false;
    if(!owns(task)) return fail(f,RetailTaskRegistryError::foreign_task,
                               "retail current task owner");
    if(current_ && current_!=task)
        return fail(f,RetailTaskRegistryError::retention_guard,
                    "retail current task already selected");
    current_=task;
    return true;
}
bool NativeRetailTaskRegistry::clear_current_task(RetailTaskRegistryFailure& f) noexcept {
    if(!access(f)) return false;
    current_.reset();
    return true;
}
std::shared_ptr<NativeRetailObserverList>
NativeRetailTaskRegistry::current_task_observers_from_158080(
    RetailTaskRegistryFailure& f) noexcept {
    if(!access(f)) return {};
    if(!current_ || find_active(current_)==active_.end()) {
        fail(f,RetailTaskRegistryError::foreign_task,
             "158080 source current task absent");
        return {};
    }
    if(!current_->observers_e8_) {
        fail(f,RetailTaskRegistryError::game_pool,
             "158080 source task observer root absent");
        return {};
    }
    return current_->observers_e8_;
}
bool NativeRetailTaskRegistry::retire_link(
    const std::shared_ptr<NativeRetailTaskObject>& task,
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return false;
    auto it=find_active(task);
    if(it==active_.end()) return fail(f,RetailTaskRegistryError::foreign_task,
                                      "retail task link retirement owner");
    return retire_active(it,f);
}
std::optional<RetailTaskRegistryObservation> NativeRetailTaskRegistry::observe(
    RetailTaskRegistryFailure& f) const noexcept {
    if(!owner_thread(f)) return {};
    try {
        RetailTaskRegistryObservation result;
        result.ordered.reserve(active_.size());
        for(const auto& link:active_) {
            const auto& task=*link.object;
            result.ordered.push_back({link.object,task.kind_,task.cookie_,
                                      link.cached_cookie,task.priority_,link.flags});
        }
        result.retained_links=retained_.size();
        result.recycled_links=recycled_.size();
        result.pending_links=pending_.size();
        result.after_effect_unknown=after_effect_unknown_;
        return result;
    } catch(const std::bad_alloc&) {
        fail(f,RetailTaskRegistryError::allocation,"retail task observation allocation");
    } catch(...) {
        fail(f,RetailTaskRegistryError::native_exception,"retail task observation");
    }
    return {};
}
} // namespace mka::retail
