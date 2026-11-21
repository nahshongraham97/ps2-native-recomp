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
    if (startup.active_overlay) {
        if (startup.active_overlay->reload_mode_one(startup.content, failure.active_overlay))
            return true;
    } else {
        auto overlay = NativeRetailActiveOverlay::initialize_mode_one(
            startup.content, failure.active_overlay);
        if (overlay) {
            startup.active_overlay = std::move(overlay);
            return true;
        }
    }
    failure.kind = StartupError::active_overlay;
    failure.operation = "3DDA60 native GAMER initializer or overlay owner";
    return false;
}
} // namespace mka::retail::main_binding
