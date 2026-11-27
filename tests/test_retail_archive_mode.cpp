#include "../runtime/src/native/retail_main_control.hpp"
#include "mka/retail_mode_scene.hpp"
#include "retail_scene_archive_fixtures.hpp"
#include <stdexcept>
#include <algorithm>
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_scene_owners.hpp"
#include "mka/retail_display_service.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_platform.hpp"
#include "mka/retail_renderer_allocator.hpp"
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/content_volume.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif

namespace {

mka::retail::NativeRetailMainState* transfer_main{};
bool transfer_constructor(mka::retail::NativeRetailRendererCore&,mka::retail::RetailRendererCoreFailure&){
    mka::retail::reenter_native_retail_main(*transfer_main,23);
}
void transfer_destructor(mka::retail::NativeRetailRendererCore&) noexcept {}
std::size_t mode_checks{};
void check(bool ok, const char* msg) {
    ++mode_checks;
    if(!ok)throw std::runtime_error(msg);
}

void write_bytes(const std::filesystem::path& p, const std::vector<std::byte>& bytes) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(out.good(), "write test asset file");
}

struct PipelineHarness {
    mka::retail::NativeRetailStartupState startup;
    mka::input::ControllerInput input;
    std::unique_ptr<mka::retail::NativeRetailMainState> main_state;
    std::shared_ptr<mka::retail::NativeRetailRendererCore> core;

    explicit PipelineHarness(const std::filesystem::path& vol_root)
        : startup([&] {
              mka::content::ContentVolumeFailure cf;
              auto vol = mka::content::NativeContentVolume::open(vol_root, cf);
              check(vol.has_value(), "open content volume");
              return mka::retail::NativeRetailStartupState(std::move(*vol), {"cdrom0:\\SLUS_214.10"});
          }()) {
        mka::retail::StartupFailure sf;
        check(mka::retail::initialize_native_retail_game_session(startup, sf), "initialize game session");
        check(mka::retail::initialize_retail_memory_owners(startup, sf), "initialize retail memory owners");

        main_state = std::make_unique<mka::retail::NativeRetailMainState>(startup, input);
        mka::retail::RetailRendererAllocatorFailure af;
        auto alloc = mka::retail::NativeRetailRendererAllocatorService::create(*startup.memory_owners, af);
        check(alloc != nullptr, "create renderer allocator");
        mka::retail::RetailRendererCoreFailure cf;
        core = mka::retail::NativeRetailRendererCore::for_session(
            startup.game_session, std::move(alloc), startup.content, cf);
        check(core != nullptr, "create renderer core");
        auto init = core->initialize_core(cf);
        check(init && *init, "initialize renderer core");

        auto* world_cams = startup.game_session->render_cameras(sf.game);
        check(world_cams != nullptr, "get world_cams");
        mka::retail::RetailCameraWorldFailure cwf;
        check(world_cams->store_initial_count_from_127260(cwf), "admit 127260 camera count");
    }
};

} // namespace

int main(int argc,char**argv) try {
    using namespace mka::retail;
    namespace fixture=mka::test::scene_archive;
    check(argc==2,"fresh mode GPU evidence directory required");
    const auto base=std::filesystem::absolute(argv[1]).lexically_normal();
    check(!std::filesystem::exists(base),"preserve earlier mode GPU fixtures/evidence");
    std::filesystem::create_directories(base/"volume");
    auto archive=fixture::pak({{"scene.txd",fixture::txd()},{"arena.bsp",fixture::world()},
                              {"fighter.dff",fixture::geometry()}});
    write_bytes(base/"volume"/"modes.pak",archive);
    auto invalid=archive;
    // The first payload starts at20. Its record lives at header.table+0.
    auto u32=[](const std::vector<std::byte>& b,std::size_t off){
        std::uint32_t x{};for(unsigned i=0;i<4;++i)x|=std::uint32_t(std::to_integer<unsigned char>(b.at(off+i)))<<(8*i);return x;
    };
    const auto toc=u32(invalid,12);
    for(unsigned i=0;i<4;++i)invalid[toc+4+i]=std::byte(0xff);
    write_bytes(base/"volume"/"invalid.pak",invalid);
#ifdef _WIN32
    DisplayServiceFailure df;
    auto display=NativeOffscreenDisplayService::create(64,48,df);
    check(display&&display->acquire_context(df),"actual native hidden display context");
    const auto vendor=glGetString(GL_VENDOR),renderer=glGetString(GL_RENDERER),version=glGetString(GL_VERSION);
    check(vendor&&renderer&&version,"query actual native GL driver");
    std::printf("ARCHIVE_MODE_GL vendor=%s renderer=%s version=%s\n",reinterpret_cast<const char*>(vendor),reinterpret_cast<const char*>(renderer),reinterpret_cast<const char*>(version));
    mka::video::MovieGpuError ge;
    auto retirement=mka::video::NativeMovieGpuRetirementOwner::create(ge);
    check(retirement!=nullptr,"explicit mode GPU retirement host");
    PipelineHarness h(base/"volume");
    RetailRendererCoreFailure core_failure;
    check(h.core->construct_registered(core_failure),"construct actual native engine modules before baseline measurement");
    const auto textures=h.core->textures(core_failure);
    const auto materials=h.core->materials(core_failure);
    const auto rasters=h.core->rasters(core_failure);
    const auto initial_textures=textures->active_count(),initial_materials=materials->active_count();
    h.main_state->mode_history.emplace();
    RetailModeFailure mf;
    check(h.main_state->mode_history->push(22,mf),"preexisting independent interaction history");
    RetailArchiveSceneStreams selection;
    selection.archive_resource_key="modes.pak";
    selection.txd=RetailArchiveMemberSelector{std::string("scene.txd"),{}};
    selection.world=RetailArchiveMemberSelector{{},std::size_t(1)};
    selection.geometry=RetailArchiveMemberSelector{std::string("fighter.dff"),{}};
    RetailFirstSceneConfig config;
    config.display=display;config.gpu_retirement=retirement;config.archive_streams=selection;
    config.viewport_width=64;config.viewport_height=48;config.call_show_raster=false;
    config.clear_color={0,0,0,1};
    config.hud=RetailHudSubmission{64,48,{{{1,1,8,8},{0,0,1,1},{0,1,0,.5f},{}}}};
    auto second=config;second.hud->quads[0].color={1,0,0,.5f};
    auto bad=config;bad.archive_streams->archive_resource_key="invalid.pak";
    {
        PipelineHarness transfer_case(base/"volume");
        RetailRendererCoreFailure cf;
        check(bool(transfer_case.core->register_factory({0x7f01,0},{transfer_constructor,transfer_destructor},cf)),
              "explicit test-only native transfer boundary");
        StartupFailure transfer_failure;
        check(NativeRetailModeSceneRoutes::install(*transfer_case.main_state,transfer_case.core,
            {{7,RetailInitialTransitionBody::source_245930,23,config}},transfer_failure),"install transfer test presentation route");
        transfer_main=transfer_case.main_state.get();
        initial_task_required::transition_1225d0(*transfer_case.main_state,7,RetailInitialTransitionBody::source_245930);
        bool transferred{};
        try{(void)main_binding::dispatch_retail_enabled_mode(*transfer_case.main_state,transfer_failure);}
        catch(const detail::NativeMainReentry& value){transferred=value.owner==transfer_main&&value.value==23;}
        transfer_main=nullptr;
        check(transferred&&transfer_failure.kind==StartupError::none,"native Main transfer propagates through scene bootstrap and mode dispatcher");
        check(transfer_case.core->destroy_constructed(cf),"actual constructed prefix remains owned and releasable after transfer");
    }
    StartupFailure sf;
    check(NativeRetailModeSceneRoutes::install(*h.main_state,h.core,{
        {7,RetailInitialTransitionBody::source_245930,23,config},
        {4,RetailInitialTransitionBody::menu_18db60,30,second},
        {9,RetailInitialTransitionBody::source_245930,24,bad}},sf),
        "install explicit native presentation routes");
    initial_task_required::transition_1225d0(*h.main_state,7,RetailInitialTransitionBody::source_245930);
    check(h.main_state->mode_history->top()==1,"transition context resets to1; does not store game mode7 in history");
    check(h.main_state->mode_scene_routes->render_pending(),"transition queues selected native scene");
    check(main_binding::dispatch_retail_enabled_mode(*h.main_state,sf),"dispatch selected archive mode");
    check(h.main_state->scene&&h.main_state->scene->scene_loaded,"mode dispatch loaded actual selected archive scene");
    check(h.main_state->mode_scene_routes->active_mode()==7&&h.main_state->mode_scene_routes->active_stage()==23,
          "transition mode and dispatch selector remain distinct native values");
    check(h.main_state->scene->world->bsp_sectors().size()==1&&h.main_state->scene->world->scene_objects().size()==1,
          "both BSP world and DFF geometry loaded through named/indexed TOC entries");
    auto pose=[&]{
        RetailRendererFrameFailure ff;
        RetailRendererMatrixValue camera{{{1,0,0},{0,1,0},{0,0,1},{0,0,-4}},0x20000u};
        check(h.main_state->scene->frames->set_modelling(h.main_state->scene->camera_init->primary.frame,camera,ff),
              "bind explicit native camera test pose");
        auto obj=h.main_state->scene->world->scene_objects().front().frame;
        RetailRendererMatrixValue model{{{1,0,0},{0,1,0},{0,0,1},{.8f,0,.5f}},0x20000u};
        check(h.main_state->scene->frames->set_modelling(obj,model,ff),"separate clump from world in constructed GPU case");
    };
    pose();
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ZERO);glColor4f(.2f,.3f,.4f,.5f);
    GLfloat prior_color[4]{};glGetFloatv(GL_CURRENT_COLOR,prior_color);
    check(main_binding::render_retail_frame(*h.main_state,sf),"mode scene submits textured3D and HUD");
    GLfloat after_color[4]{};glGetFloatv(GL_CURRENT_COLOR,after_color);
    GLboolean mask{};glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
    check(!glIsEnabled(GL_DEPTH_TEST)&&mask==GL_FALSE&&glIsEnabled(GL_BLEND),"frame restores native depth/blend enables and depth mask");
    check(std::equal(std::begin(prior_color),std::end(prior_color),std::begin(after_color)),"frame restores actual current vertex color");
    check(h.main_state->scene->frames_rendered==1,"exactly one native world submission");
    auto read=[&](const char* tag,bool green){
        RetailRendererCameraFailure cf;
        const auto& scene=h.main_state->scene;
        check(scene->cameras->begin_update(scene->camera_init->primary.camera,cf),"native target bind for diagnostic readback");
        std::vector<unsigned char> rgba(64*48*4);std::vector<float> depth(64*48);
        glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());display->record_diagnostic_readback(2);
        check(scene->cameras->end_update(scene->camera_init->primary.camera,cf),"native target end after diagnostic readback");
        std::size_t gold{},overlay{};
        for(std::size_t i=0;i<64*48;++i)
            if(rgba[i*4]>220&&rgba[i*4+1]>170&&rgba[i*4+2]<30&&depth[i]<.99f)++gold;
        for(unsigned y=41;y<46;++y)for(unsigned x=2;x<7;++x){
            const auto i=y*64+x;
            const unsigned c=green?1:0,other=green?0:1;
            if(std::abs(int(rgba[i*4+c])-128)<=2&&rgba[i*4+other]<2&&rgba[i*4+2]<2&&depth[i]==1.0f)++overlay;
        }
        check(gold>20,"textured archive world/clump produces measured color and depth");
        check(overlay==25,"HUD straight-alpha pixels appear above3D without depth writes");
        std::ofstream color(base/(std::string(tag)+".rgba8"),std::ios::binary);
        color.write(reinterpret_cast<const char*>(rgba.data()),std::streamsize(rgba.size()));
        std::ofstream z(base/(std::string(tag)+".depth32f"),std::ios::binary);
        z.write(reinterpret_cast<const char*>(depth.data()),std::streamsize(depth.size()*sizeof(float)));
        check(color.good()&&z.good(),"preserve constructed GPU diagnostic frames");
        std::printf("ARCHIVE_MODE_PIXELS %s gold=%zu hud=%zu\n",tag,gold,overlay);
    };
    read("mode7",true);
    check(!h.main_state->scene->world->gpu_texture_cache().empty(),"actual native scene texture allocation");
    const GLuint old_id=h.main_state->scene->world->gpu_texture_cache().front().gl_texture_id;
    auto wrong=NativeOffscreenDisplayService::create(32,32,df);
    check(wrong!=nullptr&&display->release_context(df)&&wrong->acquire_context(df),"explicitly switch to another native context");
    GLuint foreign{};glGenTextures(1,&foreign);glBindTexture(GL_TEXTURE_2D,foreign);
    check(glIsTexture(foreign),"foreign context owns independent texture");
    check(!scene_bootstrap_required::continue_scene_reset_at_125d60(*h.main_state,sf)&&h.main_state->scene,
          "wrong-context teardown refuses and retains actual scene");
    check(glIsTexture(foreign),"wrong-context teardown cannot delete a foreign object");
    check(!main_binding::render_retail_frame(*h.main_state,sf),"wrong-context render cannot report success");
    glDeleteTextures(1,&foreign);
    check(wrong->release_context(df)&&display->acquire_context(df),"restore original owning native context");
    initial_task_required::transition_1225d0(*h.main_state,4,RetailInitialTransitionBody::menu_18db60);
    check(main_binding::dispatch_retail_enabled_mode(*h.main_state,sf),"second mode transition reloads archive cleanly");
    check(glIsTexture(old_id)==GL_FALSE,"mode transition releases previous native texture names before new draw");
    check(h.main_state->mode_scene_routes->active_mode()==4&&h.main_state->mode_scene_routes->active_stage()==30,
          "second mode and stage select the second HUD plan");
    pose();check(main_binding::render_retail_frame(*h.main_state,sf),"second mode GPU frame");read("mode4",false);
    const GLuint last_id=h.main_state->scene->world->gpu_texture_cache().front().gl_texture_id;
    initial_task_required::transition_1225d0(*h.main_state,0xffffffffu,RetailInitialTransitionBody::source_245930);
    check(!main_binding::dispatch_retail_enabled_mode(*h.main_state,sf)&&h.main_state->scene,
          "unbound mode refuses rather than silently selecting a default scene");
    initial_task_required::transition_1225d0(*h.main_state,9,RetailInitialTransitionBody::source_245930);
    check(!main_binding::dispatch_retail_enabled_mode(*h.main_state,sf),"corrupted selected archive fails mode preparation");
    check(glIsTexture(last_id)==GL_FALSE,"failed mode load leaves no earlier GPU texture allocation");
    check(!h.main_state->scene||!h.main_state->scene->scene_loaded,"failed load publishes no ready scene");
    check(retirement->empty(),"all mode GPU retirement work completed");
    check(textures->active_count()==initial_textures&&materials->active_count()==initial_materials,"mode teardown and failed load restore actual initial texture/material owners");
    check(scene_bootstrap_required::continue_scene_reset_at_125d60(*h.main_state,sf),"final empty mode teardown");
    check(glGetError()==GL_NO_ERROR,"zero native GL errors at final teardown");
    check(display->release_context(df),"release actual owning native context");
#endif
    std::printf("PASS native archive mode GPU checks=%zu; constructed component only; no retail mode-body or gameplay admission\n",mode_checks);
    return 0;
} catch(const std::exception& e) {
    std::fprintf(stderr,"FAIL native archive mode after%zu checks: %s\n",mode_checks,e.what());
    return 1;
}


