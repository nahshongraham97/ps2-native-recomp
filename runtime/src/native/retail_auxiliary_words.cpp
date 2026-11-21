#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool reset_retail_auxiliary_words(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    // 26F1E0: first zero store, return, second zero store in its delay slot.
    // Neighboring transfer/resource owners and the PRNG are not reset here.
    const auto session = main.startup.game_session;
    if (!session) {
        failure = {StartupError::game_session, "native auxiliary word owner missing"};
        return false;
    }
    auto* words = session->auxiliary_words(failure.game);
    if (!words) {
        failure.kind = StartupError::game_session;
        failure.operation = "native auxiliary word owner unavailable";
        return false;
    }
    words->first = 0;
    words->second = 0;
    return true;
}
} // namespace mka::retail::main_binding
