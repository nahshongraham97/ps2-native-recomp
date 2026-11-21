#include "native_retail_startup.hpp"
namespace mka::retail {
bool initialize_native_retail_game_session(NativeRetailStartupState& state, StartupFailure& failure) {
    failure = {};
    // Initialized data and BSS values belong to the game lifetime, not a new
    // main invocation. Reentry retains the exact same owners and RNG phase.
    if (state.game_session) {
        if (!state.game_session->validate_current_thread(failure.game)) {
            failure.kind = StartupError::game_session;
            failure.operation = "native game session existing owner validation";
            return false;
        }
        if (!state.game_record_reset) {
            auto reset = NativeRetailGameRecordReset::create(state.game_session, failure.game_record_reset);
            if (!reset) {
                failure.kind = StartupError::game_record_reset;
                failure.operation = "native game-record reset owner birth";
                return false;
            }
            state.game_record_reset = std::move(reset);
        }
        return true;
    }
    auto session = NativeRetailGameSession::create(failure.game);
    if (!session) {
        failure.kind = StartupError::game_session;
        failure.operation = "native game session initialization";
        return false;
    }
    auto reset = NativeRetailGameRecordReset::create(session, failure.game_record_reset);
    if (!reset) {
        failure.kind = StartupError::game_record_reset;
        failure.operation = "native game-record reset owner birth";
        return false;
    }
    state.game_session = std::move(session);
    state.game_record_reset = std::move(reset);
    return true;
}
} // namespace mka::retail
