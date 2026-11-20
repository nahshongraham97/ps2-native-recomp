#include "mka/frontend_scheduler.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
#include <utility>

// Isolated CPU ownership fixture. No movie source, decoder, GPU or endpoint.
namespace {
thread_local bool forbid_allocations = false;
thread_local unsigned forbidden_attempts = 0;
void check(bool value, const char* operation) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", operation); std::abort(); }
}
struct NoAllocation {
    NoAllocation() { check(!forbid_allocations, "allocation guard nesting"); forbidden_attempts = 0; forbid_allocations = true; }
    ~NoAllocation() { forbid_allocations = false; check(forbidden_attempts == 0, "preallocated cancellation/retirement must not allocate"); }
};
}
void* operator new(std::size_t size) {
    if (forbid_allocations) { ++forbidden_attempts; throw std::bad_alloc(); }
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

namespace {
using namespace mka::game;
struct Completion {
    bool signalled = false, observation_failed = false, completed_error = false;
    unsigned updates = 0, polls = 0, retries = 0, destroyed = 0;
};
struct Detail final : FrontendTaskFailureDetail {
    explicit Detail(unsigned value) : code(value) {}
    unsigned code;
};
enum class Body { block, finish, fail };
class Task final : public NativeFrontendTask {
public:
    Task(Completion& completion, bool owned, Body body, const Completion* predecessor = nullptr,
         std::shared_ptr<const FrontendTaskFailureDetail> detail = {})
        : completion_(completion), owns_(owned), body_(body), predecessor_(predecessor), detail_(std::move(detail)) {}
    ~Task() override {
        check(!owns_, "task destruction cannot abandon actual pending ownership");
        ++completion_.destroyed;
    }
    TaskUpdate update(NativeFrontendExecution&, FrontendSchedulerFailure& failure) override {
        ++completion_.updates;
        if (predecessor_) check(predecessor_->destroyed == 1, "ordinary game task runs only after retained cleanup destroyed owner");
        if (body_ == Body::fail) {
            failure.kind = FrontendSchedulerError::invalid_result;
            failure.operation = "isolated pending task primary failure";
            failure.task_code = 707;
            return TaskUpdate::error;
        }
        return body_ == Body::block ? TaskUpdate::blocked : TaskUpdate::finished;
    }
    bool retirement_required() const noexcept override { return owns_; }
    TaskRetirement retire(FrontendSchedulerFailure& failure) override {
        ++completion_.polls;
        return observe(failure);
    }
    TaskRetirement retry_retirement(FrontendSchedulerFailure& failure) override {
        ++completion_.retries;
        completion_.observation_failed = false;
        return observe(failure);
    }
private:
    TaskRetirement observe(FrontendSchedulerFailure& failure) {
        if (completion_.observation_failed) {
            failure.kind = FrontendSchedulerError::invalid_result;
            failure.operation = "isolated actual completion observation failure";
            failure.task_code = 919;
            return TaskRetirement::error;
        }
        if (!completion_.signalled) return TaskRetirement::pending;
        owns_ = false;
        if (completion_.completed_error) {
            failure.kind = FrontendSchedulerError::invalid_result;
            failure.operation = "isolated completed cleanup terminal failure";
            failure.task_code = 929;
            failure.native_cause = detail_;
            return TaskRetirement::complete_with_error;
        }
        return TaskRetirement::complete;
    }
    Completion& completion_;
    bool owns_;
    Body body_;
    const Completion* predecessor_;
    std::shared_ptr<const FrontendTaskFailureDetail> detail_;
};
std::unique_ptr<mka::input::NativeGameActionInput> actions() {
    mka::input::GameActionFailure failure;
    auto result = mka::input::NativeGameActionInput::create(
        {{0, {{6, 1}, {11, 2}}, false, false}, {1, {{6, 1}, {11, 2}}, false, false}}, false, failure);
    check(bool(result), "actual native action owner");
    return result;
}
std::unique_ptr<NativeFrontendScheduler> scheduler(mka::input::NativeGameActionInput& input) {
    FrontendSchedulerFailure failure;
    auto result = NativeFrontendScheduler::create(input, 0, failure);
    check(bool(result), "actual native frontend scheduler");
    return result;
}
FrontendTaskToken put(NativeFrontendScheduler& queue, Completion& completion, bool owned, Body body,
                      std::int64_t priority, const Completion* predecessor = nullptr,
                      std::shared_ptr<const FrontendTaskFailureDetail> detail = {}) {
    std::unique_ptr<NativeFrontendTask> task = std::make_unique<Task>(completion, owned, body, predecessor, std::move(detail));
    FrontendSchedulerFailure failure;
    auto token = queue.enqueue(task, priority, false, failure);
    check(token && !task && failure.kind == FrontendSchedulerError::none, "exact preallocated task admission");
    return *token;
}
void cancelled_barrier_and_retry() {
    auto input = actions();
    Completion resource, ordinary, unrelated;
    auto queue = scheduler(*input), foreign = scheduler(*input);
    FrontendSchedulerFailure failure;
    const auto token = put(*queue, resource, true, Body::block, 1);
    put(*queue, ordinary, false, Body::finish, 2, &resource);
    const auto other = put(*foreign, unrelated, false, Body::finish, 1);
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && resource.updates == 1,
          "real blocked logical call begins");
    { NoAllocation guard; check(queue->cancel(token, failure), "cancel transfers preallocated node into retirement"); }
    check(resource.destroyed == 0 && resource.polls == 0, "cancel acceptance neither destroys nor invents cleanup completion");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && resource.polls == 1 && ordinary.updates == 0,
          "retirement barrier precedes ordinary task dispatch");
    check(!queue->retry_retirement(token, failure) && failure.kind == FrontendSchedulerError::invalid_state,
          "pending cleanup is not a failed receipt to retry");
    resource.observation_failed = true;
    check(queue->poll_retirements(failure).progress == FrontendDispatchProgress::error && failure.task_code == 919,
          "actual completion error retained with cause");
    const auto polls = resource.polls;
    resource.observation_failed = false;
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::error && failure.task_code == 919 &&
          queue->poll_retirements(failure).progress == FrontendDispatchProgress::error && resource.polls == polls &&
          resource.retries == 0 && ordinary.updates == 0,
          "ordinary polling cannot silently retry a failed completion");
    check(!queue->retry_retirement(other, failure) && failure.kind == FrontendSchedulerError::foreign_owner && resource.retries == 0,
          "foreign token cannot retry exact retained work");
    FrontendSchedulerFailure wrong_thread;
    bool accepted = true;
    std::thread thread([&] { accepted = queue->retry_retirement(token, wrong_thread); });
    thread.join();
    check(!accepted && wrong_thread.kind == FrontendSchedulerError::wrong_thread && resource.retries == 0,
          "foreign thread cannot drive owner completion");
    check(queue->retry_retirement(token, failure) && resource.retries == 1 && resource.destroyed == 0,
          "exact explicit retry returns to pending without dropping ownership");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && ordinary.updates == 0,
          "retried pending cleanup still blocks game tasks");
    resource.signalled = true;
    check(queue->poll_retirements(failure).progress == FrontendDispatchProgress::pass_complete && resource.destroyed == 1,
          "actual external completion releases retained task exactly once");
    check(!queue->retry_retirement(token, failure) && failure.kind == FrontendSchedulerError::stale_task &&
          !queue->cancel(token, failure) && failure.kind == FrontendSchedulerError::stale_task,
          "completed token cannot retry or cancel successor ownership");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::pass_complete && ordinary.updates == 1,
          "ordinary task resumes only after native cleanup barrier");
    check(foreign->dispatch(failure).progress == FrontendDispatchProgress::pass_complete, "unrelated owner survives foreign retry attempt");
}
void finished_task_still_owns_cleanup() {
    auto input = actions(); Completion resource, ordinary; auto queue = scheduler(*input); FrontendSchedulerFailure failure;
    put(*queue, resource, true, Body::finish, 1);
    put(*queue, ordinary, false, Body::finish, 2, &resource);
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && ordinary.updates == 0 && resource.destroyed == 0,
          "finished task with pending resource does not run next game task");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && ordinary.updates == 0,
          "finished task retirement remains pending across host pumps");
    resource.signalled = true;
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::pass_complete && resource.destroyed == 1 && ordinary.updates == 1,
          "completion, destruction and live-next ordering survive asynchronous finish");
}
void monotonic_shutdown_and_failed_task() {
    auto input = actions(); Completion failed, never_run; auto queue = scheduler(*input); FrontendSchedulerFailure failure;
    const auto token = put(*queue, failed, true, Body::fail, 1);
    put(*queue, never_run, false, Body::finish, 2);
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::error && failure.task_code == 707 && failed.destroyed == 0,
          "task failure preserves actual native ownership and primary cause");
    auto before_shutdown = queue->snapshot(failure);
    check(before_shutdown && before_shutdown->failed && before_shutdown->retiring == 1 &&
          before_shutdown->tasks.size() == 1 && !before_shutdown->shutting_down,
          "terminal task immediately transfers actual cleanup to reachable retirement");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && failed.polls == 1 &&
          never_run.updates == 0 && failed.destroyed == 0,
          "failed-task retirement is polled before shutdown without resuming normal jobs");
    { NoAllocation guard; check(queue->shutdown(failure).progress == FrontendDispatchProgress::blocked,
                               "shutdown transfers failed-task owner without allocation"); }
    check(failed.polls == 2 && failed.destroyed == 0 && never_run.destroyed == 1 && never_run.updates == 0,
          "failed task retirement remains reachable and ordinary queued task is removed");
    auto snapshot = queue->snapshot(failure);
    check(snapshot && snapshot->shutting_down && snapshot->failed && snapshot->retiring == 1 && snapshot->tasks.empty(),
          "shutdown is monotonic and retains failed-task cleanup");
    Completion rejected;
    std::unique_ptr<NativeFrontendTask> task = std::make_unique<Task>(rejected, false, Body::finish);
    check(!queue->enqueue(task, 1, false, failure) && task && failure.kind == FrontendSchedulerError::invalid_state,
          "shutdown cannot reopen admission or steal rejected task");
    task.reset();
    failed.observation_failed = true;
    check(queue->shutdown(failure).progress == FrontendDispatchProgress::error && failure.task_code == 919,
          "shutdown cleanup failure remains observable");
    const auto polls = failed.polls;
    failed.signalled = true;
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::error && failed.polls == polls && failed.destroyed == 0,
          "shutdown failure latch never retries implicitly even after signal");
    check(queue->retry_retirement(token, failure) && failed.retries == 1 && failed.destroyed == 1,
          "exact failed-task token still reaches explicit shutdown cleanup retry");
    check(queue->shutdown(failure).progress == FrontendDispatchProgress::pass_complete &&
          queue->dispatch(failure).progress == FrontendDispatchProgress::pass_complete,
          "completed shutdown remains closed and pumps no ordinary task");
    check(!queue->retry_retirement(token, failure) && failure.kind == FrontendSchedulerError::stale_task,
          "shutdown-completed receipt is stale");
}

class TransitionTask final : public NativeFrontendTask {
public:
    TransitionTask(Completion& self, Completion& replacement, const Completion& retiring)
        : self_(self), next_(std::make_unique<Task>(replacement, false, Body::finish, &retiring)) {}
    ~TransitionTask() override { ++self_.destroyed; }
    TaskUpdate update(NativeFrontendExecution& execution, FrontendSchedulerFailure& failure) override {
        ++self_.updates;
        check(execution.clear_for_transition(failure), "actual transition clear accepts current execution");
        auto pending = execution.transition_pending(failure);
        check(pending && *pending, "source transition capability remains live during clear callback");
        auto token = execution.enqueue(next_, 32, false, failure);
        check(token && !next_ && execution.validate_transition_task(*token, 32, failure),
              "replacement is actually owned and validated after clear");
        return TaskUpdate::transition;
    }
private:
    Completion& self_;
    std::unique_ptr<NativeFrontendTask> next_;
};
void transition_waits_for_other_owned_work() {
    auto input = actions(); Completion transitioning, resource, removed, replacement;
    auto queue = scheduler(*input); FrontendSchedulerFailure failure;
    std::unique_ptr<NativeFrontendTask> transition = std::make_unique<TransitionTask>(transitioning, replacement, resource);
    auto transition_token = queue->enqueue(transition, 1, false, failure);
    check(transition_token && !transition, "preallocated transition task owned");
    const auto resource_token = put(*queue, resource, true, Body::block, 2);
    put(*queue, removed, false, Body::finish, 3);
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && transitioning.updates == 1 &&
          transitioning.destroyed == 1 && resource.updates == 0 && resource.polls == 1 && resource.destroyed == 0 &&
          removed.destroyed == 1 && removed.updates == 0 && replacement.updates == 0,
          "clear retires other native work, removes old jobs and withholds transition notification");
    auto snapshot = queue->snapshot(failure);
    check(snapshot && snapshot->retiring == 1 && snapshot->tasks.size() == 1 && snapshot->tasks[0].priority == 32,
          "replacement ownership survives asynchronous old-world cleanup");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::blocked && replacement.updates == 0 && resource.destroyed == 0,
          "pending transition admits no replacement or old ordinary task");
    resource.observation_failed = true;
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::error && failure.task_code == 919 && replacement.updates == 0,
          "transition retirement failure is named before any transition report");
    const auto polls = resource.polls;
    resource.observation_failed = false;
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::error && resource.polls == polls && replacement.updates == 0,
          "failed transition completion never retries or reports handoff implicitly");
    check(queue->retry_retirement(resource_token, failure) && resource.retries == 1 && resource.destroyed == 0,
          "exact cleared task token reaches explicit pending transition cleanup retry");
    resource.signalled = true;
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::transition && resource.destroyed == 1 && replacement.updates == 0,
          "actual completion reports exactly one delayed transition before replacement execution");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::pass_complete && replacement.updates == 1 &&
          replacement.destroyed == 1 && transitioning.updates == 1,
          "replacement runs only in subsequent pass after cleanup and handoff notification");
    check(queue->dispatch(failure).progress == FrontendDispatchProgress::pass_complete && replacement.updates == 1,
          "delayed transition notification cannot repeat after receipt retirement");
}
void terminal_cleanup_error_releases_owner() {
    for (bool via_retry : {false, true}) {
        auto input = actions(); Completion resource, ordinary; auto queue = scheduler(*input); FrontendSchedulerFailure failure;
        const unsigned detail_code = 0xc0120929u + unsigned(via_retry);
        auto detail = std::make_shared<Detail>(detail_code); // Allocate before cancellation/retirement guards.
        std::weak_ptr<const Detail> weak_detail = detail;
        const auto token = put(*queue, resource, true, Body::block, 1, nullptr, std::move(detail));
        check(!detail, "task exclusively receives original typed failure owner");
        put(*queue, ordinary, false, Body::finish, 2);
        check(queue->cancel(token, failure), "terminal cleanup case owns retirement node");
        if (via_retry) {
            resource.observation_failed = true;
            check(queue->poll_retirements(failure).progress == FrontendDispatchProgress::error && resource.destroyed == 0,
                  "retry case begins with genuine pending-owner cleanup failure");
        }
        resource.signalled = true; resource.completed_error = true;
        if (via_retry)
            check(!queue->retry_retirement(token, failure) && failure.task_code == 929,
                  "completed cleanup with error is failure even on explicit retry");
        else
            check(queue->poll_retirements(failure).progress == FrontendDispatchProgress::error && failure.task_code == 929,
                  "completed cleanup with error remains an observable terminal failure");
        check(resource.destroyed == 1 && ordinary.updates == 0,
              "complete_with_error destroys actual resource owner without admitting normal work");
        auto snapshot = queue->snapshot(failure);
        check(snapshot && snapshot->failed && snapshot->retiring == 0 && snapshot->retirement_failure &&
              snapshot->retirement_failure->task_code == 929 && snapshot->tasks.size() == 1,
              "terminal receipt cause persists in snapshot after native owner destruction");
        const auto* retained = dynamic_cast<const Detail*>(snapshot->retirement_failure->native_cause.get());
        check(retained && retained->code == detail_code && !weak_detail.expired() && resource.destroyed == 1,
              "actual typed native detail survives destroyed task in scheduler snapshot");
        const auto polls = resource.polls, retries = resource.retries;
        check(queue->dispatch(failure).progress == FrontendDispatchProgress::error && ordinary.updates == 0 &&
              resource.polls == polls && resource.retries == retries && resource.destroyed == 1,
              "completed-error receipt cannot restart cleanup or normal dispatch");
        check(!queue->retry_retirement(token, failure) && failure.kind == FrontendSchedulerError::stale_task,
              "destroyed completed-error owner has no retryable native resource");
        check(queue->shutdown(failure).progress == FrontendDispatchProgress::pass_complete && ordinary.destroyed == 1 && ordinary.updates == 0,
              "terminal cleanup failure still permits truthful resource-free shutdown");
        snapshot = queue->snapshot(failure);
        check(snapshot && snapshot->shutting_down && snapshot->retirement_failure && snapshot->retirement_failure->task_code == 929,
              "shutdown preserves completed-error evidence after all jobs are gone");
        retained = dynamic_cast<const Detail*>(snapshot->retirement_failure->native_cause.get());
        check(retained && retained->code == detail_code, "shutdown retains full typed cause rather than only summary code");
        queue.reset();
        check(!weak_detail.expired(), "snapshot independently retains typed diagnostics after scheduler destruction");
        snapshot.reset(); failure = {};
        check(weak_detail.expired(), "typed failure ownership releases without hidden task or scheduler cycle");
    }
}
}
int main() {
    cancelled_barrier_and_retry();
    finished_task_still_owns_cleanup();
    monotonic_shutdown_and_failed_task();
    transition_waits_for_other_owned_work();
    terminal_cleanup_error_releases_owner();
    std::puts("PASS pending native frontend retirement/no-allocation cancel/exact retry/shutdown; CPU scheduler only, no movie decode or endpoint");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
