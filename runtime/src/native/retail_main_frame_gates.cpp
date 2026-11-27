#include "mka/retail_main.hpp"
#include "mka/retail_mode_scene.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
namespace {
std::shared_ptr<NativeRetailGameSession> require_frame_session(
    const NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure = {StartupError::game_session, "Main frame gate lifetime/thread"};
        return {};
    }
    auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "Main frame gate actual Session unavailable";
        return {};
    }
    return session;
}
}

std::optional<bool> retail_frame_processing_inhibited(
    const NativeRetailMainState& main, StartupFailure& failure) {
    const auto session = require_frame_session(main, failure);
    if (!session) return {};
    const auto controls = session->controls(failure.game);
    if (!controls) {
        failure.kind = StartupError::game_session;
        failure.operation = "Main frame gate actual controls unavailable";
        return {};
    }
    const auto value = controls->task_dispatch_inhibited_bit5(failure.game.controls);
    if (!value) {
        failure.kind = StartupError::game_session;
        failure.operation = "Main1229CC/122A54 live game-info byte1 bit5";
    }
    return value;
}

std::optional<bool> retail_ordinary_counter_inhibited(
    const NativeRetailMainState& main, StartupFailure& failure) {
    const auto session = require_frame_session(main, failure);
    if (!session) return {};
    auto* tasks = session->task_domain(failure.game);
    if (!tasks) {
        failure.kind = StartupError::game_session;
        failure.operation = "Main ordinary counter actual task domain unavailable";
        return {};
    }
    const auto value = tasks->root_wait_inhibited(failure.game.task_domain);
    if (!value) {
        failure.kind = StartupError::game_session;
        failure.operation = "Main1229E4 actual task root-wait gate";
    }
    return value;
}

std::optional<bool> retail_render_enabled(
    const NativeRetailMainState& main, StartupFailure& failure) {
    const auto session = require_frame_session(main, failure);
    if (!session) return {};
    if(main.mode_scene_routes&&main.mode_scene_routes->render_pending())return false;
    auto* cameras = session->render_cameras(failure.game);
    if (!cameras) {
        failure.kind = StartupError::game_session;
        failure.operation = "Main render gate actual camera/world domain unavailable";
        return {};
    }
    RetailCameraWorldFailure camera_failure;
    const auto value = cameras->observe(camera_failure);
    if (!value) {
        failure.kind = StartupError::game_session;
        failure.operation = camera_failure.operation;
        return {};
    }
    //122A90 reads the actual current-world pointer; suspension is distinct.
    return value->has_world;
}
} // namespace mka::retail::main_binding
