#include "mka/logon_frontend_task.hpp"
#include <new>
#include <stdexcept>
#include <type_traits>

namespace mka::native_logon {
namespace {
struct ExecutionScope {
    game::NativeFrontendExecution*& slot;
    ExecutionScope(game::NativeFrontendExecution*& s, game::NativeFrontendExecution& e) noexcept : slot(s) { slot = &e; }
    ~ExecutionScope() { slot = nullptr; }
};
}

NativeLogonFrontendTask::NativeLogonFrontendTask(video::NativeMovieService& service,
    input::NativeGameActionInput& actions, const input::ControllerInput& host,
    input::NativeGameInputTasks& tasks, game::NativeGameTransitionState& state,
    Settings settings, FrontendEffects effects)
    : actions_(actions), input_tasks_(tasks), game_(state), effects_(std::move(effects)) {
    Effects bound{effects_.prepare_game_world, effects_.surround_movie,
        effects_.finish_surround_movie, [this](EffectFailure& f) { return transition(f); }};
    state_ = std::make_unique<NativeLogonState>(service, actions, host, tasks, settings, std::move(bound));
}
NativeLogonFrontendTask::~NativeLogonFrontendTask() = default;

std::unique_ptr<NativeLogonFrontendTask> NativeLogonFrontendTask::create(
    video::NativeMovieService& service, input::NativeGameActionInput& actions,
    const input::ControllerInput& host, input::NativeGameInputTasks& tasks,
    game::NativeGameTransitionState& state, Settings settings, FrontendEffects effects,
    FrontendTaskFailure& f) noexcept {
    f = {};
    if (!effects.prepare_game_world || !effects.surround_movie || !effects.finish_surround_movie ||
        !effects.teardown_game_world || !effects.frontend_task) {
        f.kind = FrontendTaskError::missing_effect; f.operation = "native LOGON unbound frontend game dependency"; return {};
    }
    if (!state.current_context(f.game_state)) {
        f.kind = FrontendTaskError::game_state; f.operation = "native LOGON frontend game-state ownership"; return {};
    }
    if (!actions.validate_logon_bindings(f.input) || !actions.validate_post_movie_targets(f.input)) {
        f.kind = FrontendTaskError::input; f.operation = "native LOGON frontend action ownership"; return {};
    }
    if (!tasks.validate_access(f.input.tasks)) {
        f.kind = FrontendTaskError::input; f.operation = "native LOGON frontend input-task ownership"; return {};
    }
    try { return std::unique_ptr<NativeLogonFrontendTask>(new NativeLogonFrontendTask(service, actions, host, tasks, state, settings, std::move(effects))); }
    catch (const std::bad_alloc&) { f.kind = FrontendTaskError::allocation; f.operation = "native LOGON frontend allocation"; }
    catch (const std::length_error&) { f.kind = FrontendTaskError::allocation; f.operation = "native LOGON frontend extent"; }
    return {};
}

bool NativeLogonFrontendTask::fail(FrontendTaskError kind, const char* operation, EffectFailure& f) noexcept {
    failure_.kind = kind; failure_.operation = operation;
    f.operation = operation; return false;
}

bool NativeLogonFrontendTask::resume_inputs(EffectFailure& f, bool primary_failed) noexcept {
    if (actions_.resume_source_inputs(input_tasks_, failure_.input_cleanup)) return !primary_failed;
    if (primary_failed) return false;
    return fail(FrontendTaskError::input, "native LOGON frontend source-input resume", f);
}

bool NativeLogonFrontendTask::transition(EffectFailure& f) noexcept {
    f = {};
    if (!execution_) return fail(FrontendTaskError::missing_handoff, "native LOGON transition outside scheduler callback", f);
    // The source caller already performed its first suspension. This is the
    // second, distinct call in the source transition; do not deduplicate it.
    if (!game_.enter_transition(failure_.game_state))
        return fail(FrontendTaskError::game_state, "native LOGON transition reset/push context1", f);
    if (!actions_.suspend_after_logon(input_tasks_, failure_.input))
        return fail(FrontendTaskError::input, "native LOGON second source-input suspension", f);
    if (!game_.set_transition_selector(0, failure_.game_state))
        return fail(FrontendTaskError::game_state, "native LOGON publish transition selector0", f);
    std::optional<game::FrontendTaskToken> maintenance;
    try {
        EffectFailure world;
        maintenance = effects_.teardown_game_world(*execution_, world);
        if (world.code != 0 || !world.operation || *world.operation) {
            failure_.effect = world; f = world;
            return fail(FrontendTaskError::world, "native LOGON required world teardown", f);
        }
    } catch (const std::bad_alloc&) {
        return fail(FrontendTaskError::allocation, "native LOGON world teardown allocation", f);
    } catch (...) { return fail(FrontendTaskError::world, "native LOGON world teardown exception", f); }
    auto pending = execution_->transition_pending(failure_.scheduler);
    if (!pending || !*pending)
        return fail(FrontendTaskError::missing_handoff, "native LOGON world did not clear native task ownership", f);
    if (!maintenance || !execution_->validate_transition_task(*maintenance, 16, failure_.scheduler))
        return fail(FrontendTaskError::missing_maintenance, "native LOGON world did not register owned priority16 maintenance", f);
    // Read the current game mode after teardown, not a construction snapshot.
    if (!game_.apply_post_world_transition(failure_.game_state))
        return fail(FrontendTaskError::game_state, "native LOGON post-world render-view selection", f);

    // The source always resumes inputs after attempting frontend allocation,
    // including null allocation. Preserve that cleanup and a distinct cause.
    bool queued = false;
    try {
        EffectFailure next;
        auto task = effects_.frontend_task(next);
        if (!task || next.code != 0 || !next.operation || *next.operation) {
            failure_.effect = next; f = next;
            fail(FrontendTaskError::frontend, "native LOGON required next frontend task", f);
        } else if (!execution_->enqueue(task, 32, false, failure_.scheduler)) {
            fail(FrontendTaskError::scheduler, "native LOGON frontend enqueue", f);
        } else queued = true;
    } catch (const std::bad_alloc&) {
        fail(FrontendTaskError::allocation, "native LOGON frontend allocation", f);
    } catch (...) { fail(FrontendTaskError::frontend, "native LOGON frontend factory exception", f); }
    if (!resume_inputs(f, !queued)) return false;
    handoff_ = true; return true;
}

game::TaskUpdate NativeLogonFrontendTask::task_error(game::FrontendSchedulerFailure& f, std::int64_t code) noexcept {
    f.kind = game::FrontendSchedulerError::invalid_result;
    f.operation = failure_.operation; f.task_code = code ? code : failure_.effect.code;
    f.input = failure_.input; return game::TaskUpdate::error;
}

game::TaskUpdate NativeLogonFrontendTask::update(game::NativeFrontendExecution& execution,
    game::FrontendSchedulerFailure& f) {
    f = {};
    if (execution_) {
        // Reject the nested call through its own result; a caller may handle
        // that rejection without corrupting the still-running outer update.
        f.kind = game::FrontendSchedulerError::invalid_state;
        f.operation = "native LOGON queued task reentry"; return game::TaskUpdate::error;
    }
    ExecutionScope active(execution_, execution);
    auto pending = execution.transition_pending(failure_.scheduler);
    if (!pending || *pending) {
        failure_.kind = FrontendTaskError::scheduler; failure_.operation = "native LOGON invalid live execution";
        return task_error(f);
    }
    Failure movie;
    if (!started_) {
        if (!state_->native_logon_enter(movie)) {
            failure_.kind = FrontendTaskError::movie; failure_.operation = movie.operation;
            failure_.movie = std::move(movie); return task_error(f);
        }
        started_ = true;
    }
    auto result = state_->poll(movie);
    if (!result || result->progress == video::MovieControlProgress::error || movie.kind != Error::none) {
        if (failure_.kind == FrontendTaskError::none) {
            failure_.kind = FrontendTaskError::movie; failure_.operation = movie.operation;
        }
        failure_.movie = std::move(movie);
        return task_error(f, result ? result->failure.native_code : 0);
    }
    if (result->progress == video::MovieControlProgress::ongoing) return game::TaskUpdate::blocked;
    if (!handoff_) {
        failure_.kind = FrontendTaskError::missing_handoff; failure_.operation = "native LOGON missing terminal transition";
        return task_error(f);
    }
    return game::TaskUpdate::transition;
}
static_assert(std::is_nothrow_move_assignable_v<Failure>);
} // namespace mka::native_logon
