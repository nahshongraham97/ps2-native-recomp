#include "mka/retail_initial_game_tasks.hpp"
#include "retail_main_control.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailInitialGameTaskFailure& f,RetailInitialGameTaskError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
constexpr std::size_t callback_index(RetailInitialGameTaskCallback value) noexcept {
    return static_cast<std::size_t>(value);
}
struct SourceBirth {
    std::uint32_t kind;
    std::int32_t priority;
};
SourceBirth source_birth(RetailInitialGameTaskBodyKind kind) noexcept {
    switch(kind) {
    case RetailInitialGameTaskBodyKind::camera_13c220: return {0x5001,0x0f};
    case RetailInitialGameTaskBodyKind::actors_13a7a0: return {0x5006,0x13};
    case RetailInitialGameTaskBodyKind::scene_16c1f0: return {0x5003,0x17};
    case RetailInitialGameTaskBodyKind::per_task_158080: return {0x500b,0x12};
    }
    return {};
}
}

RetailInitialGameTaskException::RetailInitialGameTaskException(
    RetailInitialGameTaskError kind,const char* operation)
    : std::runtime_error(operation),kind_(kind) {}

NativeRetailInitialGameTasks::NativeRetailInitialGameTasks(
    std::shared_ptr<NativeRetailGameTaskLists> lists,
    std::shared_ptr<NativeRetailGamePool> pool) noexcept
    : lists_(std::move(lists)),pool_(std::move(pool)) {}

std::shared_ptr<NativeRetailInitialGameTasks>
NativeRetailInitialGameTasks::create(
    std::shared_ptr<NativeRetailGameTaskLists> lists,
    std::shared_ptr<NativeRetailGamePool> pool,
    RetailInitialGameTaskFailure& f) noexcept {
    f={};
    if(!pool) {
        fail(f,RetailInitialGameTaskError::missing_pool,
             "initial game tasks actual 12DA70 pool absent");
        return {};
    }
    if(!lists || !lists->same_pool(pool,f.lists)) {
        fail(f,RetailInitialGameTaskError::missing_lists,
             "initial game tasks actual Session list owner absent or foreign");
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailInitialGameTasks>(
            new NativeRetailInitialGameTasks(std::move(lists),std::move(pool)));
    } catch(const std::bad_alloc&) {
        fail(f,RetailInitialGameTaskError::allocation,
             "initial game task owner allocation");
    } catch(...) {
        fail(f,RetailInitialGameTaskError::native_exception,
             "initial game task owner creation");
    }
    return {};
}

bool NativeRetailInitialGameTasks::access(
    RetailInitialGameTaskFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailInitialGameTaskError::wrong_thread,
                    "initial game tasks owner thread");
    if(!pool_ || !lists_)
        return fail(f,RetailInitialGameTaskError::missing_pool,
                    "initial game tasks retained owners absent");
    return true;
}

bool NativeRetailInitialGameTasks::same_pool(
    const std::shared_ptr<NativeRetailGamePool>& pool,
    RetailInitialGameTaskFailure& f) const noexcept {
    if(!access(f)) return false;
    if(pool_!=pool)
        return fail(f,RetailInitialGameTaskError::foreign_pool,
                    "initial game tasks source pool identity");
    return true;
}

bool NativeRetailInitialGameTasks::same_lists(
    const std::shared_ptr<NativeRetailGameTaskLists>& lists,
    RetailInitialGameTaskFailure& f) const noexcept {
    if(!access(f)) return false;
    if(lists_!=lists)
        return fail(f,RetailInitialGameTaskError::missing_lists,
                    "initial game tasks persistent Session callback-list identity");
    return true;
}

std::shared_ptr<NativeRetailGamePool>
NativeRetailInitialGameTasks::retained_pool(
    RetailInitialGameTaskFailure& f) const noexcept {
    if(!access(f)) return {};
    return pool_;
}

bool NativeRetailInitialGameTasks::bind_camera_actor_from_source(
    std::shared_ptr<NativeRetailCameraActorState> actor,
    std::uint32_t saved_identity,RetailInitialGameTaskFailure& f) noexcept {
    if(!access(f)) return false;
    actor_slot_=std::move(actor);
    actor_saved_identity_=saved_identity;
    return true;
}

bool NativeRetailInitialGameTasks::bind_callback_from_native_source(
    RetailInitialGameTaskCallback which,
    std::shared_ptr<NativeRetailObserverCallback> callback,
    RetailInitialGameTaskFailure& f) noexcept {
    if(!access(f)) return false;
    const auto index=callback_index(which);
    if(index>=callbacks_.size() || !callback)
        return fail(f,RetailInitialGameTaskError::missing_callback,
                    "initial game task typed callback target absent");
    callbacks_[index]=std::move(callback);
    return true;
}

std::optional<RetailInitialCameraObservation>
NativeRetailInitialGameTasks::observe_camera(
    RetailInitialGameTaskFailure& f) const noexcept {
    if(!access(f)) return {};
    auto result=camera_;
    result.actor_slot_bound=!actor_slot_.expired();
    return result;
}

bool NativeRetailInitialGameTasks::run_camera_127120(
    RetailInitialGameTaskFailure& f) noexcept {
    const auto actor=actor_slot_.lock();
    if(actor && actor->identity==actor_saved_identity_)
        return fail(f,RetailInitialGameTaskError::actor_branch_unbound,
                    "127120 populated actor camera transform and 157C10");
    // Source 1817F0(0) returns the first actor slot. Its initial BSS pointer
    // is null; a missing/stale identity takes 127120's no-actor branch.
    camera_.basis={1.0f,0.0f,0.0f,
                   0.0f,1.0f,0.0f,
                   0.0f,0.0f,1.0f};
    camera_.position={0.0f,0.0f,0.0f};
    camera_.flags|=0x00020003u;
    return true;
}

bool NativeRetailInitialGameTasks::visit(
    RetailGameTaskListRoot root_id,RetailInitialGameTaskCallback which,
    bool actor_route,RetailInitialGameTaskFailure& f) {
    const auto root=lists_->root(root_id,f.lists);
    if(!root)
        return fail(f,RetailInitialGameTaskError::missing_lists,
                    "initial game task source callback root absent");
    const auto state=root->observe(f.observer);
    if(!state)
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "initial game task callback root observation");
    if(state->pending || state->after_effect_unknown)
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "initial game task callback root pending prior effect");
    if(state->linked==0) return true;
    const auto index=callback_index(which);
    if(index>=callbacks_.size() || !callbacks_[index])
        return fail(f,RetailInitialGameTaskError::missing_callback,
                    "initial game task live source callback target unbound");
    const auto retained_callback=callbacks_[index];
    const bool okay=actor_route
        ? root->visit_from_13a7a0(*retained_callback,f.observer)
        : root->visit_from_12f4e0(*retained_callback,f.observer);
    if(!okay)
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "initial game task real callback traversal");
    return true;
}

bool NativeRetailInitialGameTasks::visit_task_e8(
    const std::shared_ptr<NativeRetailObserverList>& root,
    RetailInitialGameTaskFailure& f) {
    if(!root)
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "158080 current task +E8 root missing");
    const auto state=root->observe(f.observer);
    if(!state)
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "158080 current task +E8 observation");
    if(state->pending || state->after_effect_unknown)
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "158080 task root pending prior effect");
    if(state->linked==0) return true;
    const auto index=callback_index(RetailInitialGameTaskCallback::per_task_1580b0);
    if(!callbacks_[index])
        return fail(f,RetailInitialGameTaskError::missing_callback,
                    "158080 live 1580B0 target unbound");
    const auto retained_callback=callbacks_[index];
    if(!root->visit_from_12f4e0(*retained_callback,f.observer))
        return fail(f,RetailInitialGameTaskError::observer_list,
                    "158080 source current task observer traversal");
    return true;
}

bool NativeRetailInitialGameTasks::run_source_body(
    RetailInitialGameTaskBodyKind kind,
    const std::shared_ptr<NativeRetailObserverList>& task_e8,
    RetailInitialGameTaskFailure& f) {
    if(!access(f)) return false;
    switch(kind) {
    case RetailInitialGameTaskBodyKind::camera_13c220:
        return run_camera_127120(f) &&
            visit(RetailGameTaskListRoot::e024,
                  RetailInitialGameTaskCallback::camera_13c540,false,f) &&
            visit(RetailGameTaskListRoot::e020,
                  RetailInitialGameTaskCallback::camera_13c540,false,f);
    case RetailInitialGameTaskBodyKind::actors_13a7a0:
        return visit(RetailGameTaskListRoot::dfe4,
                     RetailInitialGameTaskCallback::actor_13a870,true,f) &&
            visit(RetailGameTaskListRoot::e02c,
                  RetailInitialGameTaskCallback::actor_pose_139af0,false,f) &&
            visit(RetailGameTaskListRoot::dfe0,
                  RetailInitialGameTaskCallback::actor_motion_139bd0,false,f);
    case RetailInitialGameTaskBodyKind::scene_16c1f0:
        return visit(RetailGameTaskListRoot::e1ac,
                     RetailInitialGameTaskCallback::scene_16c270,false,f) &&
            visit(RetailGameTaskListRoot::e1a8,
                  RetailInitialGameTaskCallback::scene_227f20,false,f);
    case RetailInitialGameTaskBodyKind::per_task_158080:
        return visit_task_e8(task_e8,f);
    }
    return fail(f,RetailInitialGameTaskError::native_exception,
                "initial game task unknown source body");
}

std::shared_ptr<NativeRetailTaskObject>
NativeRetailInitialGameTasks::publish_source_task(
    RetailInitialGameTaskBodyKind kind,NativeRetailTaskDomain& domain,
    RetailInitialGameTaskFailure& f) {
    if(!access(f)) return {};
    const auto source=source_birth(kind);
    if(source.kind==0) {
        fail(f,RetailInitialGameTaskError::native_exception,
             "initial game task unknown source birth");
        return {};
    }
    auto* registry=domain.registry(f.domain);
    if(!registry) {
        fail(f,RetailInitialGameTaskError::domain,
             "initial game task source registry not born");
        return {};
    }
    std::shared_ptr<NativeRetailObserverList> e8,ec;
    try {
        e8=std::make_shared<NativeRetailObserverList>(pool_);
        ec=std::make_shared<NativeRetailObserverList>(pool_);
    } catch(const std::bad_alloc&) {
        fail(f,RetailInitialGameTaskError::allocation,
             "12EC50 two native per-task observer roots");
        return {};
    }
    auto self=shared_from_this();
    auto body=NativeRetailInitialGameTaskBody::create(kind,std::move(self),e8,ec,f);
    if(!body) return {};
    const auto cookie=domain.next_cookie_from_130a50(f.domain);
    if(!cookie) {
        fail(f,RetailInitialGameTaskError::domain,
             "12EC50 actual task identity producer");
        return {};
    }
    auto task=registry->publish_task(source.kind,source.priority,*cookie,
                                     std::move(body),std::move(e8),std::move(ec),
                                     f.registry);
    if(!task)
        fail(f,RetailInitialGameTaskError::registry,
             "12F100 real ordered task publication");
    return task;
}

NativeRetailInitialGameTaskBody::NativeRetailInitialGameTaskBody(
    RetailInitialGameTaskBodyKind kind,
    std::shared_ptr<NativeRetailInitialGameTasks> owner,
    std::shared_ptr<NativeRetailObserverList> e8,
    std::shared_ptr<NativeRetailObserverList> ec) noexcept
    : kind_(kind),owner_(std::move(owner)),task_e8_(std::move(e8)),
      task_ec_(std::move(ec)) {}

std::shared_ptr<NativeRetailInitialGameTaskBody>
NativeRetailInitialGameTaskBody::create(
    RetailInitialGameTaskBodyKind kind,
    std::shared_ptr<NativeRetailInitialGameTasks> owner,
    std::shared_ptr<NativeRetailObserverList> e8,
    std::shared_ptr<NativeRetailObserverList> ec,
    RetailInitialGameTaskFailure& f) noexcept {
    f={};
    if(!owner || !e8 || !ec || e8==ec) {
        fail(f,RetailInitialGameTaskError::observer_list,
             "12EC50 distinct task owner and observer roots required");
        return {};
    }
    const auto pool=owner->retained_pool(f);
    if(!pool) return {};
    if(e8->retained_pool(f.observer)!=pool ||
       ec->retained_pool(f.observer)!=pool) {
        fail(f,RetailInitialGameTaskError::foreign_pool,
             "12EC50 per-task observer roots foreign pool");
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailInitialGameTaskBody>(
            new NativeRetailInitialGameTaskBody(kind,std::move(owner),
                                                std::move(e8),std::move(ec)));
    } catch(const std::bad_alloc&) {
        fail(f,RetailInitialGameTaskError::allocation,
             "12EC50 real native task body allocation");
    } catch(...) {
        fail(f,RetailInitialGameTaskError::native_exception,
             "12EC50 real native task body creation");
    }
    return {};
}

NativeTaskStep NativeRetailInitialGameTaskBody::run() {
    RetailInitialGameTaskFailure f;
    if(!owner_->retained_pool(f))
        throw RetailInitialGameTaskException(f.kind,f.operation);
    if(retired_)
        throw RetailInitialGameTaskException(
            RetailInitialGameTaskError::retired_body,
            "initial source task body already retired");
    if(after_effect_unknown_)
        throw RetailInitialGameTaskException(
            RetailInitialGameTaskError::native_exception,
            "initial source task body prior partial effect");
    try {
        if(!owner_->run_source_body(kind_,task_e8_,f)) {
            after_effect_unknown_=true;
            throw RetailInitialGameTaskException(f.kind,f.operation);
        }
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(...) {
        after_effect_unknown_=true;
        throw;
    }
    return NativeTaskStep::returned(1.0f); // All four source bodies return 3F800000.
}

void NativeRetailInitialGameTaskBody::cleanup(bool release_allowed) {
    if(!release_allowed) return; // 12EE70 current/bit2 guard.
    RetailInitialGameTaskFailure f;
    if(!owner_->retained_pool(f))
        throw RetailInitialGameTaskException(f.kind,f.operation);
    if(retired_)
        throw RetailInitialGameTaskException(
            RetailInitialGameTaskError::retired_body,
            "initial source task body cleanup replay");
    // This body has no native child stack. Registry owns the real 12EE70
    // +E8 then +EC list drains after this callback and retires the task.
    retired_=true;
}

} // namespace mka::retail
