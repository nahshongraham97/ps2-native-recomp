#include "mka/retail_renderer_world.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <limits>

namespace mka::retail {

namespace {

template<class T, class Create>
std::shared_ptr<T> heap_object(
    const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,
    RetailRendererWorldFailure& f,
    Create create) noexcept {
    static_assert(alignof(T) <= 16);
    auto allocation = heap->allocate(sizeof(T), f.allocator);
    if (!allocation) {
        f.kind = RetailRendererWorldError::allocator;
        f.operation = "0412 actual typed Permanent allocation";
        return {};
    }
    try {
        auto* object = create(allocation->data());
        return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
            p->~T();
            alloc = memory::NativeAllocation{};
        });
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 actual native ownership";
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 native construction";
    }
    return {};
}

} // namespace

struct NativeRetailWorldGpuResources final : video::NativeMovieGpuRetirementTask {
    std::shared_ptr<NativeProductionDisplayService> display;
    std::weak_ptr<video::NativeMovieGpuRetirementOwner> retirement;
    std::uint64_t destination{};
    const std::thread::id thread{std::this_thread::get_id()};
    std::vector<RetailWorldGpuTextureEntry> textures;
    bool deletion_unknown{false};
    bool needs_finish{true};
    bool current() const noexcept {
        return std::this_thread::get_id()==thread && display &&
            display->destination_identity()==destination && display->is_context_current();
    }
    video::MovieFenceResult clean() noexcept {
        using namespace video;
        if(deletion_unknown)return {MovieFenceState::failed,MovieGpuError::stale_resource};
        if(textures.empty()&&!needs_finish)return {MovieFenceState::complete,MovieGpuError::none};
        if(!current())return {MovieFenceState::pending,MovieGpuError::pending};
#ifdef _WIN32
        if(glGetError()!=GL_NO_ERROR)return {MovieFenceState::failed,MovieGpuError::present_failed};
        while(!textures.empty()){
            auto& entry=textures.back();
            if(entry.gl_texture_id){
                const GLuint id=entry.gl_texture_id;glDeleteTextures(1,&id);
                if(glGetError()!=GL_NO_ERROR){deletion_unknown=true;return {MovieFenceState::failed,MovieGpuError::stale_resource};}
            }
            textures.pop_back();
        }
        // Explicit resource close/host retirement, never the per-frame draw path:
        // wait for native commands that may still reference the deleted storage.
        glFinish();
        if(glGetError()!=GL_NO_ERROR){deletion_unknown=true;return {MovieFenceState::failed,MovieGpuError::fence_failed};}
        needs_finish=false;
        return {MovieFenceState::complete,MovieGpuError::none};
#else
        return {MovieFenceState::failed,MovieGpuError::unavailable};
#endif
    }
    video::MovieFenceResult poll_retirement() noexcept override { return clean(); }
    video::MovieFenceResult retry_retirement() noexcept override { return clean(); }
};

NativeRetailWorld::NativeRetailWorld(const std::shared_ptr<NativeRetailGameSession>& session,
    std::weak_ptr<NativeRetailRendererCore> core) noexcept
    : NativeRetailRenderWorld(session),session_(session),core_(std::move(core)) {}

bool NativeRetailWorld::access_world(RetailRendererWorldFailure& f) const noexcept {
    f={};
    if(retired_){f.kind=RetailRendererWorldError::retired_world;f.operation="0412 world retired";return false;}
    if(std::this_thread::get_id()!=thread_){f.kind=RetailRendererWorldError::wrong_thread;f.operation="0412 world owner thread required";return false;}
    auto session=session_.lock();RetailGameSessionFailure sf;
    if(!session||!session->validate_current_thread(sf)){f.kind=RetailRendererWorldError::owner;f.operation="0412 live world Session required";return false;}
    return true;
}
void NativeRetailWorld::retire_world() noexcept { retired_=true;hud_.reset();gpu_registration_.retire(); }
NativeRetailWorld::~NativeRetailWorld() { release_gpu_resources();gpu_registration_.retire(); }

const std::vector<RetailWorldGpuTextureEntry>& NativeRetailWorld::gpu_texture_cache() const noexcept {
    static const std::vector<RetailWorldGpuTextureEntry> empty;
    return gpu_?gpu_->textures:empty;
}
bool NativeRetailWorld::bind_gpu_resources(std::shared_ptr<NativeProductionDisplayService> display,
    std::shared_ptr<video::NativeMovieGpuRetirementOwner> retirement,RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return false;
    if(!display||!retirement||!display->destination_identity()||!display->is_context_current()||
       !display->target_width()||!display->target_height()){
        f.kind=RetailRendererWorldError::gpu_context;f.operation="0412 explicit current display and host retirement owner required";return false;
    }
    if(gpu_){
        if(gpu_->display!=display||gpu_->display.owner_before(display)||display.owner_before(gpu_->display)||
           gpu_->retirement.owner_before(retirement)||retirement.owner_before(gpu_->retirement)||
           gpu_->destination!=display->destination_identity()||gpu_->deletion_unknown){
            f.kind=RetailRendererWorldError::gpu_context;f.operation="0412 GPU binding cannot change owner or generation";return false;
        }
        return true;
    }
    try{
        auto state=std::make_shared<NativeRetailWorldGpuResources>();state->display=std::move(display);
        state->destination=state->display->destination_identity();state->retirement=retirement;
        video::MovieGpuError error;auto receipt=retirement->register_task(state,error);
        if(!receipt){f.kind=RetailRendererWorldError::allocation;f.operation="0412 pre-register native resource retirement";return false;}
        gpu_registration_=video::MovieGpuRetirementRegistration(std::move(receipt));gpu_=std::move(state);
        return true;
    }catch(...){f.kind=RetailRendererWorldError::allocation;f.operation="0412 native GPU resource ownership allocation";return false;}
}
std::shared_ptr<NativeProductionDisplayService> NativeRetailWorld::gpu_display(RetailRendererWorldFailure& f) const noexcept {
    if(!access_world(f))return {};
    if(!gpu_){f.kind=RetailRendererWorldError::gpu_unbound;f.operation="0412 GPU owner not bound";return {};}
    if(!gpu_->current()||gpu_->retirement.expired()||gpu_->deletion_unknown){
        f.kind=RetailRendererWorldError::gpu_context;f.operation="0412 original live GPU context/retirement generation required";return {};
    }
    return gpu_->display;
}
bool NativeRetailWorld::release_gpu_resources(RetailRendererWorldFailure& f) noexcept {
    f={};
    if(std::this_thread::get_id()!=thread_){f.kind=RetailRendererWorldError::wrong_thread;f.operation="0412 resource cleanup requires original host thread";return false;}
    if(!gpu_)return true;
    const auto result=gpu_->clean();
    if(result.state==video::MovieFenceState::complete){
        hud_.reset();gpu_registration_.retire();gpu_registration_=video::MovieGpuRetirementRegistration{};gpu_.reset();return true;
    }
    f.kind=result.state==video::MovieFenceState::pending?RetailRendererWorldError::gpu_context:RetailRendererWorldError::gpu_operation;
    f.operation="0412 retain GPU resources until original context cleanup succeeds";return false;
}
void NativeRetailWorld::release_gpu_resources() noexcept { RetailRendererWorldFailure f;(void)release_gpu_resources(f); }

bool NativeRetailWorld::admit_render_modules(NativeRetailRendererMaterials* materials,
    NativeRetailRendererTextures* textures,NativeRetailRendererRasters* rasters,RetailRendererWorldFailure& f,
    NativeRetailRendererFrames* frames) const noexcept {
    if(!access_world(f))return false;
    auto core=core_.lock();RetailRendererCoreFailure cf;
    if(!core||(materials&&core->materials(cf).get()!=materials)||(textures&&core->textures(cf).get()!=textures)||
       (rasters&&core->rasters(cf).get()!=rasters)||(frames&&core->frames(cf).get()!=frames)){
        f.kind=RetailRendererWorldError::foreign_owner;f.operation="0412 draw modules must belong to world's actual live Core";return false;
    }
    return true;
}
std::optional<std::uint32_t> NativeRetailWorld::resolve_gpu_texture(const NativeRetailTextureHandle& texture,
    NativeRetailRendererTextures& textures,NativeRetailRendererRasters& rasters,RetailRendererWorldFailure& f) noexcept {
    if(!gpu_display(f)||!admit_render_modules(nullptr,&textures,&rasters,f))return {};
    RetailRendererTextureFailure tf;RetailRendererRasterFailure rf;
    auto rh=textures.raster(texture,tf);auto tobs=textures.observe(texture,tf);
    if(!rh||!tobs||rh->empty()){f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 texture requires live raster and metadata";return {};}
    auto robs=rasters.observe(*rh,rf);
    if(!robs){f.kind=RetailRendererWorldError::gpu_operation;f.operation=rf.operation;return {};}
    const RetailWorldGpuTextureEntry* cached=nullptr;
    for(auto it=gpu_->textures.rbegin();it!=gpu_->textures.rend();++it)if(it->texture.same_identity(texture)){
        if(!it->ready){f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 earlier upload failed; explicit cleanup required";return {};}
        cached=&*it;break;
    }
#ifdef _WIN32
    if(glGetError()!=GL_NO_ERROR){f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 incoming texture GPU error";return {};}
    GLint active{},pbo{},row{},skip_rows{},skip_pixels{},alignment{},max_size{},prior_binding{};
    glGetIntegerv(0x84e0,&active);glGetIntegerv(0x88ef,&pbo);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max_size);glGetIntegerv(GL_TEXTURE_BINDING_2D,&prior_binding);
    const auto w=robs->description.width,h=robs->description.height;
    if(glGetError()!=GL_NO_ERROR||active!=0x84c0||pbo||row||skip_rows||skip_pixels||alignment!=4||w<=0||h<=0||w>max_size||h>max_size||
       robs->is_subview||robs->description.depth!=32||robs->description.type!=4||(robs->format&0x0f00)!=0x0500||
       static_cast<std::size_t>(w)>std::numeric_limits<std::size_t>::max()/4||robs->stride!=static_cast<std::size_t>(w)*4||
       static_cast<std::size_t>(h)>std::numeric_limits<std::size_t>::max()/robs->stride){
        f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 explicit contiguous RGBA8 root texture and canonical unpack state required";return {};
    }
    if((tobs->filter!=RetailRendererTextureFilter::nearest&&tobs->filter!=RetailRendererTextureFilter::linear)||
       (tobs->address_u!=RetailRendererTextureAddress::clamp&&tobs->address_u!=RetailRendererTextureAddress::wrap)||
       (tobs->address_v!=RetailRendererTextureAddress::clamp&&tobs->address_v!=RetailRendererTextureAddress::wrap)){
        f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 unsupported native texture sampler policy";return {};
    }
    auto pixels=rasters.lock_pixels(*rh,rf);
    if(!pixels){f.kind=RetailRendererWorldError::gpu_operation;f.operation=rf.operation;return {};}
    if(pixels->size()<robs->stride*static_cast<std::size_t>(h)){
        (void)rasters.unlock_pixels(*rh,rf);f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 texture byte extent insufficient";return {};
    }
    const auto count=robs->stride*static_cast<std::size_t>(h);
    if(cached&&cached->raster.same_identity(*rh)&&cached->width==w&&cached->height==h&&
       cached->filter==tobs->filter&&cached->address_u==tobs->address_u&&cached->address_v==tobs->address_v&&
       cached->source_bytes.size()==count&&std::equal(cached->source_bytes.begin(),cached->source_bytes.end(),pixels->begin())){
        const auto id=cached->gl_texture_id;
        if(!rasters.unlock_pixels(*rh,rf)){f.kind=RetailRendererWorldError::gpu_operation;f.operation=rf.operation;return {};}
        if(!glIsTexture(id)||glGetError()!=GL_NO_ERROR){f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 owned GPU texture was revoked outside its owner";return {};}
        return id;
    }
    try{
        RetailWorldGpuTextureEntry entry;entry.texture=texture;entry.raster=*rh;entry.width=w;entry.height=h;
        entry.filter=tobs->filter;entry.address_u=tobs->address_u;entry.address_v=tobs->address_v;
        entry.source_bytes.assign(pixels->begin(),pixels->begin()+count);
        // Older image generations remain owned until explicit cleanup: a host may
        // still have a borrowed old name bound in the state being restored.
        gpu_->textures.push_back(std::move(entry));
    }catch(...){
        (void)rasters.unlock_pixels(*rh,rf);f.kind=RetailRendererWorldError::allocation;f.operation="0412 reserve texture ownership before GPU allocation";return {};
    }
    auto& entry=gpu_->textures.back();GLuint id{};glGenTextures(1,&id);entry.gl_texture_id=id;
    if(!id||glGetError()!=GL_NO_ERROR){
        (void)rasters.unlock_pixels(*rh,rf);f.kind=RetailRendererWorldError::gpu_operation;
        f.operation="0412 GPU texture name allocation failed; tracked outcome retained";return {};
    }
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,tobs->filter==RetailRendererTextureFilter::nearest?GL_NEAREST:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,tobs->filter==RetailRendererTextureFilter::nearest?GL_NEAREST:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,tobs->address_u==RetailRendererTextureAddress::clamp?0x812f:GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,tobs->address_v==RetailRendererTextureAddress::clamp?0x812f:GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels->data());
    const bool uploaded=id&&glGetError()==GL_NO_ERROR;
    glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(prior_binding));const bool restored=glGetError()==GL_NO_ERROR;
    const bool unlocked=rasters.unlock_pixels(*rh,rf);
    if(!uploaded||!restored||!unlocked){f.kind=RetailRendererWorldError::gpu_operation;f.operation="0412 GPU upload/restoration failed; tracked texture retained for cleanup";return {};}
    entry.ready=true;return id;
#else
    f.kind=RetailRendererWorldError::gpu_unbound;f.operation="0412 native GPU texture upload unavailable";return {};
#endif
}

bool NativeRetailWorld::set_hud_submission(const RetailHudSubmission& batch,RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return false;
    if(!std::isfinite(batch.logical_width)||!std::isfinite(batch.logical_height)||batch.logical_width<=0||batch.logical_height<=0||
       !std::isfinite(2.0f/batch.logical_width)||!std::isfinite(2.0f/batch.logical_height)){
        f.kind=RetailRendererWorldError::invalid_hud;f.operation="HUD positive finite explicit logical extent required";return false;
    }
    for(const auto& q:batch.quads){
        const float values[]{q.rect.x,q.rect.y,q.rect.width,q.rect.height,q.rect.x+q.rect.width,q.rect.y+q.rect.height,
            q.uv.u0,q.uv.v0,q.uv.u1,q.uv.v1};
        for(float v:values)if(!std::isfinite(v)){f.kind=RetailRendererWorldError::invalid_hud;f.operation="HUD finite rectangle and UVs required";return false;}
        if(q.rect.width<0||q.rect.height<0){f.kind=RetailRendererWorldError::invalid_hud;f.operation="HUD rectangle extent must be nonnegative";return false;}
        if(!std::isfinite(q.rect.x/batch.logical_width)||!std::isfinite((q.rect.x+q.rect.width)/batch.logical_width)||
           !std::isfinite(q.rect.y/batch.logical_height)||!std::isfinite((q.rect.y+q.rect.height)/batch.logical_height)){
            f.kind=RetailRendererWorldError::invalid_hud;f.operation="HUD finite normalized native projection required";return false;
        }
        for(float v:q.color)if(!std::isfinite(v)||v<0||v>1){f.kind=RetailRendererWorldError::invalid_hud;f.operation="HUD explicit normalized straight-alpha color required";return false;}
    }
    try{auto owned=batch;hud_=std::move(owned);return true;}
    catch(...){f.kind=RetailRendererWorldError::allocation;f.operation="HUD native batch copy allocation failed";return false;}
}
void NativeRetailWorld::clear_hud_submission() noexcept {
    if(std::this_thread::get_id()==thread_)hud_.reset();
}

bool NativeRetailWorld::add_scene_object(RetailWorldSceneObject obj, RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return false;
    if (obj.id == 0 || obj.frame.empty() || obj.triangles.empty() || (obj.triangles.size() % 3) != 0 ||
        !std::isfinite(obj.bounding_radius) || obj.bounding_radius < 0.0f ||
        !std::isfinite(obj.local_center.x)||!std::isfinite(obj.local_center.y)||!std::isfinite(obj.local_center.z)) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 valid scene object id, frame, non-empty triangles, and finite radius required";
        return false;
    }
    for (const auto& existing : scene_objects_) {
        if (existing.id == obj.id) {
            f.kind = RetailRendererWorldError::reentrant;
            f.operation = "0412 duplicate scene object id";
            return false;
        }
    }
    if (!obj.bone_frames.empty() && !obj.inv_bind_matrices.empty() &&
        obj.bone_frames.size() != obj.inv_bind_matrices.size()) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 skin bone_frames and inv_bind_matrices size mismatch";
        return false;
    }
    const std::size_t bone_count = !obj.bone_frames.empty() ? obj.bone_frames.size() : obj.bone_matrices.size();
    for (const auto& v : obj.triangles) {
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z) ||
            !std::isfinite(v.normal.x) || !std::isfinite(v.normal.y) || !std::isfinite(v.normal.z) ||
            !std::isfinite(v.u) || !std::isfinite(v.v)) {
            f.kind = RetailRendererWorldError::invalid_handle;
            f.operation = "0412 non-finite scene vertex attribute";
            return false;
        }
        for (int k = 0; k < 4; ++k) {
            if(!std::isfinite(v.color[k])||v.color[k]<0||v.color[k]>1){
                f.kind=RetailRendererWorldError::invalid_handle;f.operation="0412 normalized finite scene color required";return false;
            }
            if (!std::isfinite(v.bone_weights[k]) || v.bone_weights[k] < 0.0f) {
                f.kind = RetailRendererWorldError::invalid_handle;
                f.operation = "0412 invalid negative or non-finite bone weight";
                return false;
            }
            if (v.bone_weights[k] > 0.0f && (bone_count == 0 || v.bone_indices[k] >= bone_count)) {
                f.kind = RetailRendererWorldError::invalid_handle;
                f.operation = "0412 skin bone index out of range";
                return false;
            }
        }
    }
    try {
        scene_objects_.push_back(std::move(obj));
        return true;
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 scene object allocation";
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 scene object insertion";
    }
    return false;
}

bool NativeRetailWorld::remove_scene_object(std::uint64_t id, RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return false;
    const auto it = std::find_if(scene_objects_.begin(), scene_objects_.end(),
        [id](const RetailWorldSceneObject& o) { return o.id == id; });
    if (it == scene_objects_.end()) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 scene object id not found";
        return false;
    }
    scene_objects_.erase(it);
    return true;
}

void NativeRetailWorld::clear_scene_objects() noexcept {
    if(std::this_thread::get_id()!=thread_||retired_)return;
    scene_objects_.clear();
}

std::optional<std::int32_t> NativeRetailWorld::add_bsp_sector(
    RetailWorldBspSector sector,
    RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return {};
    if (sector.id == 0 || !std::isfinite(sector.bsphere_radius) || sector.bsphere_radius < 0.0f) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 valid BSP sector id and finite bounding radius required";
        return {};
    }
    if (sector.is_leaf && (sector.triangles.size() % 3) != 0) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 BSP leaf sector triangles must be a multiple of 3";
        return {};
    }
    const float bounds[]{sector.bsphere_center.x,sector.bsphere_center.y,sector.bsphere_center.z,
        sector.bbox_min.x,sector.bbox_min.y,sector.bbox_min.z,sector.bbox_max.x,sector.bbox_max.y,sector.bbox_max.z};
    for(float value:bounds)if(!std::isfinite(value)){
        f.kind=RetailRendererWorldError::invalid_handle;f.operation="0412 finite BSP bounds required";return {};
    }
    for(const auto& v:sector.triangles){
        const float values[]{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.u,v.v};
        for(float value:values)if(!std::isfinite(value)){
            f.kind=RetailRendererWorldError::invalid_handle;f.operation="0412 finite BSP vertex required";return {};
        }
        for(float value:v.color)if(!std::isfinite(value)||value<0||value>1){
            f.kind=RetailRendererWorldError::invalid_handle;f.operation="0412 normalized BSP color required";return {};
        }
    }
    if(bsp_sectors_.size()>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())){
        f.kind=RetailRendererWorldError::allocation;f.operation="0412 native BSP index range exhausted";return {};
    }
    try {
        const auto idx = static_cast<std::int32_t>(bsp_sectors_.size());
        bsp_sectors_.push_back(std::move(sector));
        return idx;
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 BSP sector allocation";
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 BSP sector insertion";
    }
    return {};
}

void NativeRetailWorld::clear_bsp_sectors() noexcept {
    if(std::this_thread::get_id()!=thread_||retired_)return;
    bsp_sectors_.clear();
    bsp_root_sector_ = -1;
}

bool NativeRetailWorld::add_light(RetailWorldLight light, RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return false;
    if ((light.type!=RetailWorldLightType::ambient&&light.type!=RetailWorldLightType::directional)||light.id == 0 ||
        !std::isfinite(light.color[0]) || !std::isfinite(light.color[1]) || !std::isfinite(light.color[2]) ||
        light.color[0] < 0.0f || light.color[1] < 0.0f || light.color[2] < 0.0f) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 valid light id and finite non-negative color required";
        return false;
    }
    if (light.type == RetailWorldLightType::directional) {
        const float len2 = light.direction.x * light.direction.x +
                           light.direction.y * light.direction.y +
                           light.direction.z * light.direction.z;
        if (!std::isfinite(len2) || len2 <= 1e-12f) {
            f.kind = RetailRendererWorldError::invalid_handle;
            f.operation = "0412 directional light requires finite non-zero direction";
            return false;
        }
    }
    for (const auto& existing : lights_) {
        if (existing.id == light.id) {
            f.kind = RetailRendererWorldError::reentrant;
            f.operation = "0412 duplicate light id";
            return false;
        }
    }
    try {
        lights_.push_back(light);
        return true;
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 light allocation";
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 light insertion";
    }
    return false;
}

bool NativeRetailWorld::remove_light(std::uint64_t id, RetailRendererWorldFailure& f) noexcept {
    if(!access_world(f))return false;
    const auto it = std::find_if(lights_.begin(), lights_.end(),
        [id](const RetailWorldLight& l) { return l.id == id; });
    if (it == lights_.end()) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 light id not found";
        return false;
    }
    lights_.erase(it);
    return true;
}

void NativeRetailWorld::clear_lights() noexcept {
    if(std::this_thread::get_id()!=thread_||retired_)return;
    lights_.clear();
}

struct NativeRetailWorldRecord final {
    std::shared_ptr<NativeRetailWorld> world;
    bool destroying{false};
    bool retired{false};

    NativeRetailWorldRecord() = default;
};

NativeRetailRendererWorldModule::NativeRetailRendererWorldModule(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
    std::weak_ptr<NativeRetailRendererCore> core)
    : session_(std::move(session)),
      allocator_(std::move(allocator)),core_(std::move(core)) {}

NativeRetailRendererWorldModule::~NativeRetailRendererWorldModule() {
    retire();
}

bool NativeRetailRendererWorldModule::access(RetailRendererWorldFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != thread_) {
        f.kind = RetailRendererWorldError::wrong_thread;
        f.operation = "0412 thread affinity check";
        return false;
    }
    if (retired_) {
        f.kind = RetailRendererWorldError::retired;
        f.operation = "0412 world module retired";
        return false;
    }
    if (session_.expired()) {
        f.kind = RetailRendererWorldError::owner;
        f.operation = "0412 session lifetime expired";
        return false;
    }
    return true;
}

std::shared_ptr<NativeRetailWorldRecord> NativeRetailRendererWorldModule::validate(
    NativeRetailWorldHandle handle,
    RetailRendererWorldFailure& f) const noexcept {
    if (handle.empty()) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 empty world handle";
        return {};
    }
    auto owner = handle.owner_.lock();
    if (!owner || owner.get() != this) {
        f.kind = RetailRendererWorldError::foreign_owner;
        f.operation = "0412 foreign world owner";
        return {};
    }
    auto rec = handle.record_.lock();
    if (!rec || rec->retired) {
        f.kind = RetailRendererWorldError::retired_world;
        f.operation = "0412 retired world handle";
        return {};
    }
    return rec;
}

std::shared_ptr<NativeRetailRendererWorldModule> NativeRetailRendererWorldModule::create(
    NativeRetailRendererCore& core,
    RetailRendererWorldFailure& f) noexcept {
    RetailRendererCoreFailure cf;
    if (!core.access(cf)) {
        f.kind = RetailRendererWorldError::owner;
        f.operation = "0412 core access refusal";
        return {};
    }
    auto allocator = core.allocator_;
    if (!allocator) {
        f.kind = RetailRendererWorldError::allocator;
        f.operation = "0412 allocator missing";
        return {};
    }
    try {
        auto owner=NativeRetailRendererCore::for_session(core.session_.lock(),core.allocator_,core.content_,cf);
        if(!owner){f.kind=RetailRendererWorldError::owner;f.operation="0412 actual Core identity required";return {};}
        return std::shared_ptr<NativeRetailRendererWorldModule>(
            new NativeRetailRendererWorldModule(core.session_, std::move(allocator),owner));
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 world module allocation";
        return {};
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 world module constructor";
        return {};
    }
}

std::optional<NativeRetailWorldHandle> NativeRetailRendererWorldModule::create_world(
    RetailRendererWorldFailure& f) noexcept {
    if (!access(f)) return {};
    if (busy_) {
        f.kind = RetailRendererWorldError::reentrant;
        f.operation = "0412 reentrant world creation";
        return {};
    }
    struct Guard {
        bool& b;
        Guard(bool& b) : b(b) { b = true; }
        ~Guard() { b = false; }
    } guard(busy_);

    auto session = session_.lock();
    if (!session) {
        f.kind = RetailRendererWorldError::owner;
        f.operation = "0412 session expired";
        return {};
    }

    auto rec = heap_object<NativeRetailWorldRecord>(
        allocator_, f, [&](void* ptr) {
            return ::new (ptr) NativeRetailWorldRecord();
        });
    if (!rec) return {};

    try {
        rec->world = std::shared_ptr<NativeRetailWorld>(
            new NativeRetailWorld(session,core_));
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 native world object allocation";
        return {};
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 native world object constructor";
        return {};
    }

    NativeRetailWorldHandle handle(
        shared_from_this(),
        std::weak_ptr<NativeRetailWorldRecord>(rec));

    try {
        records_.push_back(rec);
    } catch (...) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 world record registration";
        return {};
    }

    return handle;
}

bool NativeRetailRendererWorldModule::destroy_world(
    NativeRetailWorldHandle handle,
    RetailRendererWorldFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;

    if (rec->destroying) return true;
    rec->destroying = true;

    auto it = std::find(records_.begin(), records_.end(), rec);
    if (it != records_.end()) {
        records_.erase(it);
    }

    if(rec->world)rec->world->retire_world();
    rec->world.reset();
    rec->retired = true;
    return true;
}

std::shared_ptr<NativeRetailWorld> NativeRetailRendererWorldModule::resolve_world(
    NativeRetailWorldHandle handle,
    RetailRendererWorldFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    return rec->world;
}

std::size_t NativeRetailRendererWorldModule::active_count() const noexcept {
    return records_.size();
}

void NativeRetailRendererWorldModule::retire() noexcept {
    if (retired_) return;
    retired_ = true;
    for (auto& r : records_) {
        if (r) {
            r->retired = true;
            if(r->world)r->world->retire_world();
            r->world.reset();
        }
    }
    records_.clear();
}

std::shared_ptr<NativeRetailRendererWorldModule> NativeRetailRendererCore::world_module(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return world_module_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_0412_count(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return module_0412_count_.load(std::memory_order_relaxed);
}

namespace renderer_required {

bool construct_core_plugin_0412(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
    if (!core.access(f)) return false;
    if (core.world_module_) {
        f.kind = RetailRendererCoreError::reentrant;
        f.operation = "0412 duplicate plugin registration";
        return false;
    }
    RetailRendererWorldFailure wf;
    auto owner = NativeRetailRendererWorldModule::create(core, wf);
    if (!owner) {
        f.kind = RetailRendererCoreError::construction;
        f.operation = wf.operation ? wf.operation : "0412 construction failure";
        return false;
    }
    core.world_module_ = std::move(owner);
    core.module_0412_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void destroy_core_plugin_0412(NativeRetailRendererCore& core) noexcept {
    if (core.world_module_) {
        core.world_module_->retire();
        core.world_module_.reset();
        core.module_0412_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace renderer_required

} // namespace mka::retail
