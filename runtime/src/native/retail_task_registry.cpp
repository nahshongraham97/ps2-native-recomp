#include "mka/retail_task_registry.hpp"
#include "retail_main_control.hpp"

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

bool NativeRetailTaskRegistry::retire_active(
    std::list<Link>::iterator it, RetailTaskRegistryFailure&) {
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
            recycled_.splice(recycled_.begin(),pending_,pending);
            return true;
        }
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
        retained_.splice(retained_.end(),pending_,pending);
        return true;
    }
    task->body_.reset();
    task->continuation_.reset();
    task->wait_resume_.reset();
    pending->object.reset();
    pending->cached_cookie=1;
    pending->flags=0;
    recycled_.splice(recycled_.begin(),pending_,pending);
    return true;
}

std::shared_ptr<NativeRetailTaskObject> NativeRetailTaskRegistry::publish_task(
    std::uint32_t kind, std::int32_t priority, std::uint32_t source_cookie,
    std::shared_ptr<NativeRetailTaskBody> body,
    RetailTaskRegistryFailure& f) {
    if(!access(f)) return {};
    if(!body) {
        fail(f,RetailTaskRegistryError::invalid_body,
             "retail task publication needs executable body and cleanup");
        return {};
    }
    std::shared_ptr<NativeRetailTaskObject> task;
    std::list<Link> staged;
    try {
        task=std::shared_ptr<NativeRetailTaskObject>(
            new NativeRetailTaskObject(kind,priority,source_cookie,body));
        // 12FFE0 clears the recycled link flag byte at publication.
        staged.push_back(Link{task,source_cookie,0});
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
            retire_active(it,f);
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

std::optional<std::shared_ptr<NativeRetailTaskObject>>
NativeRetailTaskRegistry::find_first_live(std::uint32_t kind,
                                          RetailTaskRegistryFailure& f) {
    if(!access(f)) return {};
    for(auto it=active_.begin();it!=active_.end();) {
        auto next=std::next(it);
        if(it->cached_cookie!=it->object->cookie_) {
            it->cached_cookie=1;
            retire_active(it,f);
            it=next;
            continue;
        }
        if(it->object->kind_==kind) return it->object;
        it=next;
    }
    return std::shared_ptr<NativeRetailTaskObject>{};
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
