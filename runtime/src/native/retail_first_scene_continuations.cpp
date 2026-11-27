#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_movie_controller.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::scene_bootstrap_required {

// NOTE / AUDIT: continue_first_scene_at_225d40 is currently a native link diagnostic
// placeholder that checks lifecycle and constructs registered plugins, but does NOT
// execute authentic retail 0x00225d40 (callback registration into gp) or retail EngineOpen/Start.
// It is relabeled here as an unrecovered placeholder, so that symbol resolution is recognized
// as satisfying link diagnostics rather than recovered game behavior.
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState& main,
    std::shared_ptr<NativeRetailRendererCore> core, StartupFailure& f) {
    record_placeholder_hit(0, "continue_first_scene_at_225d40", main.iteration);
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread() || !main.startup.game_session) {
        f = {StartupError::game_session, "225D40 placeholder: Main lifetime/thread/Session required"};
        return RetailStartupGate::native_failure;
    }
    if (!core) {
        f = {StartupError::memory_owners, "225D40 placeholder: null renderer core"};
        return RetailStartupGate::native_failure;
    }

    RetailRendererCoreFailure cf;
    if (!core->construct_registered(cf)) {
        f.kind = StartupError::memory_owners;
        f.operation = cf.operation;
        return RetailStartupGate::native_failure;
    }
    if (!core->world_module(cf) || !core->textures(cf) || !core->materials(cf)) {
        f.kind = StartupError::memory_owners;
        f.operation = cf.operation;
        return RetailStartupGate::native_failure;
    }

    // PS2 retail 0x00225d40 sets callback 0x00225d50 into -23540($gp).
    // Real retail startup then proceeds to 0x001a6c30 (EngineOpen) and 0x001a6b30 (EngineStart).
    return RetailStartupGate::ready;
}

// NOTE / AUDIT: continue_scene_reset_at_125d60 is currently a native link diagnostic
// placeholder that detaches cameras via 181A30, but does not replicate retail 0x00125d60
// (which zeros 5 global words: 0x005ab560, 0x005ab564, -25144($gp), 0x005ab568, 0x005ab56c).
// It is relabeled here as an unrecovered placeholder.
bool continue_scene_reset_at_125d60(NativeRetailMainState& main, StartupFailure& f) {
    record_placeholder_hit(1, "continue_scene_reset_at_125d60", main.iteration);
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread() || !main.startup.game_session) {
        f = {StartupError::game_session, "125D60 placeholder: Main lifetime/thread/Session required"};
        return false;
    }
    RetailGameSessionFailure gf;
    RetailCameraWorldFailure cf;
    auto* cameras = main.startup.game_session->render_cameras(gf);
    if (cameras) {
        cameras->detach_cameras_from_181a30(cf);
    }
    return true;
}

} // namespace mka::retail::scene_bootstrap_required
