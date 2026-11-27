#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_scene_owners.hpp"
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_movie_controller.hpp"
#include "mka/retail_display_service.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <new>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mka::retail {

NativeRetailSceneOwners::~NativeRetailSceneOwners() noexcept {
    if (world) world->release_gpu_resources();
    if (stream_resources) {
        RetailRendererWorldFailure owned_failure;
        (void)stream_resources->cleanup(owned_failure);
    }
    stream_resources.reset();
    if (gpu_retirement) (void)gpu_retirement->poll();
    if (camera_init && cameras && frames && rasters) {
        if (world && camera_init->primary.world_link) {
            RetailCameraWorldFailure wf;
            (void)detach_camera_from_world_1cf350(camera_init->primary.world_link, world, wf);
        }
        destroy_camera_producer_17ebb0(*cameras, *frames, *rasters, camera_init->primary);
        if (camera_init->scene) {
            if (world && camera_init->scene->world_link) {
                RetailCameraWorldFailure wf;
                (void)detach_camera_from_world_1cf350(camera_init->scene->world_link, world, wf);
            }
            destroy_camera_producer_17ebb0(*cameras, *frames, *rasters, *camera_init->scene);
        }
        camera_init.reset();
    }
    if (world) {
        world->release_gpu_resources();
    }
    if (world_module && !world_handle.empty()) {
        RetailRendererWorldFailure wf;
        (void)world_module->destroy_world(world_handle, wf);
        world_handle = {};
    }
    world.reset();
    materials.reset();
    textures.reset();
    rasters.reset();
    cameras.reset();
    frames.reset();
    world_module.reset();
    display.reset();
    core.reset();
    scene_loaded = false;
}

} // namespace mka::retail

namespace mka::retail::scene_bootstrap_required {

namespace {

bool has_active_gl_context(const std::shared_ptr<NativeProductionDisplayService>& display) noexcept {
    return display && display->is_context_current();
}

bool resource_exists(const content::NativeContentVolume& volume, std::string_view key) noexcept {
    if (key.empty()) {
        return false;
    }
    content::ContentVolumeFailure cf;
    auto file = volume.open_resource(key, cf);
    return file.has_value();
}

} // namespace

RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState& main,
    std::shared_ptr<NativeRetailRendererCore> core, StartupFailure& f) try {
    record_placeholder_hit(0, "continue_first_scene_at_225d40", main.iteration);
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread() || !main.startup.game_session) {
        f = {StartupError::game_session, "225D40: Main lifetime/thread/Session required"};
        return RetailStartupGate::native_failure;
    }
    if (!core || !core->same_session(main.startup.game_session)) {
        f = {StartupError::memory_owners, "225D40: renderer Core must belong to Main Session"};
        return RetailStartupGate::native_failure;
    }

    RetailRendererCoreFailure cf;
    if (!core->native_plugins_constructed() && !core->construct_registered(cf)) {
        f.kind = StartupError::memory_owners;
        f.operation = cf.operation;
        return RetailStartupGate::native_failure;
    }
    auto world_mod = core->world_module(cf);
    auto tex_mod = core->textures(cf);
    auto mat_mod = core->materials(cf);
    auto raster_mod = core->rasters(cf);
    auto frame_mod = core->frames(cf);
    auto cam_mod = core->camera_module(cf);
    if (!world_mod || !tex_mod || !mat_mod || !raster_mod || !frame_mod || !cam_mod) {
        f.kind = StartupError::memory_owners;
        f.operation = cf.operation && *cf.operation ? cf.operation : "225D40: required renderer modules unavailable";
        return RetailStartupGate::native_failure;
    }

    if(main.scene && main.scene->scene_loaded) {
        f={StartupError::already_initialized,"225D40: reset existing scene before loading another"};
        return RetailStartupGate::native_failure;
    }
    // Prepare separately; a failed load cannot publish a half-built Scene.
    auto scene = std::make_shared<NativeRetailSceneOwners>();
    if(main.scene) scene->config=main.scene->config;
    scene->core = core;
    scene->world_module = world_mod;
    scene->frames = frame_mod;
    scene->cameras = cam_mod;
    scene->rasters = raster_mod;
    scene->textures = tex_mod;
    scene->materials = mat_mod;
    scene->display = scene->config.display;
    scene->gpu_retirement = scene->config.gpu_retirement;
    scene->gl_context_active = has_active_gl_context(scene->display);
    scene->scene_loaded = false;
    scene->geometry_ingested = false;
    scene->stream_result = {};
    scene->frames_rendered = 0;

    const auto& selected_archive=scene->config.archive_streams;
    const auto& selected_ssf=scene->config.ssf_scene;
    if(selected_archive&&selected_ssf){
        f={StartupError::content,"225D40: ambiguous direct archive and SSF scene sources"};return RetailStartupGate::native_failure;
    }
    const bool has_txd = selected_ssf ? false : selected_archive ? bool(selected_archive->txd) : resource_exists(main.startup.content, scene->config.txd_resource_key);
    const bool has_world_stream = selected_ssf ? false : selected_archive ? bool(selected_archive->world) : resource_exists(main.startup.content, scene->config.world_resource_key);
    const bool has_geom = bool(selected_ssf) || (selected_archive ? bool(selected_archive->geometry) : resource_exists(main.startup.content, scene->config.geometry_resource_key));
    if(selected_archive && !has_txd && !has_world_stream && !has_geom) {
        f={StartupError::content,"archive scene requires explicit stream selections"};return RetailStartupGate::native_failure;
    }

    // Asset-less headless mode: fail closed on scene assets without failing startup
    // so the 60-frame headless entry gate completes cleanly.
    if (!has_txd && !has_world_stream && !has_geom) {
        main.scene = std::move(scene);
        return RetailStartupGate::ready;
    }

    RetailRendererWorldFailure wf;
    auto world_handle = world_mod->create_world(wf);
    if (!world_handle) {
        f = {StartupError::memory_owners, wf.operation};
        return RetailStartupGate::native_failure;
    }
    auto world = world_mod->resolve_world(*world_handle, wf);
    if (!world) {
        (void)world_mod->destroy_world(*world_handle, wf);
        f = {StartupError::memory_owners, wf.operation};
        return RetailStartupGate::native_failure;
    }
    scene->world_handle = *world_handle;
    scene->world = world;
    scene->stream_resources=std::make_unique<RetailSceneResources>(raster_mod,tex_mod,mat_mod,frame_mod,world);
    if(scene->display) {
        if(!scene->gpu_retirement) {
            video::MovieGpuError error;
            scene->gpu_retirement=video::NativeMovieGpuRetirementOwner::create(error);
            if(!scene->gpu_retirement) {
                f={StartupError::allocation,"225D40: native GPU retirement owner allocation"};return RetailStartupGate::native_failure;
            }
        }
        if(!world->bind_gpu_resources(scene->display,scene->gpu_retirement,wf)) {
            f={StartupError::memory_owners,wf.operation};return RetailStartupGate::native_failure;
        }
    }
    if(selected_ssf) {
        RetailSceneArchiveFailure af;
        auto loaded=load_retail_ssf_scene_from_archive(main.startup.content,*selected_ssf,
            scene->config.base_object_id,*raster_mod,*tex_mod,*mat_mod,*frame_mod,
            *world,{},af,*scene->stream_resources);
        if(!loaded){f={StartupError::content,af.operation};return RetailStartupGate::native_failure;}
        scene->ssf_data=std::move(*loaded);
        scene->geometry_ingested=true;
    } else if(selected_archive) {
        RetailSceneArchiveFailure af;
        auto loaded=load_retail_scene_streams_from_archive(main.startup.content,*selected_archive,
            scene->config.base_sector_id,scene->config.base_object_id,*raster_mod,*tex_mod,*mat_mod,*frame_mod,
            *world,{},af,*scene->stream_resources);
        if(!loaded) {f={StartupError::content,af.operation};return RetailStartupGate::native_failure;}
        scene->stream_result={loaded->bytes_read,loaded->txd_ingested,loaded->world_ingested};
        scene->geometry_ingested=loaded->geometry_ingested;
    }

    const std::string_view txd_key = has_txd ? std::string_view(scene->config.txd_resource_key) : std::string_view{};
    const std::string_view world_key = has_world_stream ? std::string_view(scene->config.world_resource_key) : std::string_view{};
    if (!selected_archive && !selected_ssf && (!txd_key.empty() || !world_key.empty())) {
        auto loaded = load_retail_scene_streams_from_volume(
            main.startup.content,
            txd_key,
            world_key,
            scene->config.base_sector_id,
            *raster_mod,
            *tex_mod,
            *mat_mod,
            *world,
            wf,scene->stream_resources.get());
        if (!loaded) {
            scene.reset();
            f = {StartupError::content, wf.operation};
            return RetailStartupGate::native_failure;
        }
        scene->stream_result = *loaded;
    }

    if (!selected_archive && !selected_ssf && has_geom) {
        content::ContentVolumeFailure cvf;
        auto geom_file = main.startup.content.open_resource(scene->config.geometry_resource_key, cvf);
        const auto geom_len = geom_file ? geom_file->length(cvf) : std::optional<std::uint64_t>{};
        if (!geom_len || *geom_len < 12 || *geom_len > (64ull * 1024ull * 1024ull)) {
            scene.reset();
            f = {StartupError::content, "225D40: geometry stream length invalid or out of bounds"};
            return RetailStartupGate::native_failure;
        }
        std::vector<std::byte> geom_bytes(static_cast<std::size_t>(*geom_len));
        const auto rd = geom_file->read(geom_bytes.data(), geom_bytes.size(), cvf);
        if (cvf.kind != content::ContentVolumeError::none || rd.bytes != geom_bytes.size()) {
            scene.reset();
            f = {StartupError::content, "225D40: geometry stream read truncated"};
            return RetailStartupGate::native_failure;
        }
        if (!ingest_rw_geometry_stream(
                geom_bytes,
                scene->config.base_object_id,
                *frame_mod,
                *mat_mod,
                *tex_mod,
                *world,
                NativeRetailRendererFrameHandle{},
                wf,scene->stream_resources.get())) {
            scene.reset();
            f = {StartupError::content, wf.operation};
            return RetailStartupGate::native_failure;
        }
        scene->geometry_ingested = true;
    }

    if(scene->config.hud && !world->set_hud_submission(*scene->config.hud,wf)) {
        f={StartupError::content,wf.operation};return RetailStartupGate::native_failure;
    }
    RetailGameSessionFailure gf;
    auto* world_cams = main.startup.game_session->render_cameras(gf);
    if (!world_cams) {
        scene.reset();
        f = {StartupError::game_session, gf.operation};
        return RetailStartupGate::native_failure;
    }
    RetailCameraInit180ED0Config cam_cfg;
    cam_cfg.width = scene->config.viewport_width;
    cam_cfg.height = scene->config.viewport_height;
    cam_cfg.fov_degrees = scene->config.fov_degrees;
    cam_cfg.near_clip = scene->config.near_clip;
    cam_cfg.far_clip = scene->config.far_clip;
    cam_cfg.create_scene_camera = false;
    cam_cfg.primary_has_color = bool(scene->display);
    cam_cfg.primary_has_depth = bool(scene->display);
    cam_cfg.depth_inputs = {0.0f, 1.0f};
    cam_cfg.display = scene->display;
    if (scene->display && scene->config.viewport_width > 0 && scene->config.viewport_height > 0) {
        cam_cfg.target_policy = RetailRendererCameraTargetPolicy{
            {0, 0,
             static_cast<std::uint32_t>(scene->config.viewport_width),
             static_cast<std::uint32_t>(scene->config.viewport_height)},
            1.0f};
    }

    RetailCameraProducerFailure pf;
    auto cam_init = initialize_cameras_180ed0(
        main.startup.game_session,
        *cam_mod,
        *frame_mod,
        *raster_mod,
        *world_cams,
        cam_cfg,
        pf);
    if (!cam_init) {
        scene.reset();
        f = {StartupError::memory_owners, pf.operation};
        return RetailStartupGate::native_failure;
    }
    scene->camera_init = *cam_init;

    if (!attach_camera_to_world_1cf300(
            *cam_mod,
            *frame_mod,
            scene->camera_init->primary.camera,
            scene->camera_init->primary.world_link,
            world,
            pf)) {
        scene.reset();
        f = {StartupError::memory_owners, pf.operation};
        return RetailStartupGate::native_failure;
    }
    RetailCameraWorldFailure cwf;
    if (!world_cams->publish_world_after_186404(world, cwf)) {
        scene.reset();
        f = {StartupError::game_session, cwf.operation};
        return RetailStartupGate::native_failure;
    }

    scene->scene_loaded = true;
    main.scene = std::move(scene);
    return RetailStartupGate::ready;
} catch(const detail::NativeMainReentry&) {
    throw;
} catch(const std::bad_alloc&) {
    f={StartupError::allocation,"225D40: scene allocation failed before publication"};return RetailStartupGate::native_failure;
} catch(...) {
    f={StartupError::native_exception,"225D40: native scene load exception"};return RetailStartupGate::native_failure;
}

// NOTE / AUDIT: continue_scene_reset_at_125d60 detaches cameras via 181A30 and releases
// owned first-scene resources, while the 5 retail global words remain unrecovered.
bool continue_scene_reset_at_125d60(NativeRetailMainState& main, StartupFailure& f) {
    record_placeholder_hit(1, "continue_scene_reset_at_125d60", main.iteration);
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread() || !main.startup.game_session) {
        f = {StartupError::game_session, "125D60 placeholder: Main lifetime/thread/Session required"};
        return false;
    }
    // Explicit close refuses a wrong context without dropping the old scene.
    if(main.scene && main.scene->world) {
        RetailRendererWorldFailure wf;
        if(!main.scene->world->release_gpu_resources(wf)) {
            f={StartupError::memory_owners,wf.operation};return false;
        }
        if(main.scene->gpu_retirement) {
            const auto retired=main.scene->gpu_retirement->poll();
            if(retired.state!=video::MovieFenceState::complete || !main.scene->gpu_retirement->empty()) {
                f={StartupError::memory_owners,"scene GPU retirement must complete before mode teardown"};return false;
            }
        }
        if(main.scene->stream_resources && !main.scene->stream_resources->cleanup(wf)) {
            f={StartupError::memory_owners,wf.operation};return false;
        }
    }
    RetailGameSessionFailure gf;
    RetailCameraWorldFailure cf;
    auto* cameras = main.startup.game_session->render_cameras(gf);
    if (cameras) {
        (void)cameras->detach_cameras_from_181a30(cf);
        (void)cameras->publish_world_after_186404({}, cf);
    }
    main.scene.reset();
    return true;
}

} // namespace mka::retail::scene_bootstrap_required
