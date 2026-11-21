#pragma once

#include <cstdint>
#include <memory>
#include <optional>

namespace mka::retail {
class NativeRetailTaskRegistry;
class NativeRetailTaskObject;

enum class NativeTaskStepKind { returned_float, yield_to, wait_at };
class NativeRetailTaskContinuation;
struct NativeTaskStep {
    NativeTaskStepKind kind{NativeTaskStepKind::returned_float};
    float value{};
    std::shared_ptr<NativeRetailTaskContinuation> next;
    static NativeTaskStep returned(float value) noexcept;
    static NativeTaskStep yield(std::shared_ptr<NativeRetailTaskContinuation>, float value) noexcept;
    static NativeTaskStep wait(std::shared_ptr<NativeRetailTaskContinuation>, float duration) noexcept;
};
class NativeRetailTaskContinuation {
public:
    virtual ~NativeRetailTaskContinuation() = default;
    virtual NativeTaskStep run() = 0;
};

enum class RetailTaskExecutionError {
    none, wrong_thread, foreign_task, missing_body, unsupported_source_branch,
    unsupported_float, malformed_step, reentrant, after_effect_unknown,
    native_exception
};
struct RetailTaskExecutionFailure {
    RetailTaskExecutionError kind{RetailTaskExecutionError::none};
    const char* operation{ "" };
};
enum class NativeTaskRunKind {
    retained_wait, returned_nonnegative, cleaned_negative,
    cleanup_guarded, invalidated_by_callback
};
struct NativeTaskRunOutcome {
    NativeTaskRunKind kind{};
    float stored_wait{};
    std::uint64_t immediate_calls{};
};
struct NativeTaskExecutionObservation {
    float stored_wait{};
    bool resume_pending{}, running{}, transfer_pending{};
};

// Bounded source 12E2F0/12E410/12E4C0 control path on a real registry task.
// The caller proves 12E670 readiness and owns the registry for this lifetime.
// Only the source initial no-script/flag-zero task branch is admitted here.
class NativeRetailTaskExecution final {
public:
    explicit NativeRetailTaskExecution(NativeRetailTaskRegistry& registry) noexcept;
    NativeRetailTaskExecution(const NativeRetailTaskExecution&) = delete;
    NativeRetailTaskExecution& operator=(const NativeRetailTaskExecution&) = delete;
    std::optional<NativeTaskRunOutcome> invoke_virtual_10_initial(
        const std::shared_ptr<NativeRetailTaskObject>&,
        RetailTaskExecutionFailure&);
    std::optional<NativeTaskExecutionObservation> observe(
        const std::shared_ptr<NativeRetailTaskObject>&,
        RetailTaskExecutionFailure&) const noexcept;
private:
    NativeRetailTaskRegistry& registry_;
};
} // namespace mka::retail
