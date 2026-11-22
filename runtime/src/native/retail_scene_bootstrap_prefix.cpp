#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
#include <bit>

namespace mka::retail::main_binding {
namespace {
bool require_main(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure = {StartupError::game_session, "1861B0 Main lifetime/thread"};
        return false;
    }
    if (!main.startup.game_session ||
        !main.startup.game_session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "1861B0 actual retained game Session";
        return false;
    }
    return true;
}
}
bool refresh_retail_timing_factors_122ad0(NativeRetailMainState& main,
    StartupFailure& failure) {
    if (!require_main(main, failure)) return false;
    const auto override_active = retail_timing_override_active(main, failure);
    if (!override_active) return false;
    bool fifty = false;
    if (!*override_active) {
        const auto rate = read_retail_frame_rate(main, failure);
        if (!rate) return false;
        fifty = *rate == 50;
    }
    //122B38/40/4C: binary32 5/6,6/5,sqrt(6/5). The EE-rounded root
    // is0x3F8C378C, already established by the existing Main timing evidence.
    // The array order is root,inverse,scale, matching the existing consumer.
    main.timing_factors = fifty ? std::array<float,3>{
        std::bit_cast<float>(0x3f8c378cu),
        std::bit_cast<float>(0x3f555555u),
        std::bit_cast<float>(0x3f99999au)} : std::array<float,3>{1,1,1};
    return true;
}
bool initialize_retail_post_scene_timing(NativeRetailMainState& main,
    StartupFailure& failure) {
    if (!require_main(main, failure)) return false;
    auto* words = main.startup.game_session->game_records_values(failure.game);
    if (!words) {
        failure.kind = StartupError::game_session;
        failure.operation = "1861B0 persistent source phase owner";
        return false;
    }
    //1861C0 is the122AD0 call delay slot. Keep this effect on ordinary
    // failure or NativeMainReentry; neither rewinds original stores.
    words->word_64e2a0 = 1;
    if (!refresh_retail_timing_factors_122ad0(main, failure)) return false;
    return scene_bootstrap_required::continue_scene_reset_at_181a30(main, failure);
}
}
