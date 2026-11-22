#include "mka/retail_main.hpp"
#include "mka/retail_renderer_allocator.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_220900(NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()||!main.startup.game_session) {
        f={StartupError::game_session,"220900 actual Main lifetime/thread/Session"};return RetailStartupGate::native_failure;
    }
    auto* domain=main.startup.game_session->render_cameras(f.game);
    RetailCameraWorldFailure cf;
    const auto observed=domain?domain->observe(cf):std::optional<RetailRenderCameraObservation>{};
    if(!observed||!observed->count_producer_admitted) {
        f={StartupError::game_session,"220900 actual127260 count producer unavailable"};return RetailStartupGate::native_failure;
    }
    if(!main.startup.memory_owners) {
        f={StartupError::memory_owners,"220900 actual native memory owner group missing"};return RetailStartupGate::native_failure;
    }
    RetailRendererAllocatorFailure detail;
    auto service=NativeRetailRendererAllocatorService::create(*main.startup.memory_owners,detail);
    if(!service) {
        f.kind=detail.kind==RetailRendererAllocatorError::allocation?StartupError::allocation:
            detail.kind==RetailRendererAllocatorError::native_exception?StartupError::native_exception:StartupError::memory_owners;
        f.operation=detail.operation;f.memory.heap=detail.heap;return RetailStartupGate::native_failure;
    }
    // Source next passes this allocation service into1A6DD0. Its1MiB RW
    // arena becomes dynamic host ownership,never a runtime capacity argument.
    return continue_first_scene_at_1a6dd0(main,std::move(service),f);
}
}
