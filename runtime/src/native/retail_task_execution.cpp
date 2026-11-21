#include "mka/retail_task_execution.hpp"
#include "mka/retail_task_registry.hpp"
#include "retail_main_control.hpp"

#include <bit>
#include <limits>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailTaskExecutionFailure& f, RetailTaskExecutionError kind,
          const char* operation) noexcept {
    f={kind,operation};
    return false;
}
bool supported_float(float value) noexcept {
    const auto bits=std::bit_cast<std::uint32_t>(value);
    const auto exponent=bits&0x7f800000u;
    const auto fraction=bits&0x007fffffu;
    return exponent!=0x7f800000u && (exponent!=0 || fraction==0);
}
}

NativeTaskStep NativeTaskStep::returned(float value) noexcept {
    return {NativeTaskStepKind::returned_float,value,{}};
}
NativeTaskStep NativeTaskStep::yield(
    std::shared_ptr<NativeRetailTaskContinuation> next,float value) noexcept {
    return {NativeTaskStepKind::yield_to,value,std::move(next)};
}
NativeTaskStep NativeTaskStep::wait(
    std::shared_ptr<NativeRetailTaskContinuation> resume,float duration) noexcept {
    return {NativeTaskStepKind::wait_at,duration,std::move(resume)};
}

NativeRetailTaskExecution::NativeRetailTaskExecution(
    NativeRetailTaskRegistry& registry) noexcept : registry_(registry) {}

std::optional<NativeTaskRunOutcome> NativeRetailTaskExecution::invoke_virtual_10_initial(
    const std::shared_ptr<NativeRetailTaskObject>& task,
    RetailTaskExecutionFailure& f) {
    f={};
    RetailTaskRegistryFailure registry_failure;
    if(!registry_.access(registry_failure)) {
        const auto kind=registry_failure.kind==RetailTaskRegistryError::wrong_thread
            ? RetailTaskExecutionError::wrong_thread
            : RetailTaskExecutionError::after_effect_unknown;
        fail(f,kind,registry_failure.operation);
        return {};
    }
    auto active=registry_.find_active(task);
    if(active==registry_.active_.end() || !task || task->retired_ ||
       active->cached_cookie!=task->cookie_ || task->cookie_==0) {
        fail(f,RetailTaskExecutionError::foreign_task,"typed task registry identity");
        return {};
    }
    if(active->flags!=0) {
        fail(f,RetailTaskExecutionError::unsupported_source_branch,
             "typed initial task link flags");
        return {};
    }
    if(task->retain_flag2_) {
        fail(f,RetailTaskExecutionError::unsupported_source_branch,
             "typed source task retained cleanup flag");
        return {};
    }
    if(!task->body_ || !task->continuation_) {
        fail(f,RetailTaskExecutionError::missing_body,"typed initial task body");
        return {};
    }
    const bool plain=task->program_class_==RetailTaskProgramClass::plain_12ec50;
    const bool retained=task->program_class_==RetailTaskProgramClass::retained_frame_12ea90;
    if(!plain && !retained) {
        fail(f,RetailTaskExecutionError::unsupported_source_branch,
             "task constructor class has no verified native virtual execution");
        return {};
    }
    if(plain && (task->resume_pending_ || task->wait_resume_)) {
        fail(f,RetailTaskExecutionError::unsupported_source_branch,
             "stock12EC50 task cannot own an extended saved wait");
        return {};
    }
    if(task->execution_active_ || registry_.execution_call_active_) {
        fail(f,RetailTaskExecutionError::reentrant,"typed task execution recursive entry");
        return {};
    }
    if(task->execution_transfer_pending_) {
        fail(f,RetailTaskExecutionError::unsupported_source_branch,
             "typed task post-main-transfer continuation unproved");
        return {};
    }
    registry_.execution_call_active_=true;
    struct CallGuard {
        bool& active;
        ~CallGuard() {active=false;}
    } call_guard{registry_.execution_call_active_};
    task->execution_active_=true;
    registry_.selected_=task; // Source 12E6E4 selected identity.
    registry_.current_=task; // Source 12E8A8 after prebody callback and cookie reload.
    std::uint64_t calls=0;
    for(;;) {
        // Stock always invokes +E0. Only the retained frame has a saved
        // source+14 continuation; no guest stack/register image is retained.
        auto continuation=retained && task->resume_pending_ ? task->wait_resume_
                                                 : task->continuation_;
        if(retained && task->resume_pending_) {
            task->resume_pending_=false;
            task->wait_resume_.reset();
        }
        if(!continuation) {
            registry_.after_effect_unknown_=true;
            task->execution_active_=false;
            fail(f,RetailTaskExecutionError::missing_body,
                 "typed task continuation missing after publication");
            return {};
        }
        NativeTaskStep step;
        try {
            step=continuation->run();
        } catch(const detail::NativeMainReentry&) {
            task->execution_active_=false;
            task->execution_transfer_pending_=true;
            throw;
        } catch(...) {
            task->execution_active_=false;
            registry_.after_effect_unknown_=true;
            fail(f,RetailTaskExecutionError::native_exception,
                 "typed task body after-effect exception");
            return {};
        }
        if(calls!=std::numeric_limits<std::uint64_t>::max()) ++calls;
        if(!supported_float(step.value)) {
            task->execution_active_=false;
            registry_.after_effect_unknown_=true;
            fail(f,RetailTaskExecutionError::unsupported_float,
                 "task EE special floating behavior unproved");
            return {};
        }
        if(step.kind==NativeTaskStepKind::wait_at) {
            if(plain) {
                task->execution_active_=false;
                registry_.after_effect_unknown_=true;
                fail(f,RetailTaskExecutionError::unsupported_source_branch,
                     "stock virtual12E090 returns; retained wait requires12EA90 frame");
                return {};
            }
            if(!step.next) {
                task->execution_active_=false;
                registry_.after_effect_unknown_=true;
                fail(f,RetailTaskExecutionError::malformed_step,
                     "task wait missing typed resume point");
                return {};
            }
            // Source +14 saves E4/F4, then the float64DF58 value at +C.
            task->wait_resume_=std::move(step.next);
            task->resume_pending_=true;
            task->stored_wait_=step.value;
            task->execution_active_=false;
            return NativeTaskRunOutcome{NativeTaskRunKind::retained_wait,
                                        task->stored_wait_,calls};
        }
        if(step.kind==NativeTaskStepKind::yield_to) {
            if(!step.next) {
                task->execution_active_=false;
                registry_.after_effect_unknown_=true;
                fail(f,RetailTaskExecutionError::malformed_step,
                     "task yield missing typed continuation");
                return {};
            }
            task->continuation_=std::move(step.next); // Source +20 stores E0.
        } else if(step.kind!=NativeTaskStepKind::returned_float || step.next) {
            task->execution_active_=false;
            registry_.after_effect_unknown_=true;
            fail(f,RetailTaskExecutionError::malformed_step,
                 "task source return result shape");
            return {};
        }
        // c.eq.s(0,f0): +0 and -0 both immediately reenter +E0.
        if(step.value==0.0f) continue;
        task->stored_wait_=step.value;
        // Both proved constructors clear +DC.12E034 and12E3B4 bypass the
        // selected/cookie recheck when that callback is null. A later nonnull
        // producer needs its actual callback and live recheck before admission.
        if(0.0f<=task->stored_wait_) {
            task->execution_active_=false;
            return NativeTaskRunOutcome{NativeTaskRunKind::returned_nonnegative,
                                        task->stored_wait_,calls};
        }
        registry_.current_.reset(); // 12E3EC before virtual cleanup +C.
        const bool release_allowed=!task->retain_flag2_;
        if(release_allowed) {
            // Both concrete virtual+C routines invalidate before releasing
            // native frame/nested ownership; their release order differs.
            task->cookie_=0;
            task->retired_=true;
        }
        registry_.in_callout_=true;
        try {
            if(release_allowed) {
                RetailTaskRegistryFailure nested_failure;
                if(!registry_.release_task_payload(
                        *task,nested_failure)) {
                    registry_.in_callout_=false;
                    task->execution_active_=false;
                    registry_.after_effect_unknown_=true;
                    fail(f,RetailTaskExecutionError::after_effect_unknown,
                         nested_failure.operation);
                    return {};
                }
            }
        } catch(const detail::NativeMainReentry&) {
            registry_.in_callout_=false;
            task->execution_active_=false;
            task->execution_transfer_pending_=true;
            throw;
        } catch(...) {
            registry_.in_callout_=false;
            task->execution_active_=false;
            registry_.after_effect_unknown_=true;
            fail(f,RetailTaskExecutionError::native_exception,
                 "task direct cleanup after-effect exception");
            return {};
        }
        registry_.in_callout_=false;
        task->execution_active_=false;
        if(!release_allowed)
            return NativeTaskRunOutcome{NativeTaskRunKind::cleanup_guarded,
                                        task->stored_wait_,calls};
        // Concrete virtual cleanup retires the object. The source link remains in its
        // list until a later stale-identity pass; do not unlink it here.
        task->continuation_.reset();
        task->wait_resume_.reset();
        task->body_.reset();
        return NativeTaskRunOutcome{NativeTaskRunKind::cleaned_negative,
                                    task->stored_wait_,calls};
    }
}

std::optional<NativeTaskExecutionObservation> NativeRetailTaskExecution::observe(
    const std::shared_ptr<NativeRetailTaskObject>& task,
    RetailTaskExecutionFailure& f) const noexcept {
    f={};
    RetailTaskRegistryFailure owner;
    if(!registry_.owner_thread(owner)) {
        fail(f,RetailTaskExecutionError::wrong_thread,owner.operation);
        return {};
    }
    if(!registry_.owns(task)) {
        fail(f,RetailTaskExecutionError::foreign_task,"typed task observation owner");
        return {};
    }
    return NativeTaskExecutionObservation{task->stored_wait_,task->resume_pending_,
                                          task->execution_active_,
                                          task->execution_transfer_pending_};
}
} // namespace mka::retail
