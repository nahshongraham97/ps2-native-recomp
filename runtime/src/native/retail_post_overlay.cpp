#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool initialize_retail_post_overlay_state(NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    const auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "182E50 native game owner unavailable";
        return false;
    }
    // 182E58 discards 2202A0's language query. Its only persistent writes
    // populate a private PS2 firmware cache (295E10 -> 295DD0 -> 295D60).
    // That removed platform backend and ROMVER cache have no native object;
    // the caller consumes neither the language result nor the cache bytes.
    // Preserve the actual game write at 182E60 in this same Main lifetime.
    main.post_overlay_state = 0;
    return true;
}
} // namespace mka::retail::main_binding
