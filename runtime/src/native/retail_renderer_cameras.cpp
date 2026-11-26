#include "mka/retail_renderer_cameras.hpp"
#include "mka/retail_game_session.hpp"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#endif
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace mka::retail {
namespace {
bool fail(RetailRendererCameraFailure& f,RetailRendererCameraError kind,const char* operation)noexcept{
 f.kind=kind;f.operation=operation;return false;
}
struct Busy {bool& flag;explicit Busy(bool& x):flag(x){flag=true;}~Busy(){flag=false;}};
float value(std::uint32_t bits)noexcept{return std::bit_cast<float>(bits);}
// Force a binary32 result at each original scalar SSE operation. No fused
// expression, EE rounding model, register emulation or changed FP settings.
float add32(float a,float b)noexcept{volatile float r=a+b;return r;}
float sub32(float a,float b)noexcept{volatile float r=a-b;return r;}
float mul32(float a,float b)noexcept{volatile float r=a*b;return r;}
float div32(float a,float b)noexcept{volatile float r=a/b;return r;}
bool default_depth(const RetailRendererCameraDepthInputs& input,float& scale,float& shift)noexcept{
 static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
 if(!std::isfinite(input.near_value)||!std::isfinite(input.far_value))return false;
 const auto delta=sub32(input.far_value,input.near_value);
 const auto margin=mul32(delta,value(0x38d1b717u));
 const auto hi=sub32(input.far_value,margin);
 const auto lo=add32(margin,input.near_value);
 const auto span=sub32(hi,lo);
 scale=mul32(span,value(0xbd4dd443u));
 const auto product=mul32(scale,value(0x41a0cccdu));
 const auto sum=add32(hi,lo);
 const auto difference=sub32(sum,product);
 shift=mul32(difference,value(0x3f000000u));
 return std::isfinite(delta)&&std::isfinite(margin)&&std::isfinite(hi)&&std::isfinite(lo)&&
  std::isfinite(scale)&&std::isfinite(shift);
}
template<class T,class Create>std::shared_ptr<T> heap_object(
 const std::shared_ptr<NativeRetailRendererAllocatorService>& allocator,RetailRendererCameraFailure& f,Create create)noexcept{
 static_assert(alignof(T)<=16);auto storage=allocator->allocate(sizeof(T),f.allocator);
 if(!storage){fail(f,RetailRendererCameraError::allocator,"0405 actual typed native allocation");return {};}
 try{
  auto* object=create(storage->data());
  return std::shared_ptr<T>(object,[heap=std::shared_ptr<NativeRetailRendererAllocatorService>(allocator),
      storage=std::move(*storage)](T* p)mutable{p->~T();storage=memory::NativeAllocation{};heap.reset();});
 }catch(const std::bad_alloc&){fail(f,RetailRendererCameraError::allocation,"0405 native control ownership");}
 catch(...){fail(f,RetailRendererCameraError::native_exception,"0405 native typed construction");}
 return {};
}
}
struct NativeRetailRendererBaseCameraRecord {
 RetailRendererBaseCameraObservation state;
 bool alive{true};
 std::weak_ptr<NativeRetailRendererCameraModule> owner;
 std::weak_ptr<NativeRetailRendererBaseCameraRecord> self;
 NativeRetailRasterHandle frame_buffer;
 NativeRetailRasterHandle z_buffer;
 unsigned int fbo{0};
 bool fbo_initialized{false};
 bool updating{false};
 NativeRetailRendererFrameObjectLink frame_link;
 NativeRetailRendererBaseCameraRecord(std::weak_ptr<NativeRetailGameSession> session,
     const RetailRendererCameraDepthInputs& depth,float scale,float shift)noexcept
  :state{{1,1},{1,1},{0,0},value(0x3d4ccccdu),value(0x41200000u),value(0x40a00000u),
    scale,shift,1,depth,false},frame_link(std::move(session)){}
 ~NativeRetailRendererBaseCameraRecord() {
#ifdef _WIN32
  typedef void (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
  static auto p_glDeleteFramebuffers = reinterpret_cast<PFNGLDELETEFRAMEBUFFERSPROC>(wglGetProcAddress("glDeleteFramebuffers"));
  if (fbo != 0 && p_glDeleteFramebuffers && wglGetCurrentContext()) {
   p_glDeleteFramebuffers(1, &fbo);
   fbo = 0;
  }
#endif
 }
};
NativeRetailRendererCameraModule::NativeRetailRendererCameraModule(
 std::weak_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator)noexcept
 :session_(std::move(session)),allocator_(std::move(allocator)){}
NativeRetailRendererCameraModule::~NativeRetailRendererCameraModule(){retire();}
std::shared_ptr<NativeRetailRendererCameraModule> NativeRetailRendererCameraModule::create(
 std::weak_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
 RetailRendererCameraFailure& f)noexcept{
 f={};const auto lease=session.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)||!allocator){fail(f,RetailRendererCameraError::owner,"0405 actual Session/Permanent ownership required");return {};}
 return heap_object<NativeRetailRendererCameraModule>(allocator,f,[&](void* p){
  return new(p)NativeRetailRendererCameraModule(session,allocator);
 });
}
std::shared_ptr<NativeRetailGameSession> NativeRetailRendererCameraModule::admit(RetailRendererCameraFailure& f)const noexcept{
 f={};
 if(std::this_thread::get_id()!=thread_){fail(f,RetailRendererCameraError::wrong_thread,"0405 owner thread");return {};}
 if(!active_.load(std::memory_order_acquire)){fail(f,RetailRendererCameraError::retired,"0405 actual camera resource retired");return {};}
 auto lease=session_.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)){fail(f,RetailRendererCameraError::owner,"0405 actual Session lifetime");return {};}
 if(busy_){fail(f,RetailRendererCameraError::reentrant,"0405 resource operation active");return {};}
 return lease;
}
std::shared_ptr<NativeRetailRendererBaseCameraRecord> NativeRetailRendererCameraModule::record(
 const NativeRetailRendererCameraHandle& h,RetailRendererCameraFailure& f)const noexcept{
 if(h.empty()){fail(f,RetailRendererCameraError::invalid_handle,"0405 empty native camera handle");return {};}
 if(h.owner_.lock().get()!=this){fail(f,RetailRendererCameraError::foreign_owner,"0405 foreign camera resource");return {};}
 auto r=h.record_.lock();
 if(!r||!r->alive){fail(f,RetailRendererCameraError::retired_camera,"0405 camera allocation retired");return {};}
 return r;
}
std::optional<NativeRetailRendererCameraHandle> NativeRetailRendererCameraModule::create_base_camera(
 const std::optional<RetailRendererCameraDepthInputs>& depth,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return {};
 if(!depth){fail(f,RetailRendererCameraError::device_depth_required,"0405 actual renderer depth inputs required; no default device");return {};}
 float scale{},shift{};
 if(!default_depth(*depth,scale,shift)){fail(f,RetailRendererCameraError::unverified_numeric_domain,"0405 nonfinite depth domain remains unverified");return {};}
 Busy guard(busy_);
 auto r=heap_object<NativeRetailRendererBaseCameraRecord>(allocator_,f,[&](void* p){
  return new(p)NativeRetailRendererBaseCameraRecord(session_,*depth,scale,shift);
 });
 if(!r)return {};
 try{
  if(records_.size()==records_.max_size()){fail(f,RetailRendererCameraError::allocation,"0405 native host ownership capacity");return {};}
  if(records_.size()==records_.capacity()){
   const auto maximum=records_.max_size();
   records_.reserve(records_.empty()?1:records_.size()<=maximum/2?records_.size()*2:maximum);
  }
  if(!active_.load(std::memory_order_acquire)){fail(f,RetailRendererCameraError::retired,"0405 retired during native allocation");return {};}
  r->owner=weak_from_this();r->self=r;
  r->frame_link.self_=std::shared_ptr<NativeRetailRendererFrameObjectLink>(r,&r->frame_link);
  const NativeRetailRendererCameraHandle result(r->owner,r->self);records_.push_back(std::move(r));return result;
 }catch(const std::bad_alloc&){fail(f,RetailRendererCameraError::allocation,"0405 native camera ownership growth");}
 catch(...){fail(f,RetailRendererCameraError::native_exception,"0405 camera publication");}
 return {};
}
std::optional<RetailRendererBaseCameraObservation> NativeRetailRendererCameraModule::observe(
 const NativeRetailRendererCameraHandle& h,RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};return r->state;
}
std::optional<std::size_t> NativeRetailRendererCameraModule::live_base_cameras(RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};return records_.size();
}
bool NativeRetailRendererCameraModule::set_frame(const NativeRetailRendererCameraHandle& h,
 const NativeRetailRendererFrameHandle& frame,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 Busy guard(busy_);
 if(frame.empty()&&r->frame_link.frame_.empty()){
  r->frame_link.frame_=frame; // 1AA0C0 still stores the nullable parent.
  return true;
 }
 const auto frames=frame.empty()?r->frame_link.owner_.lock():frame.owner_.lock();
 if(!frames){
  f.frame={RetailRendererFrameError::retired_frame,"1AA0B0 native frame owner retired"};
  return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 }
 if(!frames->set_object_frame(r->frame_link,frame,f.frame))
  return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 return true;
}
std::optional<NativeRetailRendererFrameHandle> NativeRetailRendererCameraModule::frame(
 const NativeRetailRendererCameraHandle& h,RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 if(r->frame_link.frame_.empty())return NativeRetailRendererFrameHandle{};
 const auto frames=r->frame_link.owner_.lock();
 if(!frames){
  f.frame={RetailRendererFrameError::retired_frame,"native camera frame owner retired"};
  fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);return {};
 }
 auto result=frames->object_frame(r->frame_link,f.frame);
 if(!result)fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 return result;
}
std::optional<NativeRetailRendererFrameObjectHandle> NativeRetailRendererCameraModule::frame_object_identity(
 const NativeRetailRendererCameraHandle& h,RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 return NativeRetailRendererFrameObjectHandle(r->frame_link.self_);
}
bool NativeRetailRendererCameraModule::set_view_window(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraWindow input,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 Busy guard(busy_);
 // The input is a native value copy. Original1A64A0 loads both components
 // before either store, then writes both reciprocals before its parent call.
 const auto one=value(0x3f800000u);
 std::memcpy(&r->state.view_window.x,&input.x,sizeof(input.x));
 std::memcpy(&r->state.view_window.y,&input.y,sizeof(input.y));
 r->state.inverse_window.x=div32(one,r->state.view_window.x);
 r->state.inverse_window.y=div32(one,r->state.view_window.y);
 if(!r->frame_link.frame_.empty()){
  const auto frames=r->frame_link.owner_.lock();
  if(!frames){
   f.frame={RetailRendererFrameError::retired_frame,"1A64A0 attached native frame owner retired"};
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
  }
  if(!frames->update_object_frame(r->frame_link,f.frame))
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 }
 return true;
}
bool NativeRetailRendererCameraModule::destroy_base_camera(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 // Initial-empty extension registry:1AA120 unlinks before actual release,
 // without clearing the parent's value or executing a sync callback.
 if(auto frames=r->frame_link.owner_.lock())frames->release_object_link(r->frame_link);
 r->alive=false;const auto i=std::find_if(records_.begin(),records_.end(),[&](const auto& x){return x.get()==r.get();});
 records_.erase(i);return true;
}
bool NativeRetailRendererCameraModule::require_render_ready(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return false;if(!record(h,f))return false;
 return fail(f,RetailRendererCameraError::render_services_required,
  "0405 real camera plugins/frame/raster/view/frustum synchronization and device begin/end required");
}

bool NativeRetailRendererCameraModule::set_frame_buffer_raster(const NativeRetailRendererCameraHandle& h,
 const NativeRetailRasterHandle& raster, RetailRendererCameraFailure& f) noexcept {
 const auto lease = admit(f); if (!lease) return false;
 const auto r = record(h, f); if (!r) return false;
 if (r->updating) { fail(f, RetailRendererCameraError::reentrant, "0405 cannot change frame buffer while camera is updating"); return false; }
 r->frame_buffer = raster;
 return true;
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererCameraModule::frame_buffer_raster(
 const NativeRetailRendererCameraHandle& h, RetailRendererCameraFailure& f) const noexcept {
 const auto lease = admit(f); if (!lease) return {};
 const auto r = record(h, f); if (!r) return {};
 return r->frame_buffer;
}

bool NativeRetailRendererCameraModule::set_z_buffer_raster(const NativeRetailRendererCameraHandle& h,
 const NativeRetailRasterHandle& raster, RetailRendererCameraFailure& f) noexcept {
 const auto lease = admit(f); if (!lease) return false;
 const auto r = record(h, f); if (!r) return false;
 if (r->updating) { fail(f, RetailRendererCameraError::reentrant, "0405 cannot change z buffer while camera is updating"); return false; }
 r->z_buffer = raster;
 return true;
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererCameraModule::z_buffer_raster(
 const NativeRetailRendererCameraHandle& h, RetailRendererCameraFailure& f) const noexcept {
 const auto lease = admit(f); if (!lease) return {};
 const auto r = record(h, f); if (!r) return {};
 return r->z_buffer;
}

bool NativeRetailRendererCameraModule::is_updating(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f) const noexcept {
 const auto lease = admit(f); if (!lease) return false;
 const auto r = record(h, f); if (!r) return false;
 return r->updating;
}

bool NativeRetailRendererCameraModule::begin_update(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f) noexcept {
 const auto lease = admit(f); if (!lease) return false;
 const auto r = record(h, f); if (!r) return false;
 if (r->updating) { fail(f, RetailRendererCameraError::reentrant, "0405 camera already in begin_update lifecycle"); return false; }
 if (r->frame_buffer.empty()) { fail(f, RetailRendererCameraError::render_services_required, "0405 frame buffer raster attachment required for begin_update"); return false; }

#ifdef _WIN32
 typedef void (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
 typedef void (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
 static auto p_glGenFramebuffers = reinterpret_cast<PFNGLGENFRAMEBUFFERSPROC>(wglGetProcAddress("glGenFramebuffers"));
 static auto p_glBindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(wglGetProcAddress("glBindFramebuffer"));
 if (p_glGenFramebuffers && p_glBindFramebuffer && wglGetCurrentContext()) {
  if (!r->fbo_initialized) {
   p_glGenFramebuffers(1, &r->fbo);
   r->fbo_initialized = true;
  }
  if (r->fbo != 0) {
   p_glBindFramebuffer(0x8D40 /* GL_FRAMEBUFFER */, r->fbo);
  }
 }
#endif
 r->updating = true;
 return true;
}

bool NativeRetailRendererCameraModule::clear(const NativeRetailRendererCameraHandle& h,
 float red, float green, float blue, float alpha, std::uint32_t clear_flags, RetailRendererCameraFailure& f) noexcept {
 const auto lease = admit(f); if (!lease) return false;
 const auto r = record(h, f); if (!r) return false;
 if (!r->updating) { fail(f, RetailRendererCameraError::render_services_required, "0405 clear requires active camera begin_update lifecycle"); return false; }

#ifdef _WIN32
 if (wglGetCurrentContext()) {
  glClearColor(red, green, blue, alpha);
  GLbitfield mask = 0;
  if (clear_flags & 0x01) mask |= GL_COLOR_BUFFER_BIT;
  if (clear_flags & 0x02) mask |= GL_DEPTH_BUFFER_BIT;
  if (mask != 0) glClear(mask);
 }
#else
 (void)red; (void)green; (void)blue; (void)alpha; (void)clear_flags;
#endif
 return true;
}

bool NativeRetailRendererCameraModule::end_update(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f) noexcept {
 const auto lease = admit(f); if (!lease) return false;
 const auto r = record(h, f); if (!r) return false;
 if (!r->updating) { fail(f, RetailRendererCameraError::render_services_required, "0405 end_update called without active begin_update"); return false; }

#ifdef _WIN32
 typedef void (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
 static auto p_glBindFramebuffer = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(wglGetProcAddress("glBindFramebuffer"));
 if (p_glBindFramebuffer && wglGetCurrentContext()) {
  p_glBindFramebuffer(0x8D40 /* GL_FRAMEBUFFER */, 0);
 }
#endif
 r->updating = false;
 return true;
}
void NativeRetailRendererCameraModule::retire()noexcept{
 active_.store(false,std::memory_order_release);
 for(const auto& r:records_){
  if(auto frames=r->frame_link.owner_.lock())frames->release_object_link(r->frame_link);
  r->alive=false;
 }
 records_.clear();
}
std::shared_ptr<NativeRetailRendererCameraModule> NativeRetailRendererCore::camera_module(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};
 if(!camera_module_){f.kind=RetailRendererCoreError::unbound_factory;f.operation="0405 actual camera resource not constructed";return {};}
 return camera_module_;
}
std::optional<std::uint32_t> NativeRetailRendererCore::module_0405_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return module_0405_count_.load(std::memory_order_relaxed);
}
namespace renderer_required {
bool construct_core_plugin_0405(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!core.access(f))return false;
 if(core.camera_module_){f.kind=RetailRendererCoreError::reentrant;f.operation="0405 actual camera resource already constructed";return false;}
 RetailRendererCameraFailure detail;auto resource=NativeRetailRendererCameraModule::create(core.session_,core.allocator_,detail);
 if(!resource){
  f.kind=detail.kind==RetailRendererCameraError::allocation||detail.allocator.heap.kind==memory::NativeHeapError::allocation_failed?
   RetailRendererCoreError::allocation:detail.kind==RetailRendererCameraError::native_exception?
   RetailRendererCoreError::native_exception:RetailRendererCoreError::allocator;
  f.operation=detail.operation;f.allocator=detail.allocator;return false;
 }
 core.camera_module_=std::move(resource);core.module_0405_count_.fetch_add(1,std::memory_order_relaxed);return true;
}
void destroy_core_plugin_0405(NativeRetailRendererCore& core)noexcept{
 if(core.camera_module_){core.camera_module_->retire();core.camera_module_.reset();core.module_0405_count_.fetch_sub(1,std::memory_order_relaxed);}
}
}
}
