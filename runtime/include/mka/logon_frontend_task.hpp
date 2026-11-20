#pragma once
#include "native_logon.hpp"
#include "mka/frontend_scheduler.hpp"
#include "mka/game_transition_state.hpp"

namespace mka::native_logon {
// These are required native game dependencies, not default successful effects.
// World teardown must clear native tasks and install its actual maintenance
// body at its source points. The factory supplies the actual current frontend
// continuation AFTER that teardown; no callback address/index is stored here.
struct FrontendEffects {
    Effect prepare_game_world, surround_movie, finish_surround_movie;
    std::function<std::optional<game::FrontendTaskToken>(game::NativeFrontendExecution&, EffectFailure&)> teardown_game_world;
    std::function<std::unique_ptr<game::NativeFrontendTask>(EffectFailure&)> frontend_task;
};
enum class FrontendTaskError { none, missing_effect, allocation, reentry, game_state,
    input, world, frontend, scheduler, movie, missing_handoff, missing_maintenance };
struct FrontendTaskFailure {
    FrontendTaskError kind = FrontendTaskError::none;
    const char* operation = "";
    EffectFailure effect;
    game::GameTransitionFailure game_state;
    input::GameActionFailure input, input_cleanup;
    game::FrontendSchedulerFailure scheduler;
    Failure movie;
};

// Native asynchronous form of the existing blocking retail LOGON task. All
// borrowed game/Service/input owners must outlive it on their creating thread.
// BLOCKED retains the logical scheduler call while the host pumps media/events.
// A successful transition abandons that pass; it never calls the next task.
class NativeLogonFrontendTask final : public game::NativeFrontendTask {
public:
    static std::unique_ptr<NativeLogonFrontendTask> create(
        video::NativeMovieService&, input::NativeGameActionInput&,
        const input::ControllerInput&, input::NativeGameInputTasks&,
        game::NativeGameTransitionState&, Settings, FrontendEffects,
        FrontendTaskFailure&) noexcept;
    ~NativeLogonFrontendTask() override;
    game::TaskUpdate update(game::NativeFrontendExecution&,
        game::FrontendSchedulerFailure&) override;
    // Borrowed observation, valid only while this task is owned and on its
    // owner thread. Native scheduler retirement destroys the task.
    const FrontendTaskFailure& failure() const noexcept { return failure_; }
private:
    NativeLogonFrontendTask(video::NativeMovieService&,
        input::NativeGameActionInput&, const input::ControllerInput&,
        input::NativeGameInputTasks&, game::NativeGameTransitionState&,
        Settings, FrontendEffects);
    bool transition(EffectFailure&) noexcept;
    bool fail(FrontendTaskError, const char*, EffectFailure&) noexcept;
    bool resume_inputs(EffectFailure&, bool primary_failed) noexcept;
    game::TaskUpdate task_error(game::FrontendSchedulerFailure&,
        std::int64_t code = 0) noexcept;
    input::NativeGameActionInput& actions_;
    input::NativeGameInputTasks& input_tasks_;
    game::NativeGameTransitionState& game_;
    FrontendEffects effects_;
    FrontendTaskFailure failure_;
    game::NativeFrontendExecution* execution_ = nullptr;
    bool started_ = false, handoff_ = false;
    std::unique_ptr<NativeLogonState> state_;
};
} // namespace mka::native_logon
