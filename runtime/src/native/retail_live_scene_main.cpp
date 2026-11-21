#include "mka/retail_profile_menu.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail {
std::optional<bool> observe_retail_profile_live_scene_from_2f18f0(
    NativeRetailMainState& main,RetailProfileMenuFailure& failure) {
    failure={};
    if(!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind=RetailProfileMenuError::main_lifetime;
        failure.operation="2F18F0 native Main lifetime/thread";
        return std::nullopt;
    }
    const auto session=main.startup.game_session;
    if(!session) {
        failure.kind=RetailProfileMenuError::session;
        failure.operation="2F18F0 native retained Session missing";
        return std::nullopt;
    }
    auto* reference=session->live_scene_reference(failure.session);
    if(!reference) {
        failure.kind=RetailProfileMenuError::session;
        failure.operation="2F18F0 actual live scene-reference owner";
        return std::nullopt;
    }
    const auto scene=reference->lookup_from_2f18f0(failure.scene_reference);
    if(!scene) {
        failure.kind=RetailProfileMenuError::scene_reference;
        failure.operation=failure.scene_reference.operation;
        return std::nullopt;
    }
    return bool(*scene);
}
} // namespace mka::retail
