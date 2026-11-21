#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {

bool initialize_retail_game_options(NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    const auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "164130 retained native game session unavailable";
        return false;
    }
    auto* options = session->game_options(failure.game);
    if (!options) {
        failure.kind = StartupError::game_session;
        failure.operation = "164130 retained game option owner unavailable";
        return false;
    }
    if (!options->publish_six_words_from_164130(failure.game.options)) {
        failure.game.kind = RetailGameSessionError::options;
        failure.kind = StartupError::game_session;
        failure.operation = "164130 game option publication";
        return false;
    }
    return true;
}

} // namespace mka::retail::main_binding
