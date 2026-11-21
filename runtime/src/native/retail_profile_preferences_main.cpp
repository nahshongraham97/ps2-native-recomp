#include "mka/retail_profile_preferences.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool initialize_retail_current_profile_preferences(NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    if (!main.startup.game_session) {
        failure.kind = StartupError::game_session;
        failure.operation = "295660 native current preference Session";
        return false;
    }
    auto* values = main.startup.game_session->game_records_values(failure.game);
    if (!values) {
        failure.kind = StartupError::game_session;
        failure.operation = "295660 actual retained current preferences";
        return false;
    }
    values->profile_preferences = retail_profile_preference_defaults_from_2955d0();
    // Keep the two post-copy stores in their original order.
    values->word_64eac4 = 0;
    values->word_64eac0 = 0xffffffffu;
    return true;
}
} // namespace mka::retail::main_binding
