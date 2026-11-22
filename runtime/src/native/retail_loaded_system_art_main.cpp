#include "mka/retail_loaded_system_art.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_platform.hpp"
#include "mka/retail_section_workspace.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "native_retail_startup.hpp"
#include <bit>
#include <thread>

namespace mka::retail {
namespace {
std::shared_ptr<NativeRetailGameSession> session_for_art(
    NativeRetailMainState& main, StartupFailure& f) {
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()) {
        f.kind=StartupError::game_session; f.operation="loaded art actual Main lifetime/thread"; return {};
    }
    const auto session=main.startup.game_session;
    if(!session||!session->validate_current_thread(f.game)) {
        f.kind=StartupError::game_session; f.operation="loaded art actual Session"; return {};
    }
    return session;
}
bool art_failure(StartupFailure& f, const RetailLoadedArtFailure& detail) {
    f.kind=StartupError::section_workspace; f.operation=detail.operation; return false;
}
}
namespace main_binding {
bool initialize_retail_section_service(NativeRetailMainState& main, StartupFailure& f) {
    const auto session=session_for_art(main,f); if(!session) return false;
    if(!main.package||!main.startup.section_workspace) {
        f.kind=StartupError::section_workspace; f.operation="19A630 actual package/section owners"; return false;
    }
    RetailLoadedArtFailure af;
    const auto sections=NativeRetailLoadedSystemArt::for_session(session,af);
    if(!sections||!sections->queue_from_19b530(
        *main.package,std::bit_cast<std::int32_t>(main.post_overlay_state),af)) return art_failure(f,af);
    // Family0 contains the actual logical10 sysart subrange and is shared by
    // table0/3. Existing selection refuses any unported populated retirement.
    if(!main.startup.section_workspace->mark_loaded_subrecords_unbound_from_source(0,f.section_workspace)) {
        f.kind=StartupError::section_workspace; f.operation="19A630 actual family0 loaded association"; return false;
    }
    return loaded_art_required::queue_permanent_strings_215660(main,f);
}
bool initialize_retail_post_scene_sections(NativeRetailMainState& main, StartupFailure& f) {
    const auto session=session_for_art(main,f); if(!session) return false;
    const auto sections=session->loaded_system_art(f.game);
    if(!sections||!main.startup.platform) {
        f.kind=StartupError::section_workspace; f.operation="19A5E0 actual queued section/platform owners"; return false;
    }
    auto device=main.startup.platform->texture_upload_device(f.platform);
    if(!device) { f.kind=StartupError::platform; f.operation="19A5E0 actual platform image context"; return false; }
    RetailLoadedArtFailure af;
    for(;;) {
        const auto step=sections->advance_from_19aa30(device,af);
        if(!step) return art_failure(f,af);
        if(*step==RetailLoadedArtStep::complete) break;
        // This is the original Main startup call before task creation. The
        // no-current-task3DDEF0 path waits for native IO; no guest scheduler.
        std::this_thread::yield();
    }
    if(main.startup.game_session!=session||!main.lifetime->active_on_owner_thread()) {
        f.kind=StartupError::game_session; f.operation="19A5E0 retained Main/Session after conversion"; return false;
    }
    return loaded_art_required::complete_permanent_strings_215730(main,f);
}
}
namespace initial_reset_required {
bool configure_selected_resource_zero_16fc70(NativeRetailMainState& main, StartupFailure& f) {
    const auto session=session_for_art(main,f); if(!session) return false;
    const auto sections=session->loaded_system_art(f.game);
    if(!sections) {
        f.kind=StartupError::section_workspace; f.operation="16FC70 actual loaded-section owner missing"; return false;
    }
    RetailLoadedArtFailure af;
    if(!sections->configure_zero_from_16fc70(af)) return art_failure(f,af);
    if(main.startup.game_session!=session||!main.lifetime->active_on_owner_thread()) {
        f.kind=StartupError::game_session; f.operation="16FC70 retained Main/Session after native sampler"; return false;
    }
    return true; // Includes the verified empty slot / texture-with-null-MET cases.
}
}
} // namespace mka::retail
