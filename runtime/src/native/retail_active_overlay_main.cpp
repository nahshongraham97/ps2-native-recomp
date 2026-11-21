#include "mka/retail_main.hpp"
#include "mka/retail_active_overlay.hpp"
#include "native_retail_startup.hpp"
#include <utility>

namespace mka::retail::main_binding {
bool initialize_retail_active_overlay(NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    auto& startup = main.startup;
    const auto session = startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "3DDA60 native game owner unavailable";
        return false;
    }
    if(!startup.memory_owners) {
        failure.kind=StartupError::memory_owners;
        failure.operation="3DDA70 actual native Wave owner missing";
        return false;
    }
    auto publish=[&]() {
        auto parent=startup.active_overlay->retained_companion_parent(failure.active_overlay);
        if(!parent) {
            failure.kind=StartupError::active_overlay;
            failure.operation="3DDA70 current native overlay parent";
            return false;
        }
        if(!startup.memory_owners->publish_wave_overlay_parent(std::move(parent),failure.memory)) {
            failure.kind=StartupError::memory_owners;
            failure.operation="3DDA70 native Wave current-overlay publication";
            return false;
        }
        return true;
    };
    if (startup.active_overlay) {
        if (startup.active_overlay->reload_mode_one(startup.content, failure.active_overlay))
            return publish();
    } else {
        auto overlay = NativeRetailActiveOverlay::initialize_mode_one(
            startup.content, failure.active_overlay);
        if (overlay) {
            startup.active_overlay = std::move(overlay);
            return publish();
        }
    }
    failure.kind = StartupError::active_overlay;
    failure.operation = "3DDA60 native GAMER initializer or overlay owner";
    return false;
}
} // namespace mka::retail::main_binding
