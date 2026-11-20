#pragma once
#include "mka/game_action_input.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <utility>
namespace mka::game {
enum class FrontendSchedulerError {none,wrong_thread,invalid_state,missing_task,foreign_owner,stale_task,stale_execution,overflow,allocation,input,task_exception,invalid_result};
// Owned typed native diagnostics survive destruction of their originating task.
struct FrontendTaskFailureDetail {virtual ~FrontendTaskFailureDetail()=default;};
struct FrontendSchedulerFailure {FrontendSchedulerError kind=FrontendSchedulerError::none;const char* operation="";input::GameActionFailure input;std::int64_t task_code=0;std::shared_ptr<const FrontendTaskFailureDetail> native_cause;};
// blocked retains the in-progress call/pass; yield advances to the live next
// task and makes this task eligible on a later SOURCE pass, never a wall tick.
enum class TaskRetirement {pending,complete,complete_with_error,error};
enum class TaskUpdate {blocked,yield,finished,transition,error};
enum class FrontendDispatchProgress {pass_complete,blocked,transition,error};
struct FrontendDispatchResult {FrontendDispatchProgress progress=FrontendDispatchProgress::error;std::uint64_t updates=0;};
class NativeFrontendScheduler;
struct FrontendExecutionControl;
class FrontendTaskToken {
public: FrontendTaskToken(const FrontendTaskToken&)=default;FrontendTaskToken& operator=(const FrontendTaskToken&)=default;
private:friend class NativeFrontendScheduler;friend class NativeFrontendExecution;
 FrontendTaskToken(std::shared_ptr<const void> owner,std::uint64_t generation):owner_(std::move(owner)),generation_(generation){}
 std::shared_ptr<const void> owner_;std::uint64_t generation_;
};
class NativeFrontendTask;
// A checked callback capability. Copies become stale when update returns, and
// remain safe to reject after scheduler destruction. Never owns the scheduler.
class NativeFrontendExecution final {
public:
 NativeFrontendExecution(const NativeFrontendExecution&)=default;
 NativeFrontendExecution& operator=(const NativeFrontendExecution&)=default;
 std::optional<FrontendTaskToken> enqueue(std::unique_ptr<NativeFrontendTask>&,std::int64_t priority,bool gate_override,FrontendSchedulerFailure&) noexcept;
 // Authenticates a live pending owned task after clear; does not prove its body semantics.
 bool validate_transition_task(const FrontendTaskToken&,std::int64_t expected_priority,FrontendSchedulerFailure&) const noexcept;
 bool clear_for_transition(FrontendSchedulerFailure&) noexcept;
 bool set_suspension_count(std::uint64_t,FrontendSchedulerFailure&) noexcept;
 std::optional<bool> transition_pending(FrontendSchedulerFailure&) const noexcept;
private:friend class NativeFrontendScheduler;
 NativeFrontendExecution(std::shared_ptr<FrontendExecutionControl>,std::uint64_t);
 NativeFrontendScheduler* access(FrontendSchedulerFailure&) const noexcept;
 std::shared_ptr<FrontendExecutionControl> control_;std::uint64_t generation_;
};
class NativeFrontendTask {
public: virtual ~NativeFrontendTask()=default;
 virtual TaskUpdate update(NativeFrontendExecution&,FrontendSchedulerFailure&)=0;
 // Only resources with pending native completion require deferred destruction.
 virtual bool retirement_required() const noexcept {return false;}
 virtual TaskRetirement retire(FrontendSchedulerFailure&) {return TaskRetirement::complete;}
 virtual TaskRetirement retry_retirement(FrontendSchedulerFailure& f) {return retire(f);}
};
// executing means retained logical current, including a blocked callback.
struct FrontendTaskState {FrontendTaskToken token;std::int64_t priority;bool gate_override,executing;};
struct FrontendSchedulerSnapshot {std::vector<FrontendTaskState> tasks;std::uint64_t suspension_count=0;bool pass_active=false,blocked=false,transition_pending=false,failed=false,shutting_down=false;std::size_t retiring=0;std::optional<FrontendSchedulerFailure> retirement_failure;};
class NativeFrontendScheduler final {
public:
 // actions must outlive this owner. Only the normal no-wait/delay1 profile
 // is represented; other source profiles must fail in their typed producer.
 static std::unique_ptr<NativeFrontendScheduler> create(input::NativeGameActionInput&,std::uint64_t explicit_suspension_count,FrontendSchedulerFailure&) noexcept;
 ~NativeFrontendScheduler();
 NativeFrontendScheduler(const NativeFrontendScheduler&)=delete;
 NativeFrontendScheduler& operator=(const NativeFrontendScheduler&)=delete;
 // Transfer only on success; failure leaves the caller's unique task intact.
 std::optional<FrontendTaskToken> enqueue(std::unique_ptr<NativeFrontendTask>&,std::int64_t priority,bool explicit_gate_override,FrontendSchedulerFailure&) noexcept;
 FrontendDispatchResult dispatch(FrontendSchedulerFailure&) noexcept;
 // Cancellation acceptance is not native cleanup completion. Async tasks remain
 // owned in retirement, which dispatch pumps before any ordinary game task.
 bool cancel(const FrontendTaskToken&,FrontendSchedulerFailure&) noexcept;
 FrontendDispatchResult poll_retirements(FrontendSchedulerFailure&) noexcept;
 // Failed cleanup is retained and never retried by ordinary dispatch.
 bool retry_retirement(const FrontendTaskToken&,FrontendSchedulerFailure&) noexcept;
 // Monotonic host shutdown; pump until pass_complete before destroying owners.
 FrontendDispatchResult shutdown(FrontendSchedulerFailure&) noexcept;
 bool set_suspension_count(std::uint64_t,FrontendSchedulerFailure&) noexcept;
 std::optional<FrontendSchedulerSnapshot> snapshot(FrontendSchedulerFailure&) const noexcept;
private:
 friend class NativeFrontendExecution;
 struct Impl;explicit NativeFrontendScheduler(std::unique_ptr<Impl>);
 bool access(FrontendSchedulerFailure&) const noexcept;
 std::optional<FrontendTaskToken> enqueue_owned(std::unique_ptr<NativeFrontendTask>&,std::int64_t,bool,FrontendSchedulerFailure&) noexcept;
 bool clear_transition_owned(FrontendSchedulerFailure&) noexcept;
 FrontendDispatchResult retire_owned(FrontendSchedulerFailure&) noexcept;
 std::unique_ptr<Impl> impl_;
};
} // namespace mka::game
