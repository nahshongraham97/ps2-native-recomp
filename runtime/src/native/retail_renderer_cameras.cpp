#include "mka/retail_renderer_cameras.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_renderer_world.hpp"
#include "mka/retail_game_session.hpp"
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
bool compute_view_matrix(const RetailRendererAffine& world,RetailRendererCameraMatrix4x4& out)noexcept{
 const float r00=world.right.x,r01=world.right.y,r02=world.right.z;
 const float r10=world.up.x,r11=world.up.y,r12=world.up.z;
 const float r20=world.at.x,r21=world.at.y,r22=world.at.z;
 const float c00=r11*r22-r12*r21;
 const float c01=-(r10*r22-r12*r20);
 const float c02=r10*r21-r11*r20;
 const float det=r00*c00+r01*c01+r02*c02;
 if(!std::isfinite(det)||std::fabs(det)<1e-12f)return false;
 const float inv_det=1.0f/det;
 // R^{-1} = adj(R) / det = cofactor(R)^T / det.
 const float i00=c00*inv_det;
 const float i01=-(r01*r22-r02*r21)*inv_det;
 const float i02=(r01*r12-r02*r11)*inv_det;
 const float i10=c01*inv_det;
 const float i11=(r00*r22-r02*r20)*inv_det;
 const float i12=-(r00*r12-r02*r10)*inv_det;
 const float i20=c02*inv_det;
 const float i21=-(r00*r21-r01*r20)*inv_det;
 const float i22=(r00*r11-r01*r10)*inv_det;
 const float tx=world.translation.x,ty=world.translation.y,tz=world.translation.z;
 // Column-major: view * P_world = (P_world - T) * R^{-1}
 out.elements={
  i00,i01,i02,0.0f,
  i10,i11,i12,0.0f,
  i20,i21,i22,0.0f,
  -(tx*i00+ty*i10+tz*i20),
  -(tx*i01+ty*i11+tz*i21),
  -(tx*i02+ty*i12+tz*i22),
  1.0f
 };
 for(float v:out.elements)if(!std::isfinite(v))return false;
 return true;
}
bool compute_projection_and_frustum(const RetailRendererBaseCameraObservation& state,
 const RetailRendererCameraMatrix4x4& view,
 RetailRendererCameraMatrix4x4& proj,
 std::array<RetailRendererCameraPlane,6>& planes)noexcept{
 const float near_c=state.near_clip,far_c=state.far_clip;
 const float span=far_c-near_c;
 if(!std::isfinite(near_c)||!std::isfinite(far_c)||near_c<=0.0f||span<=0.0f)return false;
 if(!std::isfinite(state.inverse_window.x)||!std::isfinite(state.inverse_window.y))return false;
 if(state.projection_type==1u){
  proj.elements={
   state.inverse_window.x,0.0f,0.0f,0.0f,
   0.0f,state.inverse_window.y,0.0f,0.0f,
   0.0f,0.0f,(far_c+near_c)/span,1.0f,
   0.0f,0.0f,-(2.0f*far_c*near_c)/span,0.0f
  };
 }else if(state.projection_type==2u){
  proj.elements={
   state.inverse_window.x,0.0f,0.0f,0.0f,
   0.0f,state.inverse_window.y,0.0f,0.0f,
   0.0f,0.0f,2.0f/span,0.0f,
   0.0f,0.0f,-(far_c+near_c)/span,1.0f
  };
 }else{
  return false;
 }
 std::array<float,16> vp{};
 for(int c=0;c<4;++c){
  for(int r=0;r<4;++r){
   float sum=0.0f;
   for(int k=0;k<4;++k)sum+=proj.elements[k*4+r]*view.elements[c*4+k];
   vp[c*4+r]=sum;
  }
 }
 auto normalize_plane=[](float a,float b,float c,float d,RetailRendererCameraPlane& out)noexcept{
  const float len=std::sqrt(a*a+b*b+c*c);
  if(!std::isfinite(len)||len<=1e-12f)return false;
  const float inv=1.0f/len;
  out={a*inv,b*inv,c*inv,d*inv};
  return std::isfinite(out.a)&&std::isfinite(out.b)&&std::isfinite(out.c)&&std::isfinite(out.d);
 };
 const auto row=[&](int r){
  return std::array<float,4>{vp[0*4+r],vp[1*4+r],vp[2*4+r],vp[3*4+r]};
 };
 const auto r0=row(0),r1=row(1),r2=row(2),r3=row(3);
 return normalize_plane(r3[0]+r0[0],r3[1]+r0[1],r3[2]+r0[2],r3[3]+r0[3],planes[0])&&
  normalize_plane(r3[0]-r0[0],r3[1]-r0[1],r3[2]-r0[2],r3[3]-r0[3],planes[1])&&
  normalize_plane(r3[0]+r1[0],r3[1]+r1[1],r3[2]+r1[2],r3[3]+r1[3],planes[2])&&
  normalize_plane(r3[0]-r1[0],r3[1]-r1[1],r3[2]-r1[2],r3[3]-r1[3],planes[3])&&
  normalize_plane(r3[0]+r2[0],r3[1]+r2[1],r3[2]+r2[2],r3[3]+r2[3],planes[4])&&
  normalize_plane(r3[0]-r2[0],r3[1]-r2[1],r3[2]-r2[2],r3[3]-r2[3],planes[5]);
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
struct NativeRetailRendererBaseCameraRecord : NativeDisplayDrawOwner {
 RetailRendererBaseCameraObservation state;
 bool alive{true};
 std::weak_ptr<NativeRetailRendererCameraModule> owner;
 std::weak_ptr<NativeRetailRendererBaseCameraRecord> self;
 NativeRetailRasterHandle frame_buffer;
 NativeRetailRasterHandle z_buffer;
 bool updating{false};
 std::optional<RetailRendererCameraTargetPolicy> target_policy;
 std::shared_ptr<NativeProductionDisplayService> active_display;
 NativeRetailRendererFrameObjectLink frame_link;
 NativeRetailRendererBaseCameraRecord(std::weak_ptr<NativeRetailGameSession> session,
     const RetailRendererCameraDepthInputs& depth,float scale,float shift)noexcept
  :state{{1,1},{1,1},{0,0},value(0x3d4ccccdu),value(0x41200000u),value(0x40a00000u),
    scale,shift,1,depth,false},frame_link(std::move(session)){}
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
 r->state.synchronized_view_available=false;
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
bool NativeRetailRendererCameraModule::set_near_clip(const NativeRetailRendererCameraHandle& h,
 float near_clip,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(!std::isfinite(near_clip)||near_clip<=0.0f||near_clip>=r->state.far_clip)
  return fail(f,RetailRendererCameraError::unverified_numeric_domain,"1A6270 near clip must be finite, positive, and less than far clip");
 Busy guard(busy_);
 r->state.near_clip=near_clip;
 r->state.synchronized_view_available=false;
 if(!r->frame_link.frame_.empty()){
  const auto frames=r->frame_link.owner_.lock();
  if(!frames){
   f.frame={RetailRendererFrameError::retired_frame,"1A6270 attached native frame owner retired"};
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
  }
  if(!frames->update_object_frame(r->frame_link,f.frame))
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 }
 return true;
}
bool NativeRetailRendererCameraModule::set_far_clip(const NativeRetailRendererCameraHandle& h,
 float far_clip,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(!std::isfinite(far_clip)||far_clip<=r->state.near_clip)
  return fail(f,RetailRendererCameraError::unverified_numeric_domain,"1A62B0 far clip must be finite and greater than near clip");
 Busy guard(busy_);
 r->state.far_clip=far_clip;
 r->state.synchronized_view_available=false;
 if(!r->frame_link.frame_.empty()){
  const auto frames=r->frame_link.owner_.lock();
  if(!frames){
   f.frame={RetailRendererFrameError::retired_frame,"1A62B0 attached native frame owner retired"};
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
  }
  if(!frames->update_object_frame(r->frame_link,f.frame))
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 }
 return true;
}
bool NativeRetailRendererCameraModule::set_projection(const NativeRetailRendererCameraHandle& h,
 std::uint32_t projection_type,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(projection_type!=1u&&projection_type!=2u)
  return fail(f,RetailRendererCameraError::unverified_numeric_domain,"1A6410 projection type must be perspective (1) or parallel (2)");
 Busy guard(busy_);
 r->state.projection_type=projection_type;
 r->state.synchronized_view_available=false;
 if(!r->frame_link.frame_.empty()){
  const auto frames=r->frame_link.owner_.lock();
  if(!frames){
   f.frame={RetailRendererFrameError::retired_frame,"1A6410 attached native frame owner retired"};
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
  }
  if(!frames->update_object_frame(r->frame_link,f.frame))
   return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 }
 return true;
}
bool NativeRetailRendererCameraModule::set_fov(const NativeRetailRendererCameraHandle& h,
 float fov_degrees,float aspect_ratio,RetailRendererCameraFailure& f)noexcept{
 if(!std::isfinite(fov_degrees)||fov_degrees<=0.0f||fov_degrees>=180.0f||
   !std::isfinite(aspect_ratio)||aspect_ratio<=0.0f){
  const auto lease=admit(f);if(!lease)return false;
  return fail(f,RetailRendererCameraError::unverified_numeric_domain,"17EF50 FOV must be in (0, 180) and aspect ratio positive");
 }
 constexpr float kPi=3.14159265358979323846f;
 const float w=std::tan(fov_degrees*(kPi/360.0f));
 const float h_win=div32(w,aspect_ratio);
 return set_view_window(h,{w,h_win},f);
}
std::optional<std::uint64_t> NativeRetailRendererCameraModule::register_sync_callback(
 RetailRendererCameraSyncCallback cb,void* user_data,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return {};
 if(!cb){fail(f,RetailRendererCameraError::render_services_required,"camera sync callback cannot be null");return {};}
 Busy guard(busy_);
 try{
  const std::uint64_t token=next_callback_token_++;
  sync_callbacks_.push_back({token,cb,user_data});
  return token;
 }catch(const std::bad_alloc&){fail(f,RetailRendererCameraError::allocation,"camera sync callback registration allocation");}
 catch(...){fail(f,RetailRendererCameraError::native_exception,"camera sync callback registration");}
 return {};
}
bool NativeRetailRendererCameraModule::unregister_sync_callback(std::uint64_t token,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;
 Busy guard(busy_);
 const auto it=std::find_if(sync_callbacks_.begin(),sync_callbacks_.end(),
  [token](const SyncCallbackEntry& e){return e.token==token;});
 if(it==sync_callbacks_.end())
  return fail(f,RetailRendererCameraError::invalid_handle,"camera sync callback token not registered");
 sync_callbacks_.erase(it);
 return true;
}
bool NativeRetailRendererCameraModule::synchronize(const NativeRetailRendererCameraHandle& h,
 NativeRetailRendererFrames& frames,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->frame_link.frame_.empty()||r->frame_link.owner_.lock().get()!=&frames)
  return fail(f,RetailRendererCameraError::frame_binding,"camera synchronize requires bound frame from the same Frames module");
 if(!frames.synchronize_dirty(f.frame))
  return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 const auto frame_obs=frames.observe(r->frame_link.frame_,f.frame);
 if(!frame_obs)return fail(f,RetailRendererCameraError::frame_binding,f.frame.operation);
 RetailRendererCameraMatrix4x4 view_mat;
 if(!compute_view_matrix(frame_obs->world.affine,view_mat))
  return fail(f,RetailRendererCameraError::unverified_numeric_domain,"singular or non-finite frame world matrix");
 RetailRendererCameraMatrix4x4 proj_mat;
 std::array<RetailRendererCameraPlane,6> planes{};
 if(!compute_projection_and_frustum(r->state,view_mat,proj_mat,planes))
  return fail(f,RetailRendererCameraError::unverified_numeric_domain,"invalid projection or frustum parameters");
 std::vector<SyncCallbackEntry> callbacks_snapshot;
 try{
  callbacks_snapshot=sync_callbacks_;
 }catch(...){
  return fail(f,RetailRendererCameraError::allocation,"camera sync callback snapshot allocation");
 }
 {
  Busy guard(busy_);
  r->state.view_matrix=view_mat;
  r->state.projection_matrix=proj_mat;
  r->state.frustum_planes=planes;
  r->state.synchronized_view_available=true;
 }
 for(const auto& entry:callbacks_snapshot){
  if(!entry.callback(h,r->state,entry.user_data)){
   r->state.synchronized_view_available=false;
   return fail(f,RetailRendererCameraError::render_services_required,"camera sync callback rejected synchronization");
  }
 }
 return true;
}
bool NativeRetailRendererCameraModule::destroy_base_camera(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->updating){
  if(!r->active_display||!r->active_display->end_render_target(*r,f.display))
   return fail(f,RetailRendererCameraError::native_target,"end actual native draw target before camera release");
  r->updating=false;r->active_display.reset();
 }
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

std::shared_ptr<NativeRetailRendererRasters> NativeRetailRendererCameraModule::raster_owner(
 const NativeRetailRasterHandle& raster,const std::shared_ptr<NativeRetailGameSession>& session,
 bool depth,RetailRendererCameraFailure& f)const noexcept{
 const auto owner=raster.owner_.lock();
 if(!owner||owner->session_.lock()!=session){
  fail(f,RetailRendererCameraError::raster_binding,"live raster owner from the camera Session required");return {};
 }
 auto observed=owner->observe(raster,f.raster);
 if(!observed){fail(f,RetailRendererCameraError::raster_binding,f.raster.operation);return {};}
 const auto kind=observed->description.type;
 if((depth&&kind!=1)||(!depth&&kind!=2&&kind!=5)){
  fail(f,RetailRendererCameraError::raster_binding,"raster role does not match camera color/depth field");return {};
 }
 return owner;
}
bool NativeRetailRendererCameraModule::set_frame_buffer_raster(const NativeRetailRendererCameraHandle& h,
 const NativeRetailRasterHandle& raster,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->updating)return fail(f,RetailRendererCameraError::reentrant,"cannot change an active native color target");
 if(!raster.empty()&&!raster_owner(raster,lease,false,f))return false;
 r->frame_buffer=raster;return true;
}
std::optional<NativeRetailRasterHandle> NativeRetailRendererCameraModule::frame_buffer_raster(
 const NativeRetailRendererCameraHandle& h,RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 if(!r->frame_buffer.empty()&&!raster_owner(r->frame_buffer,lease,false,f))return {};
 return r->frame_buffer;
}
bool NativeRetailRendererCameraModule::set_z_buffer_raster(const NativeRetailRendererCameraHandle& h,
 const NativeRetailRasterHandle& raster,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->updating)return fail(f,RetailRendererCameraError::reentrant,"cannot change an active native depth target");
 if(!raster.empty()&&!raster_owner(raster,lease,true,f))return false;
 r->z_buffer=raster;return true;
}
std::optional<NativeRetailRasterHandle> NativeRetailRendererCameraModule::z_buffer_raster(
 const NativeRetailRendererCameraHandle& h,RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 if(!r->z_buffer.empty()&&!raster_owner(r->z_buffer,lease,true,f))return {};
 return r->z_buffer;
}
bool NativeRetailRendererCameraModule::set_render_target_policy(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraTargetPolicy policy,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->updating)return fail(f,RetailRendererCameraError::reentrant,"cannot change active native target policy");
 if(!policy.physical_viewport.width||!policy.physical_viewport.height||
   (policy.clear_depth&&(!std::isfinite(*policy.clear_depth)||*policy.clear_depth<0||*policy.clear_depth>1)))
  return fail(f,RetailRendererCameraError::native_target,"explicit physical viewport and normalized native clear-depth policy required");
 r->target_policy=policy;return true;
}
bool NativeRetailRendererCameraModule::is_updating(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;return r->updating;
}
bool NativeRetailRendererCameraModule::begin_update(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->updating)return fail(f,RetailRendererCameraError::reentrant,"native camera draw target already active");
 if(r->frame_buffer.empty()||!r->target_policy)
  return fail(f,RetailRendererCameraError::render_services_required,"real display raster and explicit native target policy required");
 const auto colors=raster_owner(r->frame_buffer,lease,false,f);if(!colors)return false;
 const auto color=colors->observe(r->frame_buffer,f.raster);if(!color)return fail(f,RetailRendererCameraError::raster_binding,f.raster.operation);
 if(color->description.width<=0||color->description.height<=0)
  return fail(f,RetailRendererCameraError::raster_binding,"positive logical display raster extent required before GPU begin");
 if(color->description.type!=2)
  return fail(f,RetailRendererCameraError::render_services_required,"camera-texture target requires its native render-to-texture service");
 auto display=colors->display_target(r->frame_buffer,f.raster);
 if(!display)return fail(f,RetailRendererCameraError::raster_binding,f.raster.operation);
 if(!r->z_buffer.empty()){
  const auto depths=raster_owner(r->z_buffer,lease,true,f);if(!depths)return false;
  const auto depth=depths->observe(r->z_buffer,f.raster);if(!depth)return fail(f,RetailRendererCameraError::raster_binding,f.raster.operation);
  const auto backing=depths->display_target(r->z_buffer,f.raster);
  if(!backing)return fail(f,RetailRendererCameraError::raster_binding,f.raster.operation);
  if(backing!=display||depth->description.width!=color->description.width||depth->description.height!=color->description.height)
   return fail(f,RetailRendererCameraError::raster_binding,"camera color/depth require the same native display and logical extent");
 }
 Busy guard(busy_);
 const bool begun=display->begin_render_target(r,r->target_policy->physical_viewport,!r->z_buffer.empty(),f.display);
 if(display->owns_render_target(*r)){r->active_display=display;r->updating=true;}
 if(!begun)return fail(f,RetailRendererCameraError::native_target,f.display.message);
 RetailGameSessionFailure sf;
 if(!r->alive||!active_.load(std::memory_order_acquire)||!lease->validate_current_thread(sf)){
  DisplayServiceFailure cleanup;
  if(display->end_render_target(*r,cleanup)){r->updating=false;r->active_display.reset();}
  return fail(f,RetailRendererCameraError::retired,"camera/Session revoked during native target begin");
 }
 if(!r->updating)return fail(f,RetailRendererCameraError::native_target,"native display did not retain the actual camera draw owner");
 return true;
}
bool NativeRetailRendererCameraModule::clear(const NativeRetailRendererCameraHandle& h,
 float red,float green,float blue,float alpha,std::uint32_t flags,RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(!r->updating||!r->active_display)return fail(f,RetailRendererCameraError::render_services_required,"native clear requires active owned target");
 if(flags&~3u)return fail(f,RetailRendererCameraError::render_services_required,"stencil/unknown native clear mode remains unbound");
 if(!raster_owner(r->frame_buffer,lease,false,f))return false;
 if(!r->z_buffer.empty()&&!raster_owner(r->z_buffer,lease,true,f))return false;
 if((flags&2u)&&(r->z_buffer.empty()||!r->target_policy||!r->target_policy->clear_depth))
  return fail(f,RetailRendererCameraError::native_target,"depth clear requires its raster and explicit native projection policy");
 const auto color=(flags&1u)?std::optional<std::array<float,4>>{{red,green,blue,alpha}}:std::nullopt;
 const auto depth=(flags&2u)?r->target_policy->clear_depth:std::nullopt;
 Busy guard(busy_);
 if(!r->active_display->clear_render_target(*r,color,depth,f.display))
  return fail(f,RetailRendererCameraError::native_target,f.display.message);
 return true;
}
bool NativeRetailRendererCameraModule::end_update(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(!r->updating||!r->active_display)return fail(f,RetailRendererCameraError::render_services_required,"native end requires active owned target");
 Busy guard(busy_);
 if(!r->active_display->end_render_target(*r,f.display))return fail(f,RetailRendererCameraError::native_target,f.display.message);
 r->updating=false;r->active_display.reset();return true;
}
bool NativeRetailRendererCameraModule::show_raster(const NativeRetailRendererCameraHandle& h,
 RetailRendererCameraFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->updating)return fail(f,RetailRendererCameraError::reentrant,"cannot show raster while camera update is active");
 if(r->frame_buffer.empty())return fail(f,RetailRendererCameraError::raster_binding,"show_raster requires live frame buffer raster");
 const auto colors=raster_owner(r->frame_buffer,lease,false,f);if(!colors)return false;
 auto display=colors->display_target(r->frame_buffer,f.raster);
 if(!display)return fail(f,RetailRendererCameraError::raster_binding,f.raster.operation);
 Busy guard(busy_);
 if(!display->swap_buffers(f.display))return fail(f,RetailRendererCameraError::native_target,f.display.message);
 return true;
}
void NativeRetailRendererCameraModule::retire()noexcept{
 active_.store(false,std::memory_order_release);
 for(const auto& r:records_){
  if(r->updating&&r->active_display){
   DisplayServiceFailure cleanup;
   if(r->active_display->end_render_target(*r,cleanup)){r->updating=false;r->active_display.reset();}
   // If the host context is unavailable, the display retains the snapshot;
   // after the camera expires its real host owner explicitly restores it.
  }
  if(auto frames=r->frame_link.owner_.lock())frames->release_object_link(r->frame_link);
  r->alive=false;
 }
 sync_callbacks_.clear();
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

void destroy_camera_producer_17ebb0(
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererRasters& rasters,
 RetailCameraProducer17EBB0Result& produced) noexcept {
 RetailRendererCameraFailure cf;
 RetailRendererFrameFailure ff;
 RetailRendererRasterFailure rf;
 if(!produced.camera.empty()){
  cameras.destroy_base_camera(produced.camera,cf);
  produced.camera={};
 }
 if(!produced.frame.empty()){
  frames.destroy_leaf(produced.frame,ff);
  produced.frame={};
 }
 if(!produced.color_raster.empty()){
  rasters.destroy_raster(produced.color_raster,rf);
  produced.color_raster={};
 }
 if(!produced.depth_raster.empty()){
  rasters.destroy_raster(produced.depth_raster,rf);
  produced.depth_raster={};
 }
 produced.world_link.reset();
}

std::optional<RetailCameraProducer17EBB0Result> create_camera_producer_17ebb0(
 const std::shared_ptr<NativeRetailGameSession>& session,
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererRasters& rasters,
 const RetailCameraProducer17EBB0Config& config,
 RetailCameraProducerFailure& failure) noexcept {
 failure={};
 RetailGameSessionFailure sf;
 if(!session||!session->validate_current_thread(sf)){
  failure.world={RetailCameraWorldError::session,"17EBB0 camera/world Session thread/lifetime"};
  failure.operation=failure.world.operation;
  return {};
 }
 if(config.width<=0||config.height<=0){
  failure.operation="17EBB0 positive camera extent required";
  failure.raster.kind=RetailRendererRasterError::extent;
  failure.raster.operation=failure.operation;
  return {};
 }
 RetailCameraProducer17EBB0Result out;
 auto rollback=[&]()noexcept{
  destroy_camera_producer_17ebb0(cameras,frames,rasters,out);
 };
 auto cam=cameras.create_base_camera(config.depth_inputs,failure.camera);
 if(!cam){failure.operation=failure.camera.operation;return {};}
 out.camera=*cam;

 auto frm=frames.create_base_frame(failure.frame);
 if(!frm){failure.operation=failure.frame.operation;rollback();return {};}
 out.frame=*frm;

 if(!cameras.set_frame(out.camera,out.frame,failure.camera)){
  failure.operation=failure.camera.operation;rollback();return {};
 }

 if(config.has_depth){
  auto zb=rasters.create_raster({config.width,config.height,0,1u,config.depth_flags|1u,0u},failure.raster);
  if(!zb){failure.operation=failure.raster.operation;rollback();return {};}
  out.depth_raster=*zb;
  if(config.display&&!rasters.bind_display_target(out.depth_raster,config.display,failure.raster)){
   failure.operation=failure.raster.operation;rollback();return {};
  }
  if(!cameras.set_z_buffer_raster(out.camera,out.depth_raster,failure.camera)){
   failure.operation=failure.camera.operation;rollback();return {};
  }
 }

 if(config.has_color){
  auto fb=rasters.create_raster({config.width,config.height,0,2u,config.color_flags|2u,0u},failure.raster);
  if(!fb){failure.operation=failure.raster.operation;rollback();return {};}
  out.color_raster=*fb;
  if(config.display&&!rasters.bind_display_target(out.color_raster,config.display,failure.raster)){
   failure.operation=failure.raster.operation;rollback();return {};
  }
  if(!cameras.set_frame_buffer_raster(out.camera,out.color_raster,failure.camera)){
   failure.operation=failure.camera.operation;rollback();return {};
  }
 }

 if(config.target_policy&&!cameras.set_render_target_policy(out.camera,*config.target_policy,failure.camera)){
  failure.operation=failure.camera.operation;rollback();return {};
 }

 out.world_link=NativeRetailCameraWorldLink::create_initial(session,failure.world);
 if(!out.world_link){failure.operation=failure.world.operation;rollback();return {};}
 return out;
}

std::optional<RetailCameraInit180ED0Result> initialize_cameras_180ed0(
 const std::shared_ptr<NativeRetailGameSession>& session,
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererRasters& rasters,
 NativeRetailRenderCameras& world_cameras,
 const RetailCameraInit180ED0Config& config,
 RetailCameraProducerFailure& failure) noexcept {
 failure={};
 if(config.width<=0||config.height<=0){
  failure.operation="180ED0 positive video mode extent required";
  failure.raster.kind=RetailRendererRasterError::extent;
  failure.raster.operation=failure.operation;
  return {};
 }
 const float aspect=static_cast<float>(config.width)/static_cast<float>(config.height);
 auto configure_camera=[&](const NativeRetailRendererCameraHandle& cam)noexcept->bool{
  if(!cameras.set_near_clip(cam,config.near_clip,failure.camera)){failure.operation=failure.camera.operation;return false;}
  if(!cameras.set_far_clip(cam,config.far_clip,failure.camera)){failure.operation=failure.camera.operation;return false;}
  if(!cameras.set_fov(cam,config.fov_degrees,aspect,failure.camera)){failure.operation=failure.camera.operation;return false;}
  if(!cameras.set_projection(cam,1u,failure.camera)){failure.operation=failure.camera.operation;return false;}
  return true;
 };

 RetailCameraProducer17EBB0Config primary_cfg{
  config.width,config.height,config.primary_has_color,config.primary_has_depth,
  0u,0u,config.depth_inputs,config.display,config.target_policy
 };
 auto primary=create_camera_producer_17ebb0(session,cameras,frames,rasters,primary_cfg,failure);
 if(!primary)return {};

 RetailCameraInit180ED0Result out{std::move(*primary),std::nullopt};
 auto rollback=[&]()noexcept{
  if(out.scene)destroy_camera_producer_17ebb0(cameras,frames,rasters,*out.scene);
  destroy_camera_producer_17ebb0(cameras,frames,rasters,out.primary);
 };

 if(!configure_camera(out.primary.camera)){rollback();return {};}
 if(!world_cameras.publish_camera_link_after_180ed0(0u,out.primary.world_link,failure.world)){
  failure.operation=failure.world.operation;rollback();return {};
 }

 if(config.create_scene_camera){
  RetailCameraProducer17EBB0Config scene_cfg{
   config.width,config.height,config.scene_has_color,config.scene_has_depth,
   0x20u,0u,config.depth_inputs,config.display,config.target_policy
  };
  auto scene=create_camera_producer_17ebb0(session,cameras,frames,rasters,scene_cfg,failure);
  if(!scene){rollback();return {};}
  out.scene=std::move(*scene);
  if(!configure_camera(out.scene->camera)){rollback();return {};}
  const auto obs=world_cameras.observe(failure.world);
  if(!obs){failure.operation=failure.world.operation;rollback();return {};}
  if(obs->count>1&&!world_cameras.publish_camera_link_after_180ed0(1u,out.scene->world_link,failure.world)){
   failure.operation=failure.world.operation;rollback();return {};
  }
 }
 return out;
}

bool sphere_in_frustum(
 const std::array<RetailRendererCameraPlane,6>& planes,
 const RetailRendererPoint3& center,
 float radius) noexcept {
 if(!std::isfinite(center.x)||!std::isfinite(center.y)||!std::isfinite(center.z)||!std::isfinite(radius)||radius<0.0f)
  return false;
 for(const auto& p:planes){
  const float dist=p.a*center.x+p.b*center.y+p.c*center.z+p.d;
  if(dist<-radius)return false;
 }
 return true;
}

bool attach_camera_to_world_1cf300(
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 const NativeRetailRendererCameraHandle& camera,
 const std::shared_ptr<NativeRetailCameraWorldLink>& world_link,
 std::shared_ptr<NativeRetailRenderWorld> world,
 RetailCameraProducerFailure& failure) noexcept {
 failure={};
 if(!world_link){
  failure.world={RetailCameraWorldError::missing_record,"1CF300 camera world-link required"};
  failure.operation=failure.world.operation;
  return false;
 }
 const auto cam_frame=cameras.frame(camera,failure.camera);
 if(!cam_frame){failure.operation=failure.camera.operation;return false;}
 if(!cam_frame->empty()){
  if(!frames.mark_dirty(*cam_frame,failure.frame)){
   failure.operation=failure.frame.operation;
   return false;
  }
 }
 if(!world_link->attach_after_frame_update_from_1cf300(std::move(world),failure.world)){
  failure.operation=failure.world.operation;
  return false;
 }
 return true;
}

bool detach_camera_from_world_1cf350(
 const std::shared_ptr<NativeRetailCameraWorldLink>& world_link,
 const std::shared_ptr<NativeRetailRenderWorld>& world,
 RetailCameraWorldFailure& failure) noexcept {
 failure={};
 if(!world_link){
  failure={RetailCameraWorldError::missing_record,"1CF350 camera world-link required"};
  return false;
 }
 return world_link->remove_from_1cf350(world,failure).has_value();
}

namespace {
struct RwStreamCursor {
 std::span<const std::byte> data;
 std::size_t pos{0};
 std::size_t remaining() const noexcept { return pos<=data.size()?data.size()-pos:0; }
 bool can_read(std::size_t n) const noexcept { return n<=remaining(); }
 bool read_bytes(void* dst,std::size_t n) noexcept {
  if(!can_read(n))return false;
  if(n>0)std::memcpy(dst,data.data()+pos,n);
  pos+=n;
  return true;
 }
 bool skip(std::size_t n) noexcept {
  if(!can_read(n))return false;
  pos+=n;
  return true;
 }
 bool read_u16(std::uint16_t& out) noexcept { return read_bytes(&out,sizeof(out)); }
 bool read_u32(std::uint32_t& out) noexcept { return read_bytes(&out,sizeof(out)); }
 bool read_i32(std::int32_t& out) noexcept { return read_bytes(&out,sizeof(out)); }
 bool read_f32(float& out) noexcept {
  if(!read_bytes(&out,sizeof(out)))return false;
  return std::isfinite(out);
 }
 bool read_chunk(std::uint32_t& type,std::uint32_t& length,std::uint32_t& lib_id) noexcept {
  if(!read_u32(type)||!read_u32(length)||!read_u32(lib_id))return false;
  return length<=remaining();
 }
};

bool parse_rw_material(
 RwStreamCursor cur,
 NativeRetailRendererMaterials& materials,
 NativeRetailRendererTextures& textures,
 NativeRetailMaterialHandle& out_mat) noexcept {
 std::uint32_t st_type=0,st_len=0,st_lib=0;
 if(!cur.read_chunk(st_type,st_len,st_lib)||st_type!=0x0001u||st_len<28)return false;
 RwStreamCursor st{cur.data.subspan(cur.pos,st_len),0};
 if(!cur.skip(st_len))return false;
 std::uint32_t flags=0,unused=0,textured=0;
 std::uint8_t rgba[4]{255,255,255,255};
 float ambient=1.0f,specular=1.0f,diffuse=1.0f;
 if(!st.read_u32(flags)||!st.read_bytes(rgba,4)||!st.read_u32(unused)||!st.read_u32(textured))return false;
 if(!st.read_f32(ambient)||!st.read_f32(specular)||!st.read_f32(diffuse))return false;

 NativeRetailTextureHandle tex_handle;
 if(textured!=0&&cur.remaining()>=12){
  std::uint32_t t_type=0,t_len=0,t_lib=0;
  if(!cur.read_chunk(t_type,t_len,t_lib))return false;
  if(t_type==0x0006u){
   RwStreamCursor tc{cur.data.subspan(cur.pos,t_len),0};
   if(!cur.skip(t_len))return false;
   std::uint32_t sub_type=0,sub_len=0,sub_lib=0;
   if(!tc.read_chunk(sub_type,sub_len,sub_lib)||sub_type!=0x0001u||!tc.skip(sub_len))return false;
   if(tc.read_chunk(sub_type,sub_len,sub_lib)&&sub_type==0x0002u&&sub_len>0&&sub_len<=256){
    char name_buf[257]{};
    if(!tc.read_bytes(name_buf,sub_len))return false;
    name_buf[sub_len]='\0';
    RetailRendererTextureFailure tf;
    auto found=textures.find_texture(name_buf,tf);
    if(found&&!found->empty())tex_handle=*found;
   }
  }
 }
 RetailRendererMaterialDescription desc;
 desc.color={rgba[0],rgba[1],rgba[2],rgba[3]};
 desc.surface={ambient,specular,diffuse};
 desc.texture=tex_handle;
 RetailRendererMaterialFailure mf;
 auto mat=materials.create_material(std::move(desc),mf);
 if(!mat)return false;
 out_mat=*mat;
 return true;
}

bool parse_rw_geometry(
 RwStreamCursor cur,
 std::uint64_t obj_id,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererMaterials& materials,
 NativeRetailRendererTextures& textures,
 NativeRetailWorld& world,
 const NativeRetailRendererFrameHandle& parent_frame,
 std::vector<NativeRetailMaterialHandle>& created_mats,
 std::vector<NativeRetailRendererFrameHandle>& created_frames,
 std::vector<std::uint64_t>& created_objs) noexcept {
 std::uint32_t st_type=0,st_len=0,st_lib=0;
 if(!cur.read_chunk(st_type,st_len,st_lib)||st_type!=0x0001u||st_len<16)return false;
 RwStreamCursor st{cur.data.subspan(cur.pos,st_len),0};
 if(!cur.skip(st_len))return false;

 std::uint32_t flags=0;
 std::int32_t num_tris=0,num_verts=0,num_morphs=0;
 if(!st.read_u32(flags)||!st.read_i32(num_tris)||!st.read_i32(num_verts)||!st.read_i32(num_morphs))return false;
 if(num_tris<=0||num_verts<=0||num_morphs<1||num_tris>65536||num_verts>65536)return false;

 std::vector<std::array<float,4>> vcols(static_cast<std::size_t>(num_verts),{1.0f,1.0f,1.0f,1.0f});
 if(flags&0x08u){
  for(std::int32_t i=0;i<num_verts;++i){
   std::uint8_t c[4]{};
   if(!st.read_bytes(c,4))return false;
   vcols[static_cast<std::size_t>(i)]={c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,c[3]/255.0f};
  }
 }
 std::vector<std::array<float,2>> uvs(static_cast<std::size_t>(num_verts),{0.0f,0.0f});
 if((flags&0x04u)||(flags&0x80u)){
  for(std::int32_t i=0;i<num_verts;++i){
   float u=0.0f,v=0.0f;
   if(!st.read_f32(u)||!st.read_f32(v))return false;
   uvs[static_cast<std::size_t>(i)]={u,v};
  }
 }

 struct RawTri { std::uint16_t v0,v1,mat,v2; };
 std::vector<RawTri> tris(static_cast<std::size_t>(num_tris));
 for(std::int32_t i=0;i<num_tris;++i){
  auto& t=tris[static_cast<std::size_t>(i)];
  if(!st.read_u16(t.v1)||!st.read_u16(t.v0)||!st.read_u16(t.mat)||!st.read_u16(t.v2))return false;
  if(t.v0>=num_verts||t.v1>=num_verts||t.v2>=num_verts)return false;
 }

 float cx=0.0f,cy=0.0f,cz=0.0f,radius=0.0f;
 std::uint32_t has_verts=0,has_norms=0;
 if(!st.read_f32(cx)||!st.read_f32(cy)||!st.read_f32(cz)||!st.read_f32(radius)||radius<0.0f)return false;
 if(!st.read_u32(has_verts)||!st.read_u32(has_norms)||has_verts==0)return false;
 std::vector<RetailRendererPoint3> positions(static_cast<std::size_t>(num_verts));
 for(std::int32_t i=0;i<num_verts;++i){
  auto& p=positions[static_cast<std::size_t>(i)];
  if(!st.read_f32(p.x)||!st.read_f32(p.y)||!st.read_f32(p.z))return false;
 }

 std::vector<NativeRetailMaterialHandle> mat_list;
 while(cur.remaining()>=12){
  std::uint32_t ctype=0,clen=0,clib=0;
  if(!cur.read_chunk(ctype,clen,clib))return false;
  RwStreamCursor sub{cur.data.subspan(cur.pos,clen),0};
  if(!cur.skip(clen))return false;
  if(ctype==0x0008u){
   std::uint32_t mt=0,ml=0,mlib=0;
   if(!sub.read_chunk(mt,ml,mlib)||mt!=0x0001u||ml<4)return false;
   RwStreamCursor mst{sub.data.subspan(sub.pos,ml),0};
   if(!sub.skip(ml))return false;
   std::int32_t mcount=0;
   if(!mst.read_i32(mcount)||mcount<0||mcount>256)return false;
   for(std::int32_t i=0;i<mcount;++i){
    std::int32_t idx=0;
    if(!mst.read_i32(idx))return false;
   }
   while(sub.remaining()>=12&&static_cast<std::int32_t>(mat_list.size())<mcount){
    std::uint32_t mct=0,mcl=0,mclib=0;
    if(!sub.read_chunk(mct,mcl,mclib))return false;
    RwStreamCursor mc{sub.data.subspan(sub.pos,mcl),0};
    if(!sub.skip(mcl))return false;
    if(mct==0x0007u){
     NativeRetailMaterialHandle mh;
     if(!parse_rw_material(mc,materials,textures,mh))return false;
     created_mats.push_back(mh);
     mat_list.push_back(mh);
    }
   }
  }
 }

 RetailRendererFrameFailure ff;
 auto frm=frames.create_base_frame(ff);
 if(!frm)return false;
 created_frames.push_back(*frm);
 if(!parent_frame.empty()&&!frames.add_child(parent_frame,*frm,ff))return false;

 RetailWorldSceneObject obj;
 obj.id=obj_id;
 obj.frame=*frm;
 obj.local_center={cx,cy,cz};
 obj.bounding_radius=radius;
 obj.triangles.reserve(static_cast<std::size_t>(num_tris)*3);
 for(const auto& t:tris){
  if(!mat_list.empty()){
   if(t.mat>=mat_list.size())return false;
   if(obj.material.empty())obj.material=mat_list[t.mat];
  }
  for(std::uint16_t vi:{t.v0,t.v1,t.v2}){
   const std::size_t idx=static_cast<std::size_t>(vi);
   obj.triangles.push_back({positions[idx],uvs[idx][0],uvs[idx][1],vcols[idx]});
  }
 }
 RetailRendererWorldFailure wf;
 if(!world.add_scene_object(std::move(obj),wf))return false;
 created_objs.push_back(obj_id);
 return true;
}
} // namespace

bool ingest_rw_geometry_stream(
 std::span<const std::byte> stream_bytes,
 std::uint64_t base_object_id,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererMaterials& materials,
 NativeRetailRendererTextures& textures,
 NativeRetailWorld& world,
 const NativeRetailRendererFrameHandle& parent_frame,
 RetailRendererWorldFailure& failure) noexcept {
 failure={};
 if(base_object_id==0||stream_bytes.size()<12){
  failure.kind=RetailRendererWorldError::invalid_handle;
  failure.operation="RW stream requires non-zero base_object_id and valid 12-byte chunk header";
  return false;
 }
 std::vector<NativeRetailMaterialHandle> created_mats;
 std::vector<NativeRetailRendererFrameHandle> created_frames;
 std::vector<std::uint64_t> created_objs;
 auto rollback=[&]()noexcept{
  RetailRendererWorldFailure wf;
  for(auto id:created_objs)(void)world.remove_scene_object(id,wf);
  RetailRendererFrameFailure ff;
  for(auto it=created_frames.rbegin();it!=created_frames.rend();++it){
   if(!parent_frame.empty())(void)frames.detach(*it,ff);
   (void)frames.destroy_leaf(*it,ff);
  }
  RetailRendererMaterialFailure mf;
  for(const auto& mh:created_mats)(void)materials.destroy_material(mh,mf);
 };

 RwStreamCursor cur{stream_bytes,0};
 std::uint32_t type=0,len=0,lib=0;
 if(!cur.read_chunk(type,len,lib)){
  failure.kind=RetailRendererWorldError::invalid_handle;
  failure.operation="RW stream truncated chunk header or length out of bounds";
  return false;
 }
 RwStreamCursor body{cur.data.subspan(cur.pos,len),0};
 std::uint64_t next_id=base_object_id;
 if(type==0x000Fu){
  if(!parse_rw_geometry(body,next_id++,frames,materials,textures,world,parent_frame,created_mats,created_frames,created_objs)){
   rollback();
   failure.kind=RetailRendererWorldError::invalid_handle;
   failure.operation="RW geometry chunk validation/ingestion failed";
   return false;
  }
  return true;
 }
 if(type==0x0010u){
  bool parsed_any=false;
  while(body.remaining()>=12){
   std::uint32_t ct=0,cl=0,clib=0;
   if(!body.read_chunk(ct,cl,clib)){
    rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW clump child chunk truncated";return false;
   }
   RwStreamCursor cbody{body.data.subspan(body.pos,cl),0};
   if(!body.skip(cl))return false;
   if(ct==0x001Au){
    std::uint32_t gt=0,gl=0,glib=0;
    if(!cbody.read_chunk(gt,gl,glib)||gt!=0x0001u||!cbody.skip(gl)){
     rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW geometry list header invalid";return false;
    }
    while(cbody.remaining()>=12){
     std::uint32_t gct=0,gcl=0,gclib=0;
     if(!cbody.read_chunk(gct,gcl,gclib)){
      rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW geometry list entry truncated";return false;
     }
     RwStreamCursor gbody{cbody.data.subspan(cbody.pos,gcl),0};
     if(!cbody.skip(gcl))return false;
     if(gct==0x000Fu){
      if(!parse_rw_geometry(gbody,next_id++,frames,materials,textures,world,parent_frame,created_mats,created_frames,created_objs)){
       rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW clump geometry ingestion failed";return false;
      }
      parsed_any=true;
     }
    }
   }
  }
  if(!parsed_any){
   rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW clump contained no geometry chunks";return false;
  }
  return true;
 }
 failure.kind=RetailRendererWorldError::invalid_handle;
 failure.operation="unsupported top-level RW chunk type";
 return false;
}

std::optional<RetailWorldRenderStats> render_world_scene_frame(
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 const NativeRetailRendererCameraHandle& camera,
 const std::shared_ptr<NativeRetailCameraWorldLink>& world_link,
 const std::shared_ptr<NativeRetailWorld>& world,
 const std::array<float,4>& clear_color,
 std::uint32_t clear_flags,
 bool call_show_raster,
 RetailCameraProducerFailure& failure,
 NativeRetailRendererMaterials* materials,
 NativeRetailRendererTextures* textures,
 NativeRetailRendererRasters* rasters) noexcept {
 failure={};
 if(!world_link||!world){
  failure.world={RetailCameraWorldError::missing_record,"world render requires live camera world-link and world"};
  failure.operation=failure.world.operation;
  return {};
 }
 const auto bound_world=world_link->world_from_1cf380(failure.world);
 if(!bound_world){failure.operation=failure.world.operation;return {};}
 if(bound_world->get()!=world.get()){
  failure.world={RetailCameraWorldError::missing_record,"camera is not attached to target NativeRetailWorld"};
  failure.operation=failure.world.operation;
  return {};
 }

 if(!cameras.begin_update(camera,failure.camera)){
  failure.operation=failure.camera.operation;
  return {};
 }
 auto cleanup_end=[&]()noexcept{
  RetailRendererCameraFailure end_f;
  (void)cameras.end_update(camera,end_f);
 };

 if(!cameras.clear(camera,clear_color[0],clear_color[1],clear_color[2],clear_color[3],clear_flags,failure.camera)){
  failure.operation=failure.camera.operation;
  cleanup_end();
  return {};
 }
 if(!cameras.synchronize(camera,frames,failure.camera)){
  failure.operation=failure.camera.operation;
  cleanup_end();
  return {};
 }
 const auto cam_obs=cameras.observe(camera,failure.camera);
 if(!cam_obs||!cam_obs->synchronized_view_available){
  failure.camera.kind=RetailRendererCameraError::render_services_required;
  failure.operation="camera view/frustum synchronization unavailable";
  cleanup_end();
  return {};
 }

 RetailWorldRenderStats stats;
#ifdef _WIN32
 const GLboolean prior_depth=glIsEnabled(GL_DEPTH_TEST);
 const GLboolean prior_tex2d=glIsEnabled(GL_TEXTURE_2D);
 GLint prior_tex_binding=0,prior_tex_env=GL_MODULATE;
 glGetIntegerv(GL_TEXTURE_BINDING_2D,&prior_tex_binding);
 glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&prior_tex_env);
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
 GLint prior_depth_func=GL_LESS;
 glGetIntegerv(GL_DEPTH_FUNC,&prior_depth_func);
 GLint prior_matrix_mode=GL_MODELVIEW;
 glGetIntegerv(GL_MATRIX_MODE,&prior_matrix_mode);
 glEnable(GL_DEPTH_TEST);
 glDepthFunc(GL_LESS);
 glMatrixMode(GL_PROJECTION);
 glPushMatrix();
 glLoadMatrixf(cam_obs->projection_matrix.elements.data());
 glMatrixMode(GL_MODELVIEW);
 glPushMatrix();
#endif

 for(const auto& obj:world->scene_objects()){
  if(!obj.visible)continue;
  ++stats.total_objects;
  const auto f_obs=frames.observe(obj.frame,failure.frame);
  if(!f_obs){
   failure.operation=failure.frame.operation;
#ifdef _WIN32
   glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(prior_tex_binding));
   if(!prior_tex2d)glDisable(GL_TEXTURE_2D);else glEnable(GL_TEXTURE_2D);
   glMatrixMode(GL_MODELVIEW);glPopMatrix();
   glMatrixMode(GL_PROJECTION);glPopMatrix();
   glMatrixMode(static_cast<GLenum>(prior_matrix_mode));
   glDepthFunc(static_cast<GLenum>(prior_depth_func));
   if(!prior_depth)glDisable(GL_DEPTH_TEST);
#endif
   cleanup_end();
   return {};
  }
  const auto& m=f_obs->world.affine;
  const auto transform_pt=[&m](const RetailRendererPoint3& p)noexcept->RetailRendererPoint3{
   return {
    p.x*m.right.x+p.y*m.up.x+p.z*m.at.x+m.translation.x,
    p.x*m.right.y+p.y*m.up.y+p.z*m.at.y+m.translation.y,
    p.x*m.right.z+p.y*m.up.z+p.z*m.at.z+m.translation.z
   };
  };
  const auto world_center=transform_pt(obj.local_center);
  const auto len3=[](const RetailRendererPoint3& v)noexcept{
   return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
  };
  const float max_scale=std::max({len3(m.right),len3(m.up),len3(m.at)});
  const float world_radius=obj.bounding_radius*max_scale;
  if(!sphere_in_frustum(cam_obs->frustum_planes,world_center,world_radius)){
   ++stats.culled_objects;
   continue;
  }
  ++stats.visible_objects;

  std::array<float,4> mat_color{1.0f,1.0f,1.0f,1.0f};
  GLuint gl_tex=0;
  if(!obj.material.empty()&&materials!=nullptr){
   RetailRendererMaterialFailure mf;
   if(const auto mo=materials->observe(obj.material,mf)){
    mat_color={mo->color.r/255.0f,mo->color.g/255.0f,mo->color.b/255.0f,mo->color.a/255.0f};
   }
   if(textures!=nullptr&&rasters!=nullptr){
    if(const auto th=materials->get_texture(obj.material,mf);th&&!th->empty()){
     for(const auto& entry:world->gpu_texture_cache()){
      if(entry.texture.same_identity(*th)){gl_tex=static_cast<GLuint>(entry.gl_texture_id);break;}
     }
#ifdef _WIN32
     if(gl_tex==0){
      RetailRendererTextureFailure tf;
      RetailRendererRasterFailure rf;
      const auto rh=textures->raster(*th,tf);
      const auto tobs=textures->observe(*th,tf);
      if(rh&&!rh->empty()){
       const auto robs=rasters->observe(*rh,rf);
       auto pixels=rasters->lock_pixels(*rh,rf);
       if(robs&&pixels&&robs->description.width>0&&robs->description.height>0&&
         pixels->size()>=static_cast<std::size_t>(robs->description.width)*static_cast<std::size_t>(robs->description.height)*4u){
        glGenTextures(1,&gl_tex);
        if(gl_tex!=0){
         glBindTexture(GL_TEXTURE_2D,gl_tex);
         const GLint filter_gl=(tobs&&tobs->filter==RetailRendererTextureFilter::nearest)?GL_NEAREST:GL_LINEAR;
         const GLint wrap_u=(tobs&&tobs->address_u==RetailRendererTextureAddress::clamp)?0x812F:GL_REPEAT;
         const GLint wrap_v=(tobs&&tobs->address_v==RetailRendererTextureAddress::clamp)?0x812F:GL_REPEAT;
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter_gl);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter_gl);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap_u);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap_v);
         glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,robs->description.width,robs->description.height,0,
          GL_RGBA,GL_UNSIGNED_BYTE,pixels->data());
         try{world->gpu_texture_cache().push_back({*th,static_cast<std::uint32_t>(gl_tex)});}catch(...){}
        }
       }
       (void)rasters->unlock_pixels(*rh,rf);
      }
     }
#endif
    }
   }
  }

#ifdef _WIN32
  if(gl_tex!=0){
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D,gl_tex);
   ++stats.bound_textures;
  }else{
   glDisable(GL_TEXTURE_2D);
  }
  const std::array<float,16> model_col_major={
   m.right.x,m.right.y,m.right.z,0.0f,
   m.up.x,m.up.y,m.up.z,0.0f,
   m.at.x,m.at.y,m.at.z,0.0f,
   m.translation.x,m.translation.y,m.translation.z,1.0f
  };
  glLoadMatrixf(cam_obs->view_matrix.elements.data());
  glMultMatrixf(model_col_major.data());
  glBegin(GL_TRIANGLES);
  for(const auto& v:obj.triangles){
   glTexCoord2f(v.u,v.v);
   glColor4f(v.color[0]*mat_color[0],v.color[1]*mat_color[1],v.color[2]*mat_color[2],v.color[3]*mat_color[3]);
   glVertex3f(v.position.x,v.position.y,v.position.z);
  }
  glEnd();
#endif
  stats.submitted_triangles+=obj.triangles.size()/3;
 }

#ifdef _WIN32
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,prior_tex_env);
 glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(prior_tex_binding));
 if(!prior_tex2d)glDisable(GL_TEXTURE_2D);else glEnable(GL_TEXTURE_2D);
 glMatrixMode(GL_MODELVIEW);
 glPopMatrix();
 glMatrixMode(GL_PROJECTION);
 glPopMatrix();
 glMatrixMode(static_cast<GLenum>(prior_matrix_mode));
 glDepthFunc(static_cast<GLenum>(prior_depth_func));
 if(!prior_depth)glDisable(GL_DEPTH_TEST);
#endif

 if(!cameras.end_update(camera,failure.camera)){
  failure.operation=failure.camera.operation;
  return {};
 }
 if(call_show_raster&&!cameras.show_raster(camera,failure.camera)){
  failure.operation=failure.camera.operation;
  return {};
 }
 return stats;
}
}
