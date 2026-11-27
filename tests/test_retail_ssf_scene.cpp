#include "mka/retail_scene_archive.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_scene_owners.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_display_service.hpp"
#include "mka/retail_renderer_allocator.hpp"
#include "mka/retail_renderer_cameras.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <limits>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif
using namespace mka::retail;
namespace {
thread_local int fail_after=-1;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
}
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
// Real Main/Session/Core, with no replacement factory, callback or GL symbol.
struct Fixture {
    NativeRetailStartupState startup;
    mka::input::ControllerInput input;
    std::unique_ptr<NativeRetailMainState> main_state;
    std::shared_ptr<NativeRetailRendererCore> renderer;
    RetailRendererCoreFailure f;
    explicit Fixture(const std::filesystem::path& root):startup([&]{
        mka::content::ContentVolumeFailure vf;auto v=mka::content::NativeContentVolume::open(root,vf);
        check(bool(v),"actual original native volume");return std::move(*v);
    }(),{}) {
        StartupFailure sf;check(initialize_native_retail_game_session(startup,sf)&&initialize_retail_memory_owners(startup,sf),"actual native Session and heaps");
        main_state=std::make_unique<NativeRetailMainState>(startup,input);
        RetailRendererAllocatorFailure af;auto allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);
        check(bool(allocator),"actual native renderer allocator");
        renderer=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);
        check(bool(renderer),"actual persistent native Core");
        auto initialized=renderer->initialize_core(f);check(initialized&&*initialized,"actual Core initialization");
        auto cameras=startup.game_session->render_cameras(sf.game);RetailCameraWorldFailure cf;
        check(cameras&&cameras->store_initial_count_from_127260(cf),"actual Main initial camera count");
    }
    auto core(){return renderer;}
};
#ifdef _WIN32
struct CameraGpuFixture {
    Fixture native;
    std::shared_ptr<NativeRetailRendererCore> core;
    std::shared_ptr<NativeRetailRendererCameraModule> cameras;
    std::shared_ptr<NativeRetailRendererRasters> rasters;
    std::shared_ptr<mka::video::NativeMovieGpuRetirementOwner> world_retirement;
    explicit CameraGpuFixture(const std::filesystem::path& root):native(root),core(native.core()) {
        check(renderer_required::construct_core_plugin_0405(*core,native.f)&&renderer_required::construct_core_plugin_0407(*core,native.f),"actual native camera/raster constructors");
        cameras=core->camera_module(native.f);rasters=core->rasters(native.f);check(cameras&&rasters,"actual persistent camera/raster owners");
        mka::video::MovieGpuError error;world_retirement=mka::video::NativeMovieGpuRetirementOwner::create(error);
        check(bool(world_retirement),"explicit actual retirement owner");
    }
    ~CameraGpuFixture(){
        RetailRendererCoreFailure f;if(auto world=core->world_module(f))world->retire();
        if(world_retirement&&!world_retirement->empty()&&world_retirement->poll().state!=mka::video::MovieFenceState::complete)std::abort();
        renderer_required::destroy_core_plugin_0405(*core);renderer_required::destroy_core_plugin_0407(*core);
    }
    NativeRetailRasterHandle backed(std::uint32_t flags,const std::shared_ptr<NativeOffscreenDisplayService>& display) {
        RetailRendererRasterFailure f;auto h=rasters->create_raster({640,448,0,0,flags,0},f);
        check(h&&rasters->bind_display_target(*h,display,f),"actual display backed raster");return *h;
    }
    NativeRetailRendererCameraHandle camera(NativeRetailRasterHandle color,NativeRetailRasterHandle depth,RetailRendererCameraTargetPolicy policy) {
        RetailRendererCameraFailure f;auto h=cameras->create_base_camera(RetailRendererCameraDepthInputs{0,1},f);
        check(h&&cameras->set_frame_buffer_raster(*h,color,f)&&cameras->set_z_buffer_raster(*h,depth,f)&&
            cameras->set_render_target_policy(*h,policy,f),"actual camera backing and native policy");return *h;
    }
};
std::shared_ptr<NativeOffscreenDisplayService> native_display(){
    DisplayServiceFailure f;auto d=NativeOffscreenDisplayService::create(64,48,f,640,448);
    check(d&&d->acquire_context(f),"actual hidden native display");return d;
}
void save_gpu_bytes(const std::filesystem::path& output,const char* name,const void* bytes,std::size_t size){
    const auto p=output/name;check(!std::filesystem::exists(p),"preserve previous GPU evidence");
    std::ofstream f(p,std::ios::binary);f.write(static_cast<const char*>(bytes),size);check(bool(f),"actual GPU bytes saved");
}
#endif
}

namespace {
std::size_t ssf_checks{};
void ssf_check(bool value,const char* why) {
    ++ssf_checks;if(!value)std::fprintf(stderr,"SSF_ASSERTION: %s\n",why);check(value,why);
}
std::uint64_t ssf_hash(std::span<const std::uint8_t> bytes) {
    std::uint64_t h=14695981039346656037ull;for(auto b:bytes)h=(h^b)*1099511628211ull;return h;
}
RetailSsfSceneSelection actual_selection() {
    RetailSsfSceneSelection c;c.archive_resource_key="mkda.pak";c.member.index=219;
    c.section_index=0;c.clump_resource.name="SKYTEMPLE_BANNERS";c.atomic_index=0;c.morph_index=0;
    c.texture_resource_indices={65};return c;
}
void source_checks(const RetailDecodedSsfScene& source) {
    const auto& g=source.geometry;
    ssf_check(g.geometry_index==0&&g.frame_index==2&&g.morph_index==0&&g.morph_count==2&&
        g.vertices.size()==108&&g.triangles.size()==128&&g.materials.size()==1,
        "actual atomic frame geometry and morph selection");
    ssf_check(g.frames.size()==5&&g.frames[2].parent==1&&g.frames[1].parent==0&&g.frames[0].parent==-1,
        "actual selected frame ancestry retained");
    const auto& p=g.vertices.front().position;
    ssf_check(std::bit_cast<std::uint32_t>(p.x)==0x412291f2u&&std::bit_cast<std::uint32_t>(p.y)==0x402e3ffcu&&
        std::bit_cast<std::uint32_t>(p.z)==0x3e832703u,"actual aligned first morph vertex words");
    ssf_check(g.triangles[0].vertices==std::array<std::size_t,3>{0,1,2}&&g.triangles[0].material==0&&
        g.triangles[1].vertices==std::array<std::size_t,3>{1,3,2},"actual direct-layout triangle words and material");
    ssf_check(g.vertices.front().stored_color==std::array<std::uint8_t,4>{88,72,65,255},
        "actual raw vertex color retained");
    ssf_check(g.materials[0].texture_name=="ST_Banner"&&g.materials[0].mask_name.empty()&&
        g.materials[0].texture_mip_metadata==0xb30&&
        g.materials[0].stored_color==std::array<std::uint8_t,4>{255,255,255,255}&&
        g.materials[0].plugin_words==std::array<std::uint32_t,7>{5,0,0,0xffffffffu,0,0,0},
        "actual material and proprietary metadata retained without default substitution");
    ssf_check(source.textures.size()==1&&source.textures[0].name=="ST_Banner"&&source.textures[0].width==32&&
        source.textures[0].height==128&&source.textures[0].sampler_metadata==0x1102&&
        ssf_hash(source.textures[0].rgba_raw_alpha)==0xeeef66da6ee0a397ull,
        "actual selected texture body name and all4096 texels");
}
}

int main(int argc,char** argv) try {
    ssf_check(argc==3,"explicit original volume and fresh private output required");
    if constexpr(sizeof(std::size_t)>4) {
        const auto wide_end=ssf_geometry_detail::bounded_chunk_end(0x20,0xfffffff8u,std::size_t(0x100000100ull));
        ssf_check(wide_end&&*wide_end==std::size_t(0x100000024ull),"host-sized chunk cursor crosses u32 boundary without wrap");
        ssf_check(!ssf_geometry_detail::bounded_chunk_end(0x20,0xfffffff8u,std::size_t(0x100000023ull)),
            "host-sized chunk cursor rejects one-byte parent overflow");
    }
    ssf_check(!ssf_geometry_detail::bounded_chunk_end(std::numeric_limits<std::size_t>::max()-4,0,std::numeric_limits<std::size_t>::max()),
        "host-sized chunk cursor rejects truncated header near size maximum");
    const std::filesystem::path original(argv[1]),output(argv[2]);
    ssf_check(std::filesystem::create_directory(output),"fresh private output preserves all prior attempts");
    std::puts("SSF_NATIVE_READY explicit_original_PAK member219 section0 record4 texture65 atomic0 morph0");
    mka::content::ContentVolumeFailure vf;auto volume=mka::content::NativeContentVolume::open(original,vf);
    ssf_check(bool(volume),"actual original content volume");
    auto selection=actual_selection();RetailSceneArchiveFailure failure;
    auto decoded=decode_retail_ssf_scene_from_archive(*volume,selection,failure);
    if(!decoded)std::fprintf(stderr,"SSF decode kind=%u operation=%s geometry_offset=%zu\n",unsigned(failure.kind),failure.operation,failure.geometry.offset);
    ssf_check(bool(decoded),"actual selected SSF CPU decode");source_checks(*decoded);

#ifdef _WIN32
    CameraGpuFixture native(original);auto display=native_display();auto& core=*native.core;auto& cf=native.native.f;
    ssf_check(renderer_required::construct_core_plugin_0403(core,cf)&&renderer_required::construct_core_plugin_0408(core,cf)&&
        renderer_required::construct_core_plugin_040a(core,cf)&&renderer_required::construct_core_plugin_0412(core,cf),
        "actual native frame texture material and world modules");
    auto frames=core.frames(cf);auto materials=core.materials(cf);auto textures=core.textures(cf);auto worlds=core.world_module(cf);
    RetailRendererWorldFailure wf;auto wh=worlds->create_world(wf);auto world=wh?worlds->resolve_world(*wh,wf):nullptr;
    ssf_check(world&&world->bind_gpu_resources(display,native.world_retirement,wf),"explicit actual world GPU resource binding");
    auto resources=std::make_unique<RetailSceneResources>(native.rasters,textures,materials,frames,world);
    auto load=[&](const RetailSsfSceneSelection& c){return load_retail_ssf_scene_from_archive(*volume,c,1000,
        *native.rasters,*textures,*materials,*frames,*world,{},failure,*resources);};
    const auto texture_baseline=textures->active_count(),material_baseline=materials->active_count();
    RetailRendererRasterFailure initial_rf;const auto raster_baseline=native.rasters->live_rasters(initial_rf);
    ssf_check(bool(raster_baseline),"actual raster ownership baseline");
    ssf_check(!load(selection)&&failure.kind==RetailSceneArchiveError::render_policy&&resources->empty()&&world->scene_objects().empty(),
        "unbound retail render policy refuses before native resource publication");
    selection.render_policy=RetailSsfSceneRenderPolicy::diagnostic_unlit_raw_rgba8;
    auto bad=selection;bad.texture_resource_indices.clear();
    ssf_check(!load(bad)&&failure.kind==RetailSceneArchiveError::texture_binding&&resources->empty()&&
        textures->active_count()==texture_baseline&&materials->active_count()==material_baseline,
        "missing authentic texture refuses without white fallback or owner mutation");
    bad=selection;bad.texture_resource_indices={64};
    const auto outside_range=load(bad);
    if(failure.kind==RetailSceneArchiveError::texture_binding&&std::string_view(failure.operation)!=
        "SSF texture ordinal outside source clump range or duplicated")
        std::fprintf(stderr,"SSF_RANGE_REFUSAL observed_operation=%s\n",failure.operation);
    ssf_check(!outside_range&&failure.kind==RetailSceneArchiveError::texture_binding&&resources->empty()&&
        std::string_view(failure.operation)=="SSF texture ordinal outside source clump range or duplicated",
        "texture outside clump wrapper range refused");
    {
        Fixture foreign(original);
        ssf_check(renderer_required::construct_core_plugin_0403(*foreign.renderer,foreign.f),"actual foreign Session frame module");
        auto foreign_frames=foreign.renderer->frames(foreign.f);
        RetailSceneResources foreign_resources(native.rasters,textures,materials,foreign_frames,world);
        auto foreign_result=load_retail_ssf_scene_from_archive(*volume,selection,3000,*native.rasters,*textures,
            *materials,*foreign_frames,*world,{},failure,foreign_resources);
        if(foreign_result) {
            std::printf("SSF_FOREIGN_FRAMES observed_success=1 published_objects=%zu journal_frames=%zu\n",
                world->scene_objects().size(),foreign_resources.frames.size());
            ssf_check(foreign_resources.cleanup(wf)&&foreign_resources.empty()&&world->scene_objects().empty()&&
                textures->active_count()==texture_baseline&&materials->active_count()==material_baseline,
                "pre-fix foreign Frames reproducer explicitly cleans all published resources");
        }
        ssf_check(!foreign_result&&failure.kind==RetailSceneArchiveError::resource_owner&&foreign_resources.empty()&&
            world->scene_objects().empty()&&textures->active_count()==texture_baseline&&materials->active_count()==material_baseline,
            "SSF foreign Frames owner refused before publication");
    }
    std::optional<RetailDecodedSsfScene> loaded;
    std::size_t allocation_cuts=0;bool texture_cut=false,material_cut=false,frame_cut=false,object_cut=false;
    for(int cut=0;cut<4096;++cut) {
        fail_after=cut;auto attempt=load(selection);const bool exhausted=fail_after==0;fail_after=-1;
        if(attempt){loaded=std::move(attempt);break;}
        if(!exhausted)break; // A real format/owner failure is not an allocation retry.
        ++allocation_cuts;
        const std::string operation=failure.operation;
        texture_cut|=operation.find("0408")!=std::string::npos||operation=="texture registry growth";
        material_cut|=operation.find("040a")!=std::string::npos;
        frame_cut|=operation.find("0403")!=std::string::npos;
        object_cut|=operation=="0412 scene object allocation";
        RetailRendererRasterFailure rf;const auto live=native.rasters->live_rasters(rf);
        ssf_check(failure.kind!=RetailSceneArchiveError::none&&failure.cleanup.kind==RetailRendererWorldError::none&&
            resources->empty()&&world->scene_objects().empty()&&textures->active_count()==texture_baseline&&
            materials->active_count()==material_baseline&&live&&*live==*raster_baseline,
            "SSF allocation prefix restores actual owner journals and module counts");
        if(operation.find("0408")!=std::string::npos||operation.find("040a")!=std::string::npos||
            operation.find("0403")!=std::string::npos||operation=="0412 scene object allocation")
            std::printf("SSF_ALLOCATION_CUT cut=%d operation=%s\n",cut,failure.operation);
    }
    if(!loaded) {
        std::fprintf(stderr,"SSF load kind=%u operation=%s\n",unsigned(failure.kind),failure.operation);
        RetailRendererRasterFailure rf;const auto live=native.rasters->live_rasters(rf);
        ssf_check(resources->empty()&&world->scene_objects().empty()&&textures->active_count()==texture_baseline&&
            materials->active_count()==material_baseline&&live&&*live==*raster_baseline&&
            failure.cleanup.kind==RetailRendererWorldError::none,"SSF failed ingestion restores journals and module counts");
        std::puts("SSF_FAILURE_CLEAN actual_owner_journal_and_counts_restored");
    }
    ssf_check(bool(loaded),"actual SSF decoded data reaches native owners");source_checks(*loaded);
    ssf_check(allocation_cuts&&texture_cut&&material_cut&&frame_cut&&object_cut,
        "SSF allocation sweep reaches texture material frame and world publication failures");
    std::printf("SSF_ALLOCATION_PREFIX cuts=%zu texture=%u material=%u frame=%u object=%u\n",
        allocation_cuts,texture_cut,material_cut,frame_cut,object_cut);
    ssf_check(resources->rasters.size()==1&&resources->textures.size()==1&&resources->materials.size()==1&&
        resources->frames.size()==3&&resources->scene_objects.size()==1&&world->scene_objects().size()==1,
        "actual resource ownership journal accounts for selected mesh");
    const auto& object=world->scene_objects().front();
    ssf_check(object.triangles.size()==384&&object.diagnostic_unlit_raw_rgba8&&
        object.triangles.front().color[0]==88.0f/255&&object.triangles.front().color[1]==72.0f/255,
        "explicit diagnostic policy retains original geometry and raw vertex color");
    ssf_check(!load(selection)&&failure.kind==RetailSceneArchiveError::resource_owner,
        "populated owner cannot be replaced");

    const auto color=native.backed(2,display),depth=native.backed(1,display);
    const auto camera=native.camera(color,depth,{{0,0,64,48},1.0f});RetailRendererCameraFailure camera_failure;RetailRendererFrameFailure ff;
    auto camera_frame=frames->create_base_frame(ff);
    ssf_check(camera_frame&&native.cameras->set_frame(camera,*camera_frame,camera_failure),"actual preview camera frame association");
    // Explicit native diagnostic viewer, not a recovered retail camera producer.
    const RetailRendererAffine view{{0,0,-1},{0,1,0},{1,0,0},{-30,3.52332735f,0.09155327f}};
    ssf_check(frames->set_modelling(*camera_frame,{view,0},ff)&&
        native.cameras->set_view_window(camera,{0.15f,0.15f},camera_failure)&&
        native.cameras->set_near_clip(camera,0.1f,camera_failure)&&native.cameras->set_far_clip(camera,100,camera_failure),
        "explicit physical diagnostic view and projection");
    RetailCameraWorldFailure lf;auto link=NativeRetailCameraWorldLink::create_initial(native.native.startup.game_session,lf);
    RetailCameraProducerFailure pf;
    ssf_check(link&&attach_camera_to_world_1cf300(*native.cameras,*frames,camera,link,world,pf),"actual camera world binding");
    ssf_check(world->add_light({1,RetailWorldLightType::ambient,{0,0,0},{0,0,0},true},wf),
        "real zero ambient light makes explicit unlit policy observable");
    GLint readiness_depth{};glGetIntegerv(GL_DEPTH_BITS,&readiness_depth);
    ssf_check(readiness_depth>0&&glGetError()==GL_NO_ERROR,"actual GPU context ready for SSF draw");
    std::printf("SSF_GPU_READY vendor=%s renderer=%s version=%s depth_bits=%d\n",
        glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION),readiness_depth);
    auto render=render_world_scene_frame(*native.cameras,*frames,camera,link,world,{1,0,1,1},3,false,pf,
        materials.get(),textures.get(),native.rasters.get());
    if(!render)std::fprintf(stderr,"SSF render failed: %s\n",pf.operation);
    ssf_check(render&&render->visible_objects==1&&render->bound_textures==1,"real selected SSF mesh submitted to actual GPU");
    GLint depth_bits{};glGetIntegerv(GL_DEPTH_BITS,&depth_bits);
    ssf_check(depth_bits>0,"actual host depth buffer availability queried");
    std::vector<std::uint8_t> pixels(64*48*4);std::vector<float> depths(64*48);
    glReadBuffer(GL_BACK);glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
    ssf_check(glGetError()==GL_NO_ERROR,"actual backbuffer color and depth readback");
    std::size_t drawn=0,lit=0;unsigned max_red=0;
    for(std::size_t i=0;i<depths.size();++i)if(depths[i]<0.99999f) {
        ++drawn;max_red=std::max(max_red,unsigned(pixels[i*4]));
        if(pixels[i*4]||pixels[i*4+1]||pixels[i*4+2])++lit;
        ssf_check(std::abs(int(pixels[i*4+3])-128)<=1,"diagnostic raw texture alpha reaches real framebuffer");
    }
    ssf_check(drawn>=20&&lit==drawn,"SSF_GPU_PIXELS: actual mesh depth and unlit texture pixels visible");
    ssf_check(max_red<=89,"SSF_GPU_VERTEX_COLOR: raw source vertex color modulates texture");
    const auto& cache=world->gpu_texture_cache();
    ssf_check(cache.size()==1&&cache[0].ready,"actual retained native texture upload");
    GLint previous{};glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);glBindTexture(GL_TEXTURE_2D,cache[0].gl_texture_id);
    std::vector<std::uint8_t> uploaded(32*128*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,uploaded.data());
    glBindTexture(GL_TEXTURE_2D,GLuint(previous));
    ssf_check(glGetError()==GL_NO_ERROR&&ssf_hash(uploaded)==0xeeef66da6ee0a397ull,
        "SSF_GPU_TEXELS: all authentic texture texels retained in real GPU storage");
    save_gpu_bytes(output,"ssf-frame.rgba",pixels.data(),pixels.size());
    save_gpu_bytes(output,"ssf-depth.f32",depths.data(),depths.size()*sizeof(float));
    auto other=native_display();
    ssf_check(!world->release_gpu_resources(wf)&&wf.kind==RetailRendererWorldError::gpu_context,
        "wrong context cannot discard actual SSF GPU resources");
    DisplayServiceFailure df;ssf_check(display->acquire_context(df)&&world->release_gpu_resources(wf),"original context deletes all actual SSF GPU resources");
    ssf_check(resources->cleanup(wf)&&resources->empty()&&world->scene_objects().empty()&&
        textures->active_count()==texture_baseline&&materials->active_count()==material_baseline,
        "selected SSF resource teardown restores actual module ownership counts");
    ssf_check(native.cameras->destroy_base_camera(camera,camera_failure)&&frames->destroy_leaf(*camera_frame,ff),"preview camera and frame released");
    world->clear_lights();worlds->retire();world.reset();resources.reset();
    const auto retired=native.world_retirement->poll();
    ssf_check(retired.state==mka::video::MovieFenceState::complete,"actual retirement owner drained on original context");

    // The same authentic selection also traverses the production Main scene
    // continuation and existing reset owner. No fixture factory is substituted.
    Fixture connected(original);StartupFailure sf;
    ssf_check(connected.renderer->construct_registered(connected.f),"connected actual complete native module registry");
    auto connected_textures=connected.renderer->textures(connected.f);
    auto connected_materials=connected.renderer->materials(connected.f);
    const auto connected_texture_baseline=connected_textures->active_count(),connected_material_baseline=connected_materials->active_count();
    mka::video::MovieGpuError retirement_error;
    auto connected_retirement=mka::video::NativeMovieGpuRetirementOwner::create(retirement_error);
    ssf_check(bool(connected_retirement),"connected actual retained GPU retirement owner");
    RetailFirstSceneConfig config;config.ssf_scene=actual_selection();config.display=display;config.gpu_retirement=connected_retirement;
    config.viewport_width=64;config.viewport_height=48;config.call_show_raster=false;
    config.clear_color={1,0,1,1};config.near_clip=0.1f;config.far_clip=100;
    auto& main=*connected.main_state;
    main.scene=std::make_shared<NativeRetailSceneOwners>();main.scene->config=config;
    main.scene->config.archive_streams=RetailArchiveSceneStreams{};
    ssf_check(scene_bootstrap_required::continue_first_scene_at_225d40(main,connected.renderer,sf)==RetailStartupGate::native_failure&&
        main.scene&&!main.scene->scene_loaded&&sf.kind==StartupError::content,
        "connected ambiguous archive and SSF sources refuse without fallback");
    main.scene->config=config;
    ssf_check(scene_bootstrap_required::continue_first_scene_at_225d40(main,connected.renderer,sf)==RetailStartupGate::native_failure&&
        main.scene&&!main.scene->scene_loaded&&sf.kind==StartupError::content&&
        connected_textures->active_count()==connected_texture_baseline&&connected_materials->active_count()==connected_material_baseline,
        "connected default retail render policy remains unbound without partial publication");
    main.scene->config.ssf_scene->render_policy=RetailSsfSceneRenderPolicy::diagnostic_unlit_raw_rgba8;
    ssf_check(scene_bootstrap_required::continue_first_scene_at_225d40(main,connected.renderer,sf)==RetailStartupGate::ready&&
        main.scene&&main.scene->scene_loaded&&main.scene->ssf_data&&main.scene->geometry_ingested,
        "SSF_CONNECTED_LOAD: 225D40 retains actual decoded SSF scene");
    source_checks(*main.scene->ssf_data);
    ssf_check(main.scene->stream_result.bytes_read==0&&main.scene->ssf_data->serialized_resource_bytes==19584,
        "connected SSF decoded bytes kept separate from loose file IO receipt");
    auto& scene=*main.scene;
    ssf_check(scene.frames->set_modelling(scene.camera_init->primary.frame,{view,0},ff)&&
        scene.cameras->set_view_window(scene.camera_init->primary.camera,{0.15f,0.15f},camera_failure),
        "connected explicit diagnostic camera view");
    std::puts("SSF_CONNECTED_GPU_READY actual_Main_225D40_selected_real_SSF");
    ssf_check(main_binding::render_retail_frame(main,sf)&&scene.frames_rendered==1,
        "SSF_CONNECTED_RENDER: actual Main frame renders selected authentic mesh");
    std::vector<std::uint8_t> connected_pixels(64*48*4);
    glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,connected_pixels.data());
    ssf_check(glGetError()==GL_NO_ERROR&&connected_pixels==pixels,
        "SSF_CONNECTED_PIXELS: Main continuation matches direct actual GPU preview");
    ssf_check(scene.world->gpu_texture_cache().size()==1&&scene.stream_resources&&!scene.stream_resources->empty(),
        "connected scene owns actual native texture and resource journals");
    const GLuint connected_texture_id=scene.world->gpu_texture_cache().front().gl_texture_id;
    ssf_check(other->acquire_context(df)&&!scene_bootstrap_required::continue_scene_reset_at_125d60(main,sf)&&
        main.scene&&main.scene->scene_loaded&&main.scene->ssf_data,
        "connected wrong context reset retains actual scene and decoded metadata");
    ssf_check(display->acquire_context(df)&&scene_bootstrap_required::continue_scene_reset_at_125d60(main,sf)&&!main.scene,
        "SSF_CONNECTED_RESET: 125D60 retires actual SSF scene on original context");
    ssf_check(glIsTexture(connected_texture_id)==GL_FALSE&&connected_retirement->empty()&&
        connected_textures->active_count()==connected_texture_baseline&&connected_materials->active_count()==connected_material_baseline,
        "SSF_CONNECTED_CLEANUP: actual GL names and typed owners restored by Main reset");
    ssf_check(connected.renderer->destroy_constructed(connected.f),"connected actual module registry teardown");
    std::printf("SSF_REAL_GPU drawn_pixels=%zu nonblack_pixels=%zu max_red=%u depth_bits=%d\n",drawn,lit,max_red,depth_bits);
    std::printf("PASS real SSF scene checks=%zu source_vertices=108 source_triangles=128 texture_texels=4096; explicit diagnostic policy only; retail producer/material equivalence OPEN\n",ssf_checks);
    return 0;
#else
    std::fputs("SSF actual GPU test requires native Windows display\n",stderr);return 2;
#endif
} catch(const std::exception& e) {fail_after=-1;std::fprintf(stderr,"SSF scene failed: %s\n",e.what());return 1;}
