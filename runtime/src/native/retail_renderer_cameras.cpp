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

bool parse_rw_matlist(
 RwStreamCursor sub,
 NativeRetailRendererMaterials& materials,
 NativeRetailRendererTextures& textures,
 std::vector<NativeRetailMaterialHandle>& created_mats,
 std::vector<NativeRetailMaterialHandle>& mat_list) noexcept {
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
 return static_cast<std::int32_t>(mat_list.size())==mcount;
}

bool parse_rw_bsp_node(
 RwStreamCursor cur,
 std::uint32_t node_type,
 std::uint64_t& next_sector_id,
 const std::vector<NativeRetailMaterialHandle>& mat_list,
 std::uint32_t world_flags,
 NativeRetailWorld& world,
 std::int32_t& out_index) noexcept {
 if(node_type==0x0009u){
  std::uint32_t st=0,sl=0,slib=0;
  if(!cur.read_chunk(st,sl,slib)||st!=0x0001u||sl<44)return false;
  RwStreamCursor s{cur.data.subspan(cur.pos,sl),0};
  if(!cur.skip(sl))return false;
  std::int32_t mat_base=0,num_tris=0,num_verts=0;
  float bmin[3]{},bmax[3]{};
  std::uint32_t coll_present=0,unused=0;
  if(!s.read_i32(mat_base)||!s.read_i32(num_tris)||!s.read_i32(num_verts))return false;
  if(num_tris<0||num_verts<0||num_tris>65536||num_verts>65536||mat_base<0)return false;
  for(int i=0;i<3;++i)if(!s.read_f32(bmin[i]))return false;
  for(int i=0;i<3;++i)if(!s.read_f32(bmax[i]))return false;
  if(bmin[0]>bmax[0]||bmin[1]>bmax[1]||bmin[2]>bmax[2])return false;
  if(!s.read_u32(coll_present)||!s.read_u32(unused))return false;

  std::vector<RetailRendererPoint3> positions(static_cast<std::size_t>(num_verts));
  for(std::int32_t i=0;i<num_verts;++i){
   auto& p=positions[static_cast<std::size_t>(i)];
   if(!s.read_f32(p.x)||!s.read_f32(p.y)||!s.read_f32(p.z))return false;
  }
  if(world_flags&0x20u){
   if(!s.skip(static_cast<std::size_t>(num_verts)*3u))return false;
  }
  std::vector<std::array<float,4>> vcols(static_cast<std::size_t>(num_verts),{1.0f,1.0f,1.0f,1.0f});
  if(world_flags&0x08u){
   for(std::int32_t i=0;i<num_verts;++i){
    std::uint8_t c[4]{};
    if(!s.read_bytes(c,4))return false;
    vcols[static_cast<std::size_t>(i)]={c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,c[3]/255.0f};
   }
  }
  std::vector<std::array<float,2>> uvs(static_cast<std::size_t>(num_verts),{0.0f,0.0f});
  if((world_flags&0x04u)||(world_flags&0x80u)){
   for(std::int32_t i=0;i<num_verts;++i){
    float u=0.0f,v=0.0f;
    if(!s.read_f32(u)||!s.read_f32(v))return false;
    uvs[static_cast<std::size_t>(i)]={u,v};
   }
  }
  RetailWorldBspSector leaf;
  leaf.id=next_sector_id++;
  leaf.is_leaf=true;
  leaf.bbox_min={bmin[0],bmin[1],bmin[2]};
  leaf.bbox_max={bmax[0],bmax[1],bmax[2]};
  leaf.bsphere_center={(bmin[0]+bmax[0])*0.5f,(bmin[1]+bmax[1])*0.5f,(bmin[2]+bmax[2])*0.5f};
  const float dx=bmax[0]-bmin[0],dy=bmax[1]-bmin[1],dz=bmax[2]-bmin[2];
  leaf.bsphere_radius=0.5f*std::sqrt(dx*dx+dy*dy+dz*dz);
  leaf.triangles.reserve(static_cast<std::size_t>(num_tris)*3u);
  for(std::int32_t i=0;i<num_tris;++i){
   std::uint16_t mat_id=0,v0=0,v1=0,v2=0;
   if(!s.read_u16(mat_id)||!s.read_u16(v0)||!s.read_u16(v1)||!s.read_u16(v2))return false;
   if(v0>=num_verts||v1>=num_verts||v2>=num_verts)return false;
   if(!mat_list.empty()){
    const std::size_t abs_mat=static_cast<std::size_t>(mat_base)+static_cast<std::size_t>(mat_id);
    if(abs_mat>=mat_list.size())return false;
    if(leaf.material.empty())leaf.material=mat_list[abs_mat];
   }
   for(std::uint16_t vi:{v0,v1,v2}){
    const std::size_t idx=static_cast<std::size_t>(vi);
    leaf.triangles.push_back({positions[idx],uvs[idx][0],uvs[idx][1],vcols[idx]});
   }
  }
  RetailRendererWorldFailure wf;
  auto idx_opt=world.add_bsp_sector(std::move(leaf),wf);
  if(!idx_opt)return false;
  out_index=*idx_opt;
  return true;
 }
 if(node_type==0x000Au){
  std::uint32_t st=0,sl=0,slib=0;
  if(!cur.read_chunk(st,sl,slib)||st!=0x0001u||sl<24)return false;
  RwStreamCursor s{cur.data.subspan(cur.pos,sl),0};
  if(!cur.skip(sl))return false;
  std::uint32_t split_type=0,left_is_atomic=0,right_is_atomic=0;
  float split_val=0.0f,left_val=0.0f,right_val=0.0f;
  if(!s.read_u32(split_type)||!s.read_f32(split_val)||!s.read_u32(left_is_atomic)||
    !s.read_u32(right_is_atomic)||!s.read_f32(left_val)||!s.read_f32(right_val))return false;

  std::uint32_t lt=0,ll=0,llib=0;
  if(!cur.read_chunk(lt,ll,llib)||(lt!=0x0009u&&lt!=0x000Au))return false;
  RwStreamCursor lcur{cur.data.subspan(cur.pos,ll),0};
  if(!cur.skip(ll))return false;
  std::int32_t left_idx=-1;
  if(!parse_rw_bsp_node(lcur,lt,next_sector_id,mat_list,world_flags,world,left_idx))return false;

  std::uint32_t rt=0,rl=0,rlib=0;
  if(!cur.read_chunk(rt,rl,rlib)||(rt!=0x0009u&&rt!=0x000Au))return false;
  RwStreamCursor rcur{cur.data.subspan(cur.pos,rl),0};
  if(!cur.skip(rl))return false;
  std::int32_t right_idx=-1;
  if(!parse_rw_bsp_node(rcur,rt,next_sector_id,mat_list,world_flags,world,right_idx))return false;

  const auto& secs=world.bsp_sectors();
  const auto& lc=secs[static_cast<std::size_t>(left_idx)];
  const auto& rc=secs[static_cast<std::size_t>(right_idx)];
  RetailWorldBspSector plane;
  plane.id=next_sector_id++;
  plane.is_leaf=false;
  plane.split_type=split_type;
  plane.split_value=split_val;
  plane.left_child=left_idx;
  plane.right_child=right_idx;
  plane.bbox_min={
   std::min(lc.bbox_min.x,rc.bbox_min.x),
   std::min(lc.bbox_min.y,rc.bbox_min.y),
   std::min(lc.bbox_min.z,rc.bbox_min.z)
  };
  plane.bbox_max={
   std::max(lc.bbox_max.x,rc.bbox_max.x),
   std::max(lc.bbox_max.y,rc.bbox_max.y),
   std::max(lc.bbox_max.z,rc.bbox_max.z)
  };
  plane.bsphere_center={
   (plane.bbox_min.x+plane.bbox_max.x)*0.5f,
   (plane.bbox_min.y+plane.bbox_max.y)*0.5f,
   (plane.bbox_min.z+plane.bbox_max.z)*0.5f
  };
  const float dx=plane.bbox_max.x-plane.bbox_min.x;
  const float dy=plane.bbox_max.y-plane.bbox_min.y;
  const float dz=plane.bbox_max.z-plane.bbox_min.z;
  plane.bsphere_radius=0.5f*std::sqrt(dx*dx+dy*dy+dz*dz);
  RetailRendererWorldFailure wf;
  auto idx_opt=world.add_bsp_sector(std::move(plane),wf);
  if(!idx_opt)return false;
  out_index=*idx_opt;
  return true;
 }
 return false;
}

void resolve_material_and_bind_texture(
 const NativeRetailMaterialHandle& material,
 NativeRetailWorld& world,
 NativeRetailRendererMaterials* materials,
 NativeRetailRendererTextures* textures,
 NativeRetailRendererRasters* rasters,
 std::array<float,4>& out_mat_color,
 std::size_t& bound_textures) noexcept {
 out_mat_color={1.0f,1.0f,1.0f,1.0f};
#ifdef _WIN32
 GLuint gl_tex=0;
 if(!material.empty()&&materials!=nullptr){
  RetailRendererMaterialFailure mf;
  if(const auto mo=materials->observe(material,mf)){
   out_mat_color={mo->color.r/255.0f,mo->color.g/255.0f,mo->color.b/255.0f,mo->color.a/255.0f};
  }
  if(textures!=nullptr&&rasters!=nullptr){
   if(const auto th=materials->get_texture(material,mf);th&&!th->empty()){
    for(const auto& entry:world.gpu_texture_cache()){
     if(entry.texture.same_identity(*th)){gl_tex=static_cast<GLuint>(entry.gl_texture_id);break;}
    }
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
        try{world.gpu_texture_cache().push_back({*th,static_cast<std::uint32_t>(gl_tex)});}catch(...){}
       }
      }
      (void)rasters->unlock_pixels(*rh,rf);
     }
    }
   }
  }
 }
 if(gl_tex!=0){
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,gl_tex);
  ++bound_textures;
 }else{
  glDisable(GL_TEXTURE_2D);
 }
#else
 (void)material;(void)world;(void)materials;(void)textures;(void)rasters;(void)bound_textures;
#endif
}
static std::size_t ps2_swizzle_offset_ct32(std::uint32_t x,std::uint32_t y,std::uint32_t width)noexcept{
 static constexpr int kBlock32[4][8]={
  {0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},
  {8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}
 };
 static constexpr int kColumn32[2][8]={
  {0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}
 };
 const std::size_t pages_x=(static_cast<std::size_t>(width)+63u)/64u;
 const std::size_t page=(static_cast<std::size_t>(y/32u)*pages_x)+static_cast<std::size_t>(x/64u);
 const std::uint32_t py=y&31u,px=x&63u;
 const std::size_t col=(py&7u)/2u;
 const std::size_t inner=static_cast<std::size_t>(kBlock32[py/8u][px/8u])*256u+col*64u+static_cast<std::size_t>(kColumn32[py&1u][px&7u])*4u;
 return page*8192u+inner;
}

static std::size_t ps2_swizzle_offset_t8(std::uint32_t x,std::uint32_t y,std::uint32_t width)noexcept{
 static constexpr int kBlock32[4][8]={
  {0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},
  {8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}
 };
 static constexpr int kColumnWord[2][2][8]={
  {{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}},
  {{8,9,12,13,0,1,4,5},{10,11,14,15,2,3,6,7}}
 };
 const std::size_t pages_x=(static_cast<std::size_t>(width)+127u)/128u;
 const std::size_t page=(static_cast<std::size_t>(y/64u)*pages_x)+static_cast<std::size_t>(x/128u);
 const std::uint32_t py=y&63u,px=x&127u;
 const int block=kBlock32[(py/16u)&3u][(px/16u)&7u];
 const std::uint32_t wx=px%16u,wy=py%16u;
 const std::uint32_t col=wy/4u,wy4=wy%4u;
 const std::uint32_t table=(((wy4&2u)>>1u)^((py/4u)&1u));
 const std::uint32_t byte_in_word=((wx&8u)>>2u)+((wy4&2u)>>1u);
 const std::size_t inner=static_cast<std::size_t>(block)*256u+static_cast<std::size_t>(col)*64u+
  static_cast<std::size_t>(kColumnWord[table][wy4&1u][wx&7u])*4u+static_cast<std::size_t>(byte_in_word);
 return page*8192u+inner;
}

static std::size_t ps2_swizzle_nibble_t4(std::uint32_t x,std::uint32_t y,std::uint32_t width)noexcept{
 static constexpr int kBlock16[8][4]={
  {0,2,8,10},{1,3,9,11},{4,6,12,14},{5,7,13,15},
  {16,18,24,26},{17,19,25,27},{20,22,28,30},{21,23,29,31}
 };
 static constexpr int kColumnWord[2][2][8]={
  {{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}},
  {{8,9,12,13,0,1,4,5},{10,11,14,15,2,3,6,7}}
 };
 const std::size_t pages_x=(static_cast<std::size_t>(width)+127u)/128u;
 const std::size_t page=(static_cast<std::size_t>(y/128u)*pages_x)+static_cast<std::size_t>(x/128u);
 const std::uint32_t py=y&127u,px=x&127u;
 const int block=kBlock16[(py/16u)&7u][(px/32u)&3u];
 const std::uint32_t wx=px%32u,wy=py%16u;
 const std::uint32_t col=wy/4u,wy4=wy%4u;
 const std::uint32_t shift=(wx&0x18u)+((wy4&2u)<<1u);
 const std::uint32_t nibble=shift/4u;
 const std::uint32_t table=(((wy4&2u)>>1u)^(col&1u));
 const std::size_t inner=(static_cast<std::size_t>(col)*64u+static_cast<std::size_t>(block)*256u+
  static_cast<std::size_t>(kColumnWord[table][wy4&1u][wx&7u])*4u)*2u+static_cast<std::size_t>(nibble);
 return page*16384u+inner;
}

} // namespace

bool ingest_rw_tex_dictionary_stream(
 std::span<const std::byte> stream_bytes,
 NativeRetailRendererRasters& rasters,
 NativeRetailRendererTextures& textures,
 RetailRendererTextureFailure& failure) noexcept {
 failure={};
 if(stream_bytes.size()<12){
  failure.kind=RetailRendererTextureError::invalid_handle;
  failure.operation="TXD stream requires 12-byte chunk header";
  return false;
 }
 RwStreamCursor cur{stream_bytes,0};
 std::uint32_t type=0,len=0,lib=0;
 if(!cur.read_chunk(type,len,lib)||type!=0x0016u){
  failure.kind=RetailRendererTextureError::invalid_handle;
  failure.operation="TXD stream requires rwID_TEXDICTIONARY (0x0016) root chunk";
  return false;
 }
 RwStreamCursor body{cur.data.subspan(cur.pos,len),0};
 std::uint32_t st=0,sl=0,slib=0;
 if(!body.read_chunk(st,sl,slib)||st!=0x0001u||sl<4){
  failure.kind=RetailRendererTextureError::invalid_handle;
  failure.operation="TXD stream header struct missing or truncated";
  return false;
 }
 RwStreamCursor s{body.data.subspan(body.pos,sl),0};
 if(!body.skip(sl))return false;
 std::uint16_t count=0,device_id=0;
 if(!s.read_u16(count)||!s.read_u16(device_id)||count>1024){
  failure.kind=RetailRendererTextureError::invalid_handle;
  failure.operation="TXD texture count invalid";
  return false;
 }

 std::vector<NativeRetailTextureHandle> created_textures;
 std::vector<NativeRetailRasterHandle> created_rasters;
 auto rollback=[&]()noexcept{
  RetailRendererTextureFailure tf;
  for(const auto& th:created_textures)(void)textures.destroy_texture(th,tf);
  RetailRendererRasterFailure rf;
  for(const auto& rh:created_rasters)(void)rasters.destroy_raster(rh,rf);
 };

 std::uint16_t parsed=0;
 while(body.remaining()>=12&&parsed<count){
  std::uint32_t ct=0,cl=0,clib=0;
  if(!body.read_chunk(ct,cl,clib)){
   rollback();failure.kind=RetailRendererTextureError::invalid_handle;failure.operation="TXD child chunk truncated";return false;
  }
  RwStreamCursor cbody{body.data.subspan(body.pos,cl),0};
  if(!body.skip(cl))return false;
  if(ct!=0x0015u)continue;

  std::uint32_t nt=0,nl=0,nlib=0;
  if(!cbody.read_chunk(nt,nl,nlib)||nt!=0x0001u||nl<88){
   rollback();failure.kind=RetailRendererTextureError::invalid_handle;failure.operation="TXD rwID_TEXTURENATIVE struct truncated";return false;
  }
  RwStreamCursor nb{cbody.data.subspan(cbody.pos,nl),0};
  if(!cbody.skip(nl))return false;

  std::uint32_t platform=0,filter_addr=0,fmt=0,has_alpha=0;
  char name_buf[33]{},mask_buf[33]{};
  std::uint16_t width=0,height=0;
  std::uint8_t depth=0,mips=0,rtype=0,comp=0;
  if(!nb.read_u32(platform)||!nb.read_u32(filter_addr)||
    !nb.read_bytes(name_buf,32)||!nb.read_bytes(mask_buf,32)||
    !nb.read_u32(fmt)||!nb.read_u32(has_alpha)||
    !nb.read_u16(width)||!nb.read_u16(height)||
    !nb.read_bytes(&depth,1)||!nb.read_bytes(&mips,1)||!nb.read_bytes(&rtype,1)||!nb.read_bytes(&comp,1)){
   rollback();failure.kind=RetailRendererTextureError::invalid_handle;failure.operation="TXD texture header fields truncated";return false;
  }
  if(width==0||height==0||width>4096||height>4096){
   rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD texture dimensions invalid";return false;
  }
  const bool is_pal4=(depth==4)||((fmt&0x4000u)!=0&&depth!=8&&depth!=32);
  const bool is_pal8=(depth==8)||((fmt&0x2000u)!=0&&depth!=4&&depth!=32);
  const bool is_rgba32=(depth==32)&&!is_pal4&&!is_pal8;
  if(!is_pal4&&!is_pal8&&!is_rgba32){
   rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD texture depth/format unsupported";return false;
  }

  const std::size_t total_pixels=static_cast<std::size_t>(width)*static_cast<std::size_t>(height);
  const std::size_t rgba_bytes=total_pixels*4u;
  const bool is_swizzled=((fmt&0x00020000u)!=0u)||((comp&0x01u)!=0u);
  std::vector<std::byte> decoded_rgba;
  if(is_rgba32){
   if(is_swizzled){
    const std::size_t pages_x=(static_cast<std::size_t>(width)+63u)/64u;
    const std::size_t pages_y=(static_cast<std::size_t>(height)+31u)/32u;
    const std::size_t swizzled_bytes=pages_x*pages_y*8192u;
    if(nb.remaining()<swizzled_bytes){
     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PSMCT32 payload truncated";return false;
    }
    std::vector<std::uint8_t> raw_swizzled(swizzled_bytes);
    if(!nb.read_bytes(raw_swizzled.data(),swizzled_bytes)){
     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PSMCT32 read failed";return false;
    }
    try{decoded_rgba.resize(rgba_bytes);}catch(...){
     rollback();failure.kind=RetailRendererTextureError::allocation;failure.operation="TXD swizzled PSMCT32 decode allocation";return false;
    }
    for(std::uint32_t y=0;y<height;++y){
     for(std::uint32_t x=0;x<width;++x){
      const std::size_t src_off=ps2_swizzle_offset_ct32(x,y,width);
      if(src_off+3u>=raw_swizzled.size()){
       rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PSMCT32 offset out of bounds";return false;
      }
      const std::size_t out_off=(static_cast<std::size_t>(y)*width+x)*4u;
      decoded_rgba[out_off+0]=static_cast<std::byte>(raw_swizzled[src_off+0]);
      decoded_rgba[out_off+1]=static_cast<std::byte>(raw_swizzled[src_off+1]);
      decoded_rgba[out_off+2]=static_cast<std::byte>(raw_swizzled[src_off+2]);
      decoded_rgba[out_off+3]=static_cast<std::byte>(raw_swizzled[src_off+3]);
     }
    }
   }else if(nb.remaining()<rgba_bytes){
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD texture pixel payload truncated";return false;
   }
  }else if(is_pal4){
   if((total_pixels%2u)!=0){
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL4 pixel count must be even";return false;
   }
   const std::size_t pal_bytes=16u*4u;
   const std::size_t idx_bytes=total_pixels/2u;
   const std::size_t pages_x=(static_cast<std::size_t>(width)+127u)/128u;
   const std::size_t pages_y=(static_cast<std::size_t>(height)+127u)/128u;
   const std::size_t payload_bytes=is_swizzled?(pages_x*pages_y*8192u):idx_bytes;
   if(nb.remaining()<pal_bytes+payload_bytes){
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL4 palette/pixel payload truncated";return false;
   }
   std::array<std::uint8_t,64> pal{};
   if(!nb.read_bytes(pal.data(),pal_bytes)){
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL4 palette read failed";return false;
   }
   std::vector<std::uint8_t> indices(idx_bytes);
   if(is_swizzled){
    std::vector<std::uint8_t> raw_swizzled(payload_bytes);
    if(!nb.read_bytes(raw_swizzled.data(),payload_bytes)){
     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PAL4 index read failed";return false;
    }
    for(std::uint32_t y=0;y<height;++y){
     for(std::uint32_t x=0;x<width;++x){
      const std::size_t nib=ps2_swizzle_nibble_t4(x,y,width);
      const std::size_t src_byte=nib/2u;
      if(src_byte>=raw_swizzled.size()){
       rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PAL4 offset out of bounds";return false;
      }
      const std::uint8_t b=raw_swizzled[src_byte];
      const std::uint8_t val=(nib&1u)?static_cast<std::uint8_t>(b>>4u):static_cast<std::uint8_t>(b&0x0Fu);
      const std::size_t lin=static_cast<std::size_t>(y)*width+x;
      if((lin&1u)==0u)indices[lin/2u]=val;
      else indices[lin/2u]|=static_cast<std::uint8_t>(val<<4u);
     }
    }
   }else{
    if(!nb.read_bytes(indices.data(),idx_bytes)){
     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL4 index read failed";return false;
    }
   }
   auto expand_gs_alpha=[](std::uint8_t a)noexcept->std::uint8_t{
    return (a>=0x80u)?0xFFu:static_cast<std::uint8_t>((static_cast<std::uint32_t>(a)*255u+64u)/128u);
   };
   try{decoded_rgba.resize(rgba_bytes);}catch(...){
    rollback();failure.kind=RetailRendererTextureError::allocation;failure.operation="TXD PAL4 decode allocation";return false;
   }
   for(std::size_t i=0;i<idx_bytes;++i){
    const std::uint8_t packed=indices[i];
    const std::size_t p0=static_cast<std::size_t>(packed&0x0Fu)*4u;
    const std::size_t p1=static_cast<std::size_t>((packed>>4u)&0x0Fu)*4u;
    const std::size_t out0=(i*2u)*4u;
    const std::size_t out1=(i*2u+1u)*4u;
    decoded_rgba[out0+0]=static_cast<std::byte>(pal[p0+0]);
    decoded_rgba[out0+1]=static_cast<std::byte>(pal[p0+1]);
    decoded_rgba[out0+2]=static_cast<std::byte>(pal[p0+2]);
    decoded_rgba[out0+3]=static_cast<std::byte>(expand_gs_alpha(pal[p0+3]));
    decoded_rgba[out1+0]=static_cast<std::byte>(pal[p1+0]);
    decoded_rgba[out1+1]=static_cast<std::byte>(pal[p1+1]);
    decoded_rgba[out1+2]=static_cast<std::byte>(pal[p1+2]);
    decoded_rgba[out1+3]=static_cast<std::byte>(expand_gs_alpha(pal[p1+3]));
   }
  }else if(is_pal8){
   const std::size_t pal_bytes=256u*4u;
   const std::size_t idx_bytes=total_pixels;
   const std::size_t pages_x=(static_cast<std::size_t>(width)+127u)/128u;
   const std::size_t pages_y=(static_cast<std::size_t>(height)+63u)/64u;
   const std::size_t payload_bytes=is_swizzled?(pages_x*pages_y*8192u):idx_bytes;
   if(nb.remaining()<pal_bytes+payload_bytes){
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL8 palette/pixel payload truncated";return false;
   }
   std::array<std::uint8_t,1024> raw_pal{};
   if(!nb.read_bytes(raw_pal.data(),pal_bytes)){
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL8 palette read failed";return false;
   }
   std::vector<std::uint8_t> indices(idx_bytes);
   if(is_swizzled){
    std::vector<std::uint8_t> raw_swizzled(payload_bytes);
    if(!nb.read_bytes(raw_swizzled.data(),payload_bytes)){
     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PAL8 index read failed";return false;
    }
    for(std::uint32_t y=0;y<height;++y){
     for(std::uint32_t x=0;x<width;++x){
      const std::size_t src_off=ps2_swizzle_offset_t8(x,y,width);
      if(src_off>=raw_swizzled.size()){
       rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD swizzled PAL8 offset out of bounds";return false;
      }
      indices[static_cast<std::size_t>(y)*width+x]=raw_swizzled[src_off];
     }
    }
   }else{
    if(!nb.read_bytes(indices.data(),idx_bytes)){
     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD PAL8 index read failed";return false;
    }
   }
   auto csm1_unswizzle=[](std::size_t idx)noexcept->std::size_t{
    const std::size_t bank=idx&0x18u;
    if(bank==0x08u||bank==0x10u)return idx^0x18u;
    return idx;
   };
   auto expand_gs_alpha=[](std::uint8_t a)noexcept->std::uint8_t{
    return (a>=0x80u)?0xFFu:static_cast<std::uint8_t>((static_cast<std::uint32_t>(a)*255u+64u)/128u);
   };
   try{decoded_rgba.resize(rgba_bytes);}catch(...){
    rollback();failure.kind=RetailRendererTextureError::allocation;failure.operation="TXD PAL8 decode allocation";return false;
   }
   for(std::size_t i=0;i<total_pixels;++i){
    const std::size_t src_idx=csm1_unswizzle(static_cast<std::size_t>(indices[i]))*4u;
    const std::size_t out_off=i*4u;
    decoded_rgba[out_off+0]=static_cast<std::byte>(raw_pal[src_idx+0]);
    decoded_rgba[out_off+1]=static_cast<std::byte>(raw_pal[src_idx+1]);
    decoded_rgba[out_off+2]=static_cast<std::byte>(raw_pal[src_idx+2]);
    decoded_rgba[out_off+3]=static_cast<std::byte>(expand_gs_alpha(raw_pal[src_idx+3]));
   }
  }

  RetailRendererRasterFailure rf;
  auto rh=rasters.create_raster({static_cast<std::int32_t>(width),static_cast<std::int32_t>(height),32,4u,0x0500u,0x0500u},rf);
  if(!rh){
   rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation=rf.operation;return false;
  }
  created_rasters.push_back(*rh);
  auto locked=rasters.lock_pixels(*rh,rf);
  if(!locked||locked->size()<rgba_bytes){
   rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD raster pixel lock failed";return false;
  }
  if(is_rgba32&&!is_swizzled){
   if(!nb.read_bytes(locked->data(),rgba_bytes)){
    (void)rasters.unlock_pixels(*rh,rf);
    rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation="TXD pixel copy failed";return false;
   }
  }else{
   std::memcpy(locked->data(),decoded_rgba.data(),rgba_bytes);
  }
  (void)rasters.unlock_pixels(*rh,rf);

  const auto filter=static_cast<RetailRendererTextureFilter>(filter_addr&0xFFu);
  const auto addr_u=static_cast<RetailRendererTextureAddress>((filter_addr>>8u)&0x0Fu);
  const auto addr_v=static_cast<RetailRendererTextureAddress>((filter_addr>>12u)&0x0Fu);
  RetailRendererTextureDescription tdesc{
   std::string(name_buf),std::string(mask_buf),
   filter==RetailRendererTextureFilter::none?RetailRendererTextureFilter::linear:filter,
   addr_u==static_cast<RetailRendererTextureAddress>(0)?RetailRendererTextureAddress::wrap:addr_u,
   addr_v==static_cast<RetailRendererTextureAddress>(0)?RetailRendererTextureAddress::wrap:addr_v
  };
  auto th=textures.create_texture(std::move(tdesc),*rh,failure);
  if(!th){
   rollback();return false;
  }
  created_textures.push_back(*th);
  ++parsed;
 }
 if(parsed!=count){
  rollback();failure.kind=RetailRendererTextureError::invalid_handle;failure.operation="TXD stream ended before declared texture count";return false;
 }
 return true;
}

bool ingest_rw_world_bsp_stream(
 std::span<const std::byte> stream_bytes,
 std::uint64_t base_sector_id,
 NativeRetailRendererMaterials& materials,
 NativeRetailRendererTextures& textures,
 NativeRetailWorld& world,
 RetailRendererWorldFailure& failure) noexcept {
 failure={};
 if(base_sector_id==0||stream_bytes.size()<12){
  failure.kind=RetailRendererWorldError::invalid_handle;
  failure.operation="RW world stream requires non-zero base_sector_id and 12-byte chunk header";
  return false;
 }
 RwStreamCursor cur{stream_bytes,0};
 std::uint32_t type=0,len=0,lib=0;
 if(!cur.read_chunk(type,len,lib)||type!=0x000Bu){
  failure.kind=RetailRendererWorldError::invalid_handle;
  failure.operation="RW world stream requires rwID_WORLD (0x000B) root chunk";
  return false;
 }
 RwStreamCursor body{cur.data.subspan(cur.pos,len),0};
 std::uint32_t st=0,sl=0,slib=0;
 if(!body.read_chunk(st,sl,slib)||st!=0x0001u||sl<52){
  failure.kind=RetailRendererWorldError::invalid_handle;
  failure.operation="RW world struct missing or truncated";
  return false;
 }
 RwStreamCursor ws{body.data.subspan(body.pos,sl),0};
 if(!body.skip(sl))return false;
 std::uint32_t root_is_world=0,num_tris=0,num_verts=0,num_planes=0,num_atomics=0,col_size=0,world_flags=0;
 float wmin[3]{},wmax[3]{};
 if(!ws.read_u32(root_is_world))return false;
 for(int i=0;i<3;++i)if(!ws.read_f32(wmin[i])){failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW world bbox min invalid";return false;}
 for(int i=0;i<3;++i)if(!ws.read_f32(wmax[i])){failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW world bbox max invalid";return false;}
 if(!ws.read_u32(num_tris)||!ws.read_u32(num_verts)||!ws.read_u32(num_planes)||
   !ws.read_u32(num_atomics)||!ws.read_u32(col_size)||!ws.read_u32(world_flags)){
  failure.kind=RetailRendererWorldError::invalid_handle;
  failure.operation="RW world counts truncated";
  return false;
 }

 std::vector<NativeRetailMaterialHandle> created_mats;
 std::vector<NativeRetailMaterialHandle> mat_list;
 auto rollback=[&]()noexcept{
  world.clear_bsp_sectors();
  RetailRendererMaterialFailure mf;
  for(const auto& mh:created_mats)(void)materials.destroy_material(mh,mf);
 };

 std::int32_t root_idx=-1;
 std::uint64_t next_id=base_sector_id;
 while(body.remaining()>=12){
  std::uint32_t ct=0,cl=0,clib=0;
  if(!body.read_chunk(ct,cl,clib)){
   rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW world child chunk truncated";return false;
  }
  RwStreamCursor cbody{body.data.subspan(body.pos,cl),0};
  if(!body.skip(cl))return false;
  if(ct==0x0008u){
   if(!parse_rw_matlist(cbody,materials,textures,created_mats,mat_list)){
    rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW world matlist ingestion failed";return false;
   }
  }else if(ct==0x0009u||ct==0x000Au){
   if(!parse_rw_bsp_node(cbody,ct,next_id,mat_list,world_flags,world,root_idx)){
    rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW world BSP sector ingestion failed";return false;
   }
  }
 }
 if(root_idx<0){
  rollback();failure.kind=RetailRendererWorldError::invalid_handle;failure.operation="RW world contained no BSP sectors";return false;
 }
 world.set_bsp_root(root_idx);
 return true;
}

std::optional<RetailSceneStreamLoadResult> load_retail_scene_streams_from_volume(
 const mka::content::NativeContentVolume& volume,
 std::string_view txd_resource_key,
 std::string_view world_resource_key,
 std::uint64_t base_sector_id,
 NativeRetailRendererRasters& rasters,
 NativeRetailRendererTextures& textures,
 NativeRetailRendererMaterials& materials,
 NativeRetailWorld& world,
 RetailRendererWorldFailure& failure) noexcept {
 failure={};
 auto read_stream_bytes=[&](std::string_view key,std::vector<std::byte>& out)noexcept->bool{
  mka::content::ContentVolumeFailure cf;
  auto file_opt=volume.open_resource(key,cf);
  if(!file_opt){
   failure.kind=RetailRendererWorldError::invalid_handle;
   failure.operation="failed to open retail scene stream resource";
   return false;
  }
  const auto len_opt=file_opt->length(cf);
  if(!len_opt||*len_opt<12||*len_opt>(64ull*1024ull*1024ull)){
   failure.kind=RetailRendererWorldError::invalid_handle;
   failure.operation="retail scene stream length invalid or out of bounds";
   return false;
  }
  try{
   out.resize(static_cast<std::size_t>(*len_opt));
  }catch(...){
   failure.kind=RetailRendererWorldError::allocation;
   failure.operation="retail scene stream buffer allocation failed";
   return false;
  }
  const auto rd=file_opt->read(out.data(),out.size(),cf);
  if(cf.kind!=mka::content::ContentVolumeError::none||rd.bytes!=out.size()){
   failure.kind=RetailRendererWorldError::invalid_handle;
   failure.operation="retail scene stream read truncated";
   return false;
  }
  return true;
 };

 RetailSceneStreamLoadResult result;
 if(!txd_resource_key.empty()){
  std::vector<std::byte> txd_bytes;
  if(!read_stream_bytes(txd_resource_key,txd_bytes))return {};
  RetailRendererTextureFailure tf;
  if(!ingest_rw_tex_dictionary_stream(txd_bytes,rasters,textures,tf)){
   failure.kind=RetailRendererWorldError::invalid_handle;
   failure.operation=tf.operation;
   return {};
  }
  result.bytes_read+=txd_bytes.size();
  result.txd_ingested=true;
 }
 if(!world_resource_key.empty()){
  std::vector<std::byte> world_bytes;
  if(!read_stream_bytes(world_resource_key,world_bytes))return {};
  if(!ingest_rw_world_bsp_stream(world_bytes,base_sector_id,materials,textures,world,failure))return {};
  result.bytes_read+=world_bytes.size();
  result.world_ingested=true;
 }
 return result;
}

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
 const GLboolean prior_blend=glIsEnabled(GL_BLEND);
 GLboolean prior_depth_mask=GL_TRUE;
 glGetBooleanv(GL_DEPTH_WRITEMASK,&prior_depth_mask);
 GLint prior_blend_src=GL_ONE,prior_blend_dst=GL_ZERO;
 glGetIntegerv(GL_BLEND_SRC,&prior_blend_src);
 glGetIntegerv(GL_BLEND_DST,&prior_blend_dst);
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
 glDisable(GL_BLEND);
 glDepthMask(GL_TRUE);
 glMatrixMode(GL_PROJECTION);
 glPushMatrix();
 glLoadMatrixf(cam_obs->projection_matrix.elements.data());
 glMatrixMode(GL_MODELVIEW);
 glPushMatrix();
#endif

 auto restore_gl_state=[&]()noexcept{
#ifdef _WIN32
  glDepthMask(prior_depth_mask);
  glBlendFunc(static_cast<GLenum>(prior_blend_src),static_cast<GLenum>(prior_blend_dst));
  if(prior_blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,prior_tex_env);
  glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(prior_tex_binding));
  if(!prior_tex2d)glDisable(GL_TEXTURE_2D);else glEnable(GL_TEXTURE_2D);
  glMatrixMode(GL_MODELVIEW);glPopMatrix();
  glMatrixMode(GL_PROJECTION);glPopMatrix();
  glMatrixMode(static_cast<GLenum>(prior_matrix_mode));
  glDepthFunc(static_cast<GLenum>(prior_depth_func));
  if(!prior_depth)glDisable(GL_DEPTH_TEST);
#endif
 };

 const auto& world_lights=world->lights();
 const bool has_lights=!world_lights.empty();
 std::array<float,3> ambient_rgb{0.0f,0.0f,0.0f};
 if(has_lights){
  for(const auto& l:world_lights){
   if(l.enabled&&l.type==RetailWorldLightType::ambient){
    ambient_rgb[0]+=l.color[0];
    ambient_rgb[1]+=l.color[1];
    ambient_rgb[2]+=l.color[2];
   }
  }
 }

 auto mul_affine_local=[](const RetailRendererAffine& a,const RetailRendererAffine& b)noexcept->RetailRendererAffine{
  const auto dot=[](const RetailRendererPoint3& v,float bx,float ux,float ax)noexcept{
   return v.x*bx+v.y*ux+v.z*ax;
  };
  RetailRendererAffine out;
  out.right={dot(a.right,b.right.x,b.up.x,b.at.x),dot(a.right,b.right.y,b.up.y,b.at.y),dot(a.right,b.right.z,b.up.z,b.at.z)};
  out.up={dot(a.up,b.right.x,b.up.x,b.at.x),dot(a.up,b.right.y,b.up.y,b.at.y),dot(a.up,b.right.z,b.up.z,b.at.z)};
  out.at={dot(a.at,b.right.x,b.up.x,b.at.x),dot(a.at,b.right.y,b.up.y,b.at.y),dot(a.at,b.right.z,b.up.z,b.at.z)};
  out.translation={
   dot(a.translation,b.right.x,b.up.x,b.at.x)+b.translation.x,
   dot(a.translation,b.right.y,b.up.y,b.at.y)+b.translation.y,
   dot(a.translation,b.right.z,b.up.z,b.at.z)+b.translation.z
  };
  return out;
 };

 auto xform_pt=[](const RetailRendererAffine& m,const RetailRendererPoint3& p)noexcept->RetailRendererPoint3{
  return {
   p.x*m.right.x+p.y*m.up.x+p.z*m.at.x+m.translation.x,
   p.x*m.right.y+p.y*m.up.y+p.z*m.at.y+m.translation.y,
   p.x*m.right.z+p.y*m.up.z+p.z*m.at.z+m.translation.z
  };
 };
 auto xform_dir=[](const RetailRendererAffine& m,const RetailRendererPoint3& n)noexcept->RetailRendererPoint3{
  return {
   n.x*m.right.x+n.y*m.up.x+n.z*m.at.x,
   n.x*m.right.y+n.y*m.up.y+n.z*m.at.y,
   n.x*m.right.z+n.y*m.up.z+n.z*m.at.z
  };
 };

 auto shade_color=[&](const std::array<float,4>& base_rgba,const RetailRendererPoint3& world_n)noexcept->std::array<float,4>{
  if(!has_lights)return base_rgba;
  const float nlen=std::sqrt(world_n.x*world_n.x+world_n.y*world_n.y+world_n.z*world_n.z);
  const RetailRendererPoint3 N=(nlen>1e-6f)?RetailRendererPoint3{world_n.x/nlen,world_n.y/nlen,world_n.z/nlen}:RetailRendererPoint3{0,0,1};
  std::array<float,3> light_rgb=ambient_rgb;
  for(const auto& l:world_lights){
   if(!l.enabled||l.type!=RetailWorldLightType::directional)continue;
   const float dlen=std::sqrt(l.direction.x*l.direction.x+l.direction.y*l.direction.y+l.direction.z*l.direction.z);
   if(dlen<=1e-6f)continue;
   const float ndotl=std::max(0.0f,-(N.x*(l.direction.x/dlen)+N.y*(l.direction.y/dlen)+N.z*(l.direction.z/dlen)));
   light_rgb[0]+=ndotl*l.color[0];
   light_rgb[1]+=ndotl*l.color[1];
   light_rgb[2]+=ndotl*l.color[2];
  }
  return {
   std::min(1.0f,base_rgba[0]*light_rgb[0]),
   std::min(1.0f,base_rgba[1]*light_rgb[1]),
   std::min(1.0f,base_rgba[2]*light_rgb[2]),
   base_rgba[3]
  };
 };

 struct AlphaEntry {
  std::size_t obj_index{};
  float cam_z{};
  RetailRendererAffine model_world;
 };
 std::vector<AlphaEntry> alpha_queue;

 auto submit_object=[&](const RetailWorldSceneObject& obj,const RetailRendererAffine& m)noexcept->bool{
  std::vector<RetailRendererAffine> palette;
  if(!obj.bone_frames.empty()){
   palette.reserve(obj.bone_frames.size());
   for(std::size_t bi=0;bi<obj.bone_frames.size();++bi){
    const auto bf=frames.observe(obj.bone_frames[bi],failure.frame);
    if(!bf){failure.operation=failure.frame.operation;return false;}
    if(bi<obj.inv_bind_matrices.size()){
     palette.push_back(mul_affine_local(obj.inv_bind_matrices[bi].affine,bf->world.affine));
    }else{
     palette.push_back(bf->world.affine);
    }
   }
  }else if(!obj.bone_matrices.empty()){
   palette.reserve(obj.bone_matrices.size());
   for(const auto& bm:obj.bone_matrices)palette.push_back(bm.affine);
  }

  std::array<float,4> mat_color{1.0f,1.0f,1.0f,1.0f};
  resolve_material_and_bind_texture(obj.material,*world,materials,textures,rasters,mat_color,stats.bound_textures);
#ifdef _WIN32
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
   RetailRendererPoint3 pos=v.position;
   RetailRendererPoint3 nrm=v.normal;
   const float wsum=v.bone_weights[0]+v.bone_weights[1]+v.bone_weights[2]+v.bone_weights[3];
   if(wsum>0.0f&&!palette.empty()){
    RetailRendererPoint3 sp{0,0,0},sn{0,0,0};
    for(int k=0;k<4;++k){
     const float w=v.bone_weights[k]/wsum;
     if(w<=0.0f||v.bone_indices[k]>=palette.size())continue;
     const auto& bm=palette[v.bone_indices[k]];
     const auto tp=xform_pt(bm,v.position);
     const auto tn=xform_dir(bm,v.normal);
     sp.x+=w*tp.x;sp.y+=w*tp.y;sp.z+=w*tp.z;
     sn.x+=w*tn.x;sn.y+=w*tn.y;sn.z+=w*tn.z;
    }
    pos=sp;nrm=sn;
   }
   const auto world_n=xform_dir(m,nrm);
   const std::array<float,4> base_c{
    v.color[0]*mat_color[0],v.color[1]*mat_color[1],v.color[2]*mat_color[2],v.color[3]*mat_color[3]
   };
   const auto shaded=shade_color(base_c,world_n);
   glTexCoord2f(v.u,v.v);
   glColor4f(shaded[0],shaded[1],shaded[2],shaded[3]);
   glVertex3f(pos.x,pos.y,pos.z);
  }
  glEnd();
#endif
  stats.submitted_triangles+=obj.triangles.size()/3;
  return true;
 };

 const auto& objs=world->scene_objects();
 for(std::size_t oi=0;oi<objs.size();++oi){
  const auto& obj=objs[oi];
  if(!obj.visible)continue;
  ++stats.total_objects;
  const auto f_obs=frames.observe(obj.frame,failure.frame);
  if(!f_obs){
   failure.operation=failure.frame.operation;
   restore_gl_state();
   cleanup_end();
   return {};
  }
  const auto& m=f_obs->world.affine;
  const auto world_center=xform_pt(m,obj.local_center);
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

  bool is_alpha=obj.alpha_blended;
  if(!is_alpha&&!obj.material.empty()&&materials!=nullptr){
   RetailRendererMaterialFailure mf;
   if(const auto mo=materials->observe(obj.material,mf);mo&&mo->color.a<255u)is_alpha=true;
  }
  if(is_alpha){
   ++stats.alpha_objects;
   const auto& vm=cam_obs->view_matrix.elements;
   const float cam_z=vm[2]*world_center.x+vm[6]*world_center.y+vm[10]*world_center.z+vm[14];
   try{alpha_queue.push_back({oi,cam_z,m});}catch(...){}
   continue;
  }
  ++stats.opaque_objects;
  if(!submit_object(obj,m)){
   restore_gl_state();
   cleanup_end();
   return {};
  }
 }

 if(world->bsp_root_sector()>=0&&
   static_cast<std::size_t>(world->bsp_root_sector())<world->bsp_sectors().size()){
  const auto& sectors=world->bsp_sectors();
  auto count_leaves=[&](auto& self,std::int32_t idx)noexcept->std::size_t{
   if(idx<0||static_cast<std::size_t>(idx)>=sectors.size())return 0;
   const auto& s=sectors[static_cast<std::size_t>(idx)];
   if(s.is_leaf)return 1;
   return self(self,s.left_child)+self(self,s.right_child);
  };
  auto traverse=[&](auto& self,std::int32_t idx)noexcept->void{
   if(idx<0||static_cast<std::size_t>(idx)>=sectors.size())return;
   const auto& s=sectors[static_cast<std::size_t>(idx)];
   if(!sphere_in_frustum(cam_obs->frustum_planes,s.bsphere_center,s.bsphere_radius)){
    stats.culled_sectors+=count_leaves(count_leaves,idx);
    return;
   }
   if(!s.is_leaf){
    self(self,s.left_child);
    self(self,s.right_child);
    return;
   }
   ++stats.visible_sectors;
   std::array<float,4> sec_color{1.0f,1.0f,1.0f,1.0f};
   resolve_material_and_bind_texture(s.material,*world,materials,textures,rasters,sec_color,stats.bound_textures);
#ifdef _WIN32
   glLoadMatrixf(cam_obs->view_matrix.elements.data());
   glBegin(GL_TRIANGLES);
   for(const auto& v:s.triangles){
    const std::array<float,4> base_c{
     v.color[0]*sec_color[0],v.color[1]*sec_color[1],v.color[2]*sec_color[2],v.color[3]*sec_color[3]
    };
    const auto shaded=shade_color(base_c,v.normal);
    glTexCoord2f(v.u,v.v);
    glColor4f(shaded[0],shaded[1],shaded[2],shaded[3]);
    glVertex3f(v.position.x,v.position.y,v.position.z);
   }
   glEnd();
#endif
   stats.submitted_triangles+=s.triangles.size()/3;
  };
  stats.total_sectors=count_leaves(count_leaves,world->bsp_root_sector());
  traverse(traverse,world->bsp_root_sector());
 }

 // Pass 2: Back-to-front sorted translucent/alpha-blended pass.
 if(!alpha_queue.empty()){
  std::sort(alpha_queue.begin(),alpha_queue.end(),[](const AlphaEntry& a,const AlphaEntry& b)noexcept{
   return a.cam_z>b.cam_z;
  });
#ifdef _WIN32
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
#endif
  for(const auto& ae:alpha_queue){
   if(!submit_object(objs[ae.obj_index],ae.model_world)){
    restore_gl_state();
    cleanup_end();
    return {};
   }
  }
 }

 restore_gl_state();

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
