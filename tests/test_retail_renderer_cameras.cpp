#include "mka/retail_renderer_cameras.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_renderer_world.hpp"
#include "mka/retail_renderer_frames.hpp"
#include "mka/retail_renderer_streams.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
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
#include <bit>
#include <array>
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int fail_after=-1;}
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
enum class Later {refuse,transfer,allow};Later later{Later::refuse};
NativeRetailMainState* transfer_main{};unsigned suffix_calls{},module_cuts{},camera_cuts{},golden_cases{};
std::shared_ptr<NativeRetailRendererCameraModule> captured;NativeRetailRendererCameraHandle captured_camera;
RetailRendererCameraDepthInputs explicit_fixture_depth(){return {0,1};}
bool fixture_construct(unsigned id,NativeRetailRendererCore& core,RetailRendererCoreFailure&){
 // Actual0406/0407 now link with the raster owner. The historical test-only
 // later0406 boundary moves to0408; actual camera-prefix lifetime checks remain.
 if(id==0x409&&later!=Later::allow){
  RetailRendererCoreFailure cf;captured=core.camera_module(cf);check(bool(captured),"later fixture sees actual0405 owner");
  RetailRendererCameraFailure f;auto h=captured->create_base_camera(explicit_fixture_depth(),f);
  check(bool(h),"real base-camera allocation before later fixture");captured_camera=*h;
  if(later==Later::transfer)reenter_native_retail_main(*transfer_main,79);
  return false;
 }
 if(later!=Later::allow)std::abort();return true;
}
void fixture_destroy(unsigned)noexcept{if(later!=Later::allow)std::abort();}
struct Fixture {
 NativeRetailStartupState startup;mka::input::ControllerInput input;NativeRetailMainState main;
 StartupFailure sf;RetailRendererCoreFailure f;RetailRendererAllocatorFailure af;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
 explicit Fixture(const std::filesystem::path& root):startup(open(root),{}),main(startup,input){
  check(initialize_native_retail_game_session(startup,sf)&&initialize_retail_memory_owners(startup,sf),"actual Session/native heaps");
  allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual Permanent capability");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& root){mka::content::ContentVolumeFailure f;
  auto v=mka::content::NativeContentVolume::open(root,f);check(bool(v),"actual content root");return std::move(*v);}
 std::shared_ptr<NativeRetailRendererCore> core(){auto c=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);check(bool(c),"actual persistent Core");return c;}
 std::shared_ptr<NativeRetailRendererCameraModule> module(){
  auto c=core();auto initialized=c->initialize_core(f);check(initialized&&*initialized,"actual CoreInit/registration");
  check(renderer_required::construct_core_plugin_0405(*c,f),"ACTUAL0405 resource constructor");return c->camera_module(f);
 }
 std::size_t live(){RetailMemoryOwnerFailure f;auto o=startup.memory_owners->observe(RetailMemoryRole::permanent,f);check(bool(o),"actual Permanent observation");return o->heap.actual_live_allocations;}
 void tasks(){RetailGamePoolFailure pf;startup.game_pool=std::shared_ptr<NativeRetailGamePool>(NativeRetailGamePool::initialize(3,pf));
  RetailGameSessionFailure sf;RetailTaskDomainFailure df;auto* d=startup.game_session->task_domain(sf);
  check(startup.game_pool&&d&&d->reset_task_roots_from_12f0d0(startup.game_pool,df),"actual task roots");}
};
NativeRetailRendererCameraHandle create(const std::shared_ptr<NativeRetailRendererCameraModule>& module){
 RetailRendererCameraFailure f;auto h=module->create_base_camera(explicit_fixture_depth(),f);check(bool(h),"actual typed base-camera creation");return *h;
}
unsigned association_checks{},association_scenarios{};
void association_check(bool ok,const char* why){++association_checks;check(ok,why);}
struct CameraFrameFixture {
 Fixture native;
 std::shared_ptr<NativeRetailRendererCore> core;
 std::shared_ptr<NativeRetailRendererFrames> frames;
 std::shared_ptr<NativeRetailRendererCameraModule> cameras;
 explicit CameraFrameFixture(const std::filesystem::path& root):native(root),core(native.core()){
  auto initialized=core->initialize_core(native.f);
  association_check(initialized&&*initialized,"association actual CoreInit");
  association_check(renderer_required::construct_core_plugin_0403(*core,native.f),"association actual0403 first");
  association_check(renderer_required::construct_core_plugin_0405(*core,native.f),"association actual0405 second");
  frames=core->frames(native.f);cameras=core->camera_module(native.f);
  association_check(frames&&cameras,"association uses the persistent Core resources");
 }
 NativeRetailRendererFrameHandle new_frame(){RetailRendererFrameFailure f;auto h=frames->create_base_frame(f);
  association_check(bool(h),"association actual frame allocation");return *h;}
 NativeRetailRendererCameraHandle new_camera(){return create(cameras);}
 void retire(){renderer_required::destroy_core_plugin_0405(*core);renderer_required::destroy_core_plugin_0403(*core);}
};
NativeRetailRendererFrameObjectHandle object_identity(const std::shared_ptr<NativeRetailRendererCameraModule>& module,
 const NativeRetailRendererCameraHandle& camera){
 RetailRendererCameraFailure f;auto identity=module->frame_object_identity(camera,f);
 association_check(bool(identity)&&!identity->empty(),"actual stable native camera frame-object identity");return *identity;
}
void object_order(const std::shared_ptr<NativeRetailRendererFrames>& frames,const NativeRetailRendererFrameHandle& frame,
 std::initializer_list<NativeRetailRendererFrameObjectHandle> expected){
 RetailRendererFrameFailure f;auto actual=frames->objects(frame,f);
 association_check(actual&&actual->size()==expected.size(),"source object-list cardinality");
 std::size_t index{};for(const auto& identity:expected)
  association_check((*actual)[index++].same_identity(identity),"source object-list order and native identity");
}
void camera_frame_association_cases(const std::filesystem::path& root){
 {
  ++association_scenarios;CameraFrameFixture x(root);RetailRendererCameraFailure f;RetailRendererFrameFailure ff;
  auto first=x.new_frame(),second=x.new_frame();auto a=x.new_camera(),b=x.new_camera(),c=x.new_camera();
  auto ia=object_identity(x.cameras,a),ib=object_identity(x.cameras,b),ic=object_identity(x.cameras,c);
  auto initial=x.cameras->frame(a,f);association_check(initial&&initial->empty(),"new real camera has no frame");
  object_order(x.frames,first,{});object_order(x.frames,second,{});
  association_check(x.cameras->set_frame(a,{},f),"unchanged-null source association returns native host success");
  initial=x.cameras->frame(a,f);auto clean_queue=x.frames->dirty_roots(ff);
  auto clean_first=x.frames->observe(first,ff),clean_second=x.frames->observe(second,ff);
  association_check(initial&&initial->empty()&&clean_queue&&clean_queue->empty()&&clean_first&&clean_second&&
   clean_first->private_flags==0&&clean_second->private_flags==0,"unchanged-null association has no list or frame-dirty effects");
  object_order(x.frames,first,{});object_order(x.frames,second,{});
  association_check(x.cameras->set_frame(a,first,f)&&x.cameras->set_frame(b,first,f)&&x.cameras->set_frame(c,first,f),"source prepend three real cameras");
  object_order(x.frames,first,{ic,ib,ia});
  auto current=x.cameras->frame(b,f);association_check(current&&current->same_identity(first),"actual camera/frame association getter");
  fail_after=0;const bool rebound=x.cameras->set_frame(b,first,f);fail_after=-1;
  association_check(rebound,"same-frame source rebind has no hidden allocation and is not a no-op");
  object_order(x.frames,first,{ib,ic,ia});
  fail_after=0;auto refused_snapshot=x.frames->objects(first,ff);fail_after=-1;
  association_check(!refused_snapshot&&ff.kind==RetailRendererFrameError::allocation,"object-list snapshot allocation failure is explicit");
  object_order(x.frames,first,{ib,ic,ia});
  association_check(x.cameras->set_frame(c,second,f),"source move between actual frame object lists");
  object_order(x.frames,first,{ib,ia});object_order(x.frames,second,{ic});
  auto queue=x.frames->dirty_roots(ff);
  association_check(queue&&queue->size()==2&&(*queue)[0].same_identity(second)&&(*queue)[1].same_identity(first),"new frame dirtied once after source attachment");
  association_check(x.cameras->set_frame(b,{},f),"source null frame unlinks without replacement");
  current=x.cameras->frame(b,f);association_check(current&&current->empty(),"explicit detach exposes actual null association");
  object_order(x.frames,first,{ia});
  queue=x.frames->dirty_roots(ff);
  association_check(queue&&queue->size()==2&&(*queue)[0].same_identity(second)&&(*queue)[1].same_identity(first),"detach preserves existing dirty-root order");
  association_check(x.cameras->destroy_base_camera(a,f),"camera destruction unlinks the remaining source object");
  object_order(x.frames,first,{});
  auto replacement=x.new_camera();auto ir=object_identity(x.cameras,replacement);
  association_check(!ir.same_identity(ia),"retired object identity cannot alias reused native allocation");
  association_check(!x.cameras->set_frame(a,first,f)&&f.kind==RetailRendererCameraError::retired_camera,"retired camera cannot relink");
  association_check(!x.frames->destroy_leaf(second,ff)&&ff.kind==RetailRendererFrameError::attached_objects_required,"frame deletion refuses actual attached camera");
  object_order(x.frames,second,{ic});
  renderer_required::destroy_core_plugin_0405(*x.core);
  object_order(x.frames,first,{});object_order(x.frames,second,{});
  association_check(!x.cameras->frame(c,f)&&f.kind==RetailRendererCameraError::retired,"module retirement rejects held camera handles");
  association_check(x.frames->destroy_leaf(first,ff)&&x.frames->destroy_leaf(second,ff),"camera-module close unlinks before frame payload release");
  renderer_required::destroy_core_plugin_0403(*x.core);
 }
 {
  ++association_scenarios;CameraFrameFixture x(root),foreign(root);RetailRendererCameraFailure f;RetailRendererFrameFailure ff;
  auto first=x.new_frame(),second=x.new_frame(),dead=x.new_frame(),other=foreign.new_frame();
  auto a=x.new_camera(),b=x.new_camera(),unattached=x.new_camera(),other_camera=foreign.new_camera();
  auto ia=object_identity(x.cameras,a),ib=object_identity(x.cameras,b);
  association_check(x.cameras->set_frame(a,first,f)&&x.cameras->set_frame(b,first,f),"actual source list before failed associations");
  association_check(!x.cameras->set_frame(unattached,other,f)&&f.kind==RetailRendererCameraError::frame_binding&&f.frame.kind==RetailRendererFrameError::foreign_owner,"initially unattached camera rejects foreign Session before first parent publication");
  auto never_attached=x.cameras->frame(unattached,f);
  association_check(never_attached&&never_attached->empty(),"failed first foreign-Session attachment retains null parent");
  object_order(x.frames,first,{ib,ia});object_order(foreign.frames,other,{});
  association_check(!x.cameras->set_frame(a,other,f)&&f.kind==RetailRendererCameraError::frame_binding&&f.frame.kind==RetailRendererFrameError::foreign_owner,"foreign Session frame rejected before unlinking current membership");
  object_order(x.frames,first,{ib,ia});object_order(foreign.frames,other,{});
  association_check(x.frames->destroy_leaf(dead,ff),"retired frame fixture is physically released");
  association_check(!x.cameras->set_frame(a,dead,f)&&f.kind==RetailRendererCameraError::frame_binding&&f.frame.kind==RetailRendererFrameError::retired_frame,"retired frame rejected before source link changes");
  object_order(x.frames,first,{ib,ia});object_order(x.frames,second,{});
  association_check(!x.cameras->set_frame(other_camera,second,f)&&f.kind==RetailRendererCameraError::foreign_owner,"foreign camera rejected before frame access");
  association_check(!x.cameras->set_frame({},second,f)&&f.kind==RetailRendererCameraError::invalid_handle,"empty camera rejected before frame access");
  association_check(x.cameras->set_view_window(a,{2,4},f),"finite window before wrong-thread refusal");
  bool thread_rejected=false,thread_window_rejected=false;
  std::thread other_thread([&]{RetailRendererCameraFailure detail;
   thread_rejected=!x.cameras->set_frame(a,second,detail)&&detail.kind==RetailRendererCameraError::wrong_thread;
   thread_window_rejected=!x.cameras->set_view_window(a,{9,10},detail)&&detail.kind==RetailRendererCameraError::wrong_thread;
  });other_thread.join();association_check(thread_rejected,"foreign-thread association is rejected before links");
  auto unchanged=x.cameras->observe(a,f);
  association_check(thread_window_rejected&&unchanged&&unchanged->view_window.x==2&&unchanged->view_window.y==4,"wrong-thread setter rejects before source window prefix");
  object_order(x.frames,first,{ib,ia});object_order(x.frames,second,{});
  auto current=x.cameras->frame(a,f);association_check(current&&current->same_identity(first),"all failed associations preserve the old parent");
  x.retire();foreign.retire();
 }
 {
  ++association_scenarios;CameraFrameFixture x(root);RetailRendererCameraFailure f;RetailRendererFrameFailure ff;
  auto parent=x.new_frame(),child=x.new_frame(),unrelated=x.new_frame(),other_root=x.new_frame();auto camera=x.new_camera();
  association_check(x.frames->add_child(parent,child,ff)&&x.frames->add_child(parent,unrelated,ff),"actual nested frame hierarchy, unrelated sibling first");
  association_check(x.frames->mark_dirty(other_root,ff),"independent dirty root before camera attachment");
  association_check(x.cameras->set_frame(camera,child,f),"camera attaches to descendant with existing native root");
  const auto identity=object_identity(x.cameras,camera);object_order(x.frames,child,{identity});
  association_check(x.cameras->set_view_window(camera,{std::bit_cast<float>(0x3fd9999au),2.5f},f),"attached finite nonsquare native window update");
  auto state=x.cameras->observe(camera,f);auto root_state=x.frames->observe(parent,ff),child_state=x.frames->observe(child,ff);
  association_check(state&&std::bit_cast<std::uint32_t>(state->view_window.x)==0x3fd9999au&&state->view_window.y==2.5f&&
   std::bit_cast<std::uint32_t>(state->inverse_window.x)==0x3f169696u&&std::bit_cast<std::uint32_t>(state->inverse_window.y)==0x3ecccccdu,"independent finite source reciprocal bits, not square-only computation");
  association_check(root_state&&child_state&&(root_state->private_flags&3u)==3u&&(child_state->private_flags&12u)==12u&&child_state->root.same_identity(parent),"attached setter reuses real descendant/root dirty effects");
  auto queue=x.frames->dirty_roots(ff);
  association_check(queue&&queue->size()==2&&(*queue)[0].same_identity(other_root)&&(*queue)[1].same_identity(parent),"already-dirty nested root is neither duplicated nor reordered");
  association_check(!x.cameras->require_render_ready(camera,f)&&f.kind==RetailRendererCameraError::render_services_required,"association and dirtying do not invent synchronized rendering");
  auto count=x.frames->observe_module(ff);
  association_check(!x.frames->destroy_root_hierarchy(parent,ff)&&ff.kind==RetailRendererFrameError::attached_objects_required,"whole hierarchy deletion refuses attached descendant before sibling release");
  auto after=x.frames->observe_module(ff);
  association_check(count&&after&&count->live_base_frames==after->live_base_frames&&x.frames->observe(unrelated,ff).has_value(),"refused hierarchy deletion leaves all unrelated payloads alive");
  object_order(x.frames,child,{identity});
  association_check(x.cameras->set_frame(camera,{},f)&&x.frames->destroy_root_hierarchy(parent,ff),"explicit source detach permits hierarchy release");
  association_check(x.cameras->destroy_base_camera(camera,f)&&x.frames->destroy_leaf(other_root,ff),"actual nested association cleanup");x.retire();
 }
 {
  ++association_scenarios;CameraFrameFixture x(root);RetailRendererCameraFailure f;RetailRendererFrameFailure ff;
  auto frame=x.new_frame();auto camera=x.new_camera(),unattached=x.new_camera();
  association_check(x.cameras->set_frame(camera,frame,f),"actual camera before frame-module retirement");
  renderer_required::destroy_core_plugin_0403(*x.core);
  association_check(!x.cameras->frame(camera,f)&&f.kind==RetailRendererCameraError::frame_binding&&f.frame.kind!=RetailRendererFrameError::none,"retired frame association is a named failure, not a detached camera");
  association_check(!x.cameras->set_view_window(camera,{4,8},f)&&f.kind==RetailRendererCameraError::frame_binding&&f.frame.kind!=RetailRendererFrameError::none,"stale attached setter reports required frame service failure");
  auto state=x.cameras->observe(camera,f);
  association_check(state&&state->view_window.x==4&&state->view_window.y==8&&state->inverse_window.x==0.25f&&state->inverse_window.y==0.125f,"stale attached-frame failure retains preceding source window/inverse effects");
  association_check(renderer_required::construct_core_plugin_0403(*x.core,x.native.f),"same Session can construct a new frame resource generation");
  auto new_frames=x.core->frames(x.native.f);auto fresh=new_frames->create_base_frame(ff);
  association_check(bool(fresh),"new frame generation allocates independently");
  association_check(!x.cameras->set_frame(unattached,frame,f)&&f.kind==RetailRendererCameraError::frame_binding,"old frame generation cannot bind through replacement module");
  auto current=x.cameras->frame(unattached,f);association_check(current&&current->empty(),"failed old-generation bind leaves new camera unattached");
  object_order(new_frames,*fresh,{});
  association_check(x.cameras->destroy_base_camera(camera,f)&&x.cameras->destroy_base_camera(unattached,f),"invalidated membership still allows safe camera ownership cleanup");
  association_check(new_frames->destroy_leaf(*fresh,ff),"new generation frame cleanup");x.retire();
 }
 {
  ++association_scenarios;std::shared_ptr<NativeRetailRendererCameraModule> cameras;
  std::shared_ptr<NativeRetailRendererFrames> frames;std::weak_ptr<NativeRetailGameSession> session;
  NativeRetailRendererCameraHandle camera;NativeRetailRendererFrameHandle frame;later=Later::allow;
  {Fixture x(root);auto core=x.core();auto initialized=core->initialize_core(x.f);
   association_check(initialized&&*initialized&&core->construct_registered(x.f),"actual registered Core prefix for associated Session lifetime");
   cameras=core->camera_module(x.f);frames=core->frames(x.f);session=x.startup.game_session;
   RetailRendererFrameFailure ff;auto created=frames->create_base_frame(ff);association_check(bool(created),"Session lifetime actual frame");frame=*created;
   camera=create(cameras);RetailRendererCameraFailure f;association_check(cameras->set_frame(camera,frame,f),"Session lifetime actual association");
  }
  RetailRendererCameraFailure f;RetailRendererFrameFailure ff;
  association_check(session.expired()&&!cameras->frame(camera,f)&&f.kind==RetailRendererCameraError::retired,"camera association cannot retain usable Session storage");
  association_check(!frames->objects(frame,ff)&&ff.kind==RetailRendererFrameError::retired,"held frame module is retired after actual reverse Core teardown");later=Later::refuse;
 }
 std::printf("PASS_NATIVE_CAMERA_FRAME_ASSOCIATION scenarios=%u checks=%u; actual0403/0405 membership/order/rebind/detach,weak identity,Session/thread/retirement refusals,hierarchy deletion guard,nested dirty roots,source window prefix on stale frame; full synchronization/game OPEN\n",association_scenarios,association_checks);
}
}
#ifdef _WIN32
namespace {
unsigned gpu_checks{},gpu_scenarios{};
void gpu_check(bool ok,const char* why){++gpu_checks;check(ok,why);}
constexpr GLenum draw_framebuffer=0x8ca9,read_framebuffer=0x8ca8;
constexpr GLenum draw_binding=0x8ca6,read_binding=0x8caa,color_attachment=0x8ce0;
constexpr GLenum renderbuffer=0x8d41,framebuffer_complete=0x8cd5;
template<class T>T gpu_proc(const char* name){
 const auto raw=wglGetProcAddress(name);const auto v=reinterpret_cast<std::uintptr_t>(raw);
 gpu_check(raw&&v>3&&v!=std::numeric_limits<std::uintptr_t>::max(),name);return reinterpret_cast<T>(raw);
}
struct RealFramebuffers {
 using Gen=void(APIENTRY*)(GLsizei,GLuint*);
 using Bind=void(APIENTRY*)(GLenum,GLuint);
 using Storage=void(APIENTRY*)(GLenum,GLenum,GLsizei,GLsizei);
 using Attach=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint);
 using Status=GLenum(APIENTRY*)(GLenum);
 using Delete=void(APIENTRY*)(GLsizei,const GLuint*);
 std::shared_ptr<NativeOffscreenDisplayService> display;
 Gen gen_fbo{gpu_proc<Gen>("glGenFramebuffers")},gen_rbo{gpu_proc<Gen>("glGenRenderbuffers")};
 Bind bind_fbo{gpu_proc<Bind>("glBindFramebuffer")},bind_rbo{gpu_proc<Bind>("glBindRenderbuffer")};
 Storage storage{gpu_proc<Storage>("glRenderbufferStorage")};
 Attach attach{gpu_proc<Attach>("glFramebufferRenderbuffer")};
 Status status{gpu_proc<Status>("glCheckFramebufferStatus")};
 Delete delete_fbo{gpu_proc<Delete>("glDeleteFramebuffers")},delete_rbo{gpu_proc<Delete>("glDeleteRenderbuffers")};
 GLuint fbo[2]{},rbo[2]{};
 explicit RealFramebuffers(std::shared_ptr<NativeOffscreenDisplayService> d):display(std::move(d)){
  gen_fbo(2,fbo);gen_rbo(2,rbo);
  for(unsigned i=0;i<2;++i){
   bind_fbo(draw_framebuffer,fbo[i]);bind_rbo(renderbuffer,rbo[i]);
   storage(renderbuffer,0x8058 /*RGBA8*/,8,8);attach(draw_framebuffer,color_attachment,renderbuffer,rbo[i]);
   glDrawBuffer(color_attachment);
   gpu_check(fbo[i]&&rbo[i]&&status(draw_framebuffer)==framebuffer_complete,"actual complete nonzero framebuffer");
  }
  bind_fbo(read_framebuffer,fbo[1]);glReadBuffer(color_attachment);
  bind_fbo(draw_framebuffer,fbo[0]);
  gpu_check(glGetError()==GL_NO_ERROR,"actual framebuffer allocation has no GPU error");
 }
 ~RealFramebuffers(){
  DisplayServiceFailure f;if(display&&display->acquire_context(f)){
   bind_fbo(draw_framebuffer,0);bind_fbo(read_framebuffer,0);
   delete_fbo(2,fbo);delete_rbo(2,rbo);
  }
 }
};
struct CameraGpuFixture {
 Fixture native;
 std::shared_ptr<NativeRetailRendererCore> core;
 std::shared_ptr<NativeRetailRendererCameraModule> cameras;
 std::shared_ptr<NativeRetailRendererRasters> rasters;
 explicit CameraGpuFixture(const std::filesystem::path& root):native(root),core(native.core()){
  auto initialized=core->initialize_core(native.f);gpu_check(initialized&&*initialized,"GPU actual CoreInit");
  gpu_check(renderer_required::construct_core_plugin_0405(*core,native.f),"GPU actual0405 constructor");
  gpu_check(renderer_required::construct_core_plugin_0407(*core,native.f),"GPU actual0407 constructor");
  cameras=core->camera_module(native.f);rasters=core->rasters(native.f);
  gpu_check(cameras&&rasters,"GPU real persistent camera/raster owners");
 }
 ~CameraGpuFixture(){renderer_required::destroy_core_plugin_0405(*core);renderer_required::destroy_core_plugin_0407(*core);}
 NativeRetailRasterHandle raster(std::uint32_t flags,std::int32_t w=640,std::int32_t h=448){
  RetailRendererRasterFailure f;auto r=rasters->create_raster({w,h,0,0,flags,0},f);
  gpu_check(bool(r),"GPU actual logical raster creation");return *r;
 }
 NativeRetailRasterHandle backed(std::uint32_t flags,const std::shared_ptr<NativeOffscreenDisplayService>& d,
                                 std::int32_t w=640,std::int32_t h=448){
  auto r=raster(flags,w,h);RetailRendererRasterFailure f;
  gpu_check(rasters->bind_display_target(r,d,f),"GPU explicit actual display binding");return r;
 }
 NativeRetailRendererCameraHandle camera(NativeRetailRasterHandle color,NativeRetailRasterHandle depth={},
  RetailRendererCameraTargetPolicy policy={{5,7,13,11},0.375f}){
  auto h=create(cameras);RetailRendererCameraFailure f;
  gpu_check(cameras->set_frame_buffer_raster(h,color,f)&&cameras->set_z_buffer_raster(h,depth,f)&&
            cameras->set_render_target_policy(h,policy,f),"GPU actual camera target/policy publication");return h;
 }
};
std::shared_ptr<NativeOffscreenDisplayService> native_display(){
 DisplayServiceFailure f;auto d=NativeOffscreenDisplayService::create(64,48,f,640,448);
 gpu_check(d&&d->acquire_context(f),"actual hidden native display/context");return d;
}
void gpu_binding(GLenum name,GLint expected,const char* why){GLint actual{};glGetIntegerv(name,&actual);gpu_check(actual==expected,why);}
void gpu_viewport(std::array<GLint,4> expected,const char* why){
 std::array<GLint,4> actual{};glGetIntegerv(GL_VIEWPORT,actual.data());gpu_check(actual==expected,why);
}
void save_gpu_bytes(const std::optional<std::filesystem::path>& output,const char* filename,
                    const void* bytes,std::size_t count){
 if(!output)return;
 const auto target=*output/filename;gpu_check(!std::filesystem::exists(target),"diagnostic output preserves existing bytes");
 std::ofstream stream(target,std::ios::binary);stream.write(static_cast<const char*>(bytes),static_cast<std::streamsize>(count));
 gpu_check(bool(stream),"explicit diagnostic GPU bytes written");
}
void camera_gpu_target_cases(const std::filesystem::path& root,const std::optional<std::filesystem::path>& output){
 {
  ++gpu_scenarios;CameraGpuFixture x(root);auto a=native_display();
  RetailRendererCameraFailure f;DisplayServiceFailure df;
  GLint depth_bits{},alpha_bits{};glGetIntegerv(GL_DEPTH_BITS,&depth_bits);glGetIntegerv(GL_ALPHA_BITS,&alpha_bits);
  gpu_check(depth_bits>0&&alpha_bits>=8,"actual default framebuffer depth and alpha availability queried");
  std::printf("NATIVE_CAMERA_GPU_CAPABILITY depth_bits=%d alpha_bits=%d physical=64x48 logical=640x448\n",depth_bits,alpha_bits);
  const auto color=x.backed(2,a),depth=x.backed(1,a);const auto h=x.camera(color,depth),second=x.camera(color,depth);
  gpu_check(a->logical_width()==640&&a->logical_height()==448&&a->target_width()==64&&a->target_height()==48,
            "logical raster extent is separate from explicit physical host target");
  auto bind=gpu_proc<RealFramebuffers::Bind>("glBindFramebuffer");
  bind(draw_framebuffer,0);glDrawBuffer(GL_BACK);bind(read_framebuffer,0);glReadBuffer(GL_BACK);
  glDisable(GL_SCISSOR_TEST);glDisable(GL_DITHER);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
  glClearColor(17.0f/255,34.0f/255,68.0f/255,1);glClearDepth(0.8125);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  gpu_check(glGetError()==GL_NO_ERROR,"real backbuffer background and depth initialized");
  glDrawBuffer(GL_FRONT); // Distinct default-object state must survive the backbuffer pass.
  RealFramebuffers prior(a);
  using Maski=void(APIENTRY*)(GLuint,GLboolean,GLboolean,GLboolean,GLboolean);
  using Booleani=void(APIENTRY*)(GLenum,GLuint,GLboolean*);
  using Integeri=void(APIENTRY*)(GLenum,GLuint,GLint*);
  using Floati=void(APIENTRY*)(GLenum,GLuint,GLfloat*);
  using Enablei=void(APIENTRY*)(GLenum,GLuint);
  using IsEnabledi=GLboolean(APIENTRY*)(GLenum,GLuint);
  using Scissori=void(APIENTRY*)(GLuint,GLint,GLint,GLsizei,GLsizei);
  using Viewporti=void(APIENTRY*)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat);
  const auto maski=gpu_proc<Maski>("glColorMaski");const auto booleani=gpu_proc<Booleani>("glGetBooleani_v");
  const auto integeri=gpu_proc<Integeri>("glGetIntegeri_v");const auto floati=gpu_proc<Floati>("glGetFloati_v");
  const auto disablei=gpu_proc<Enablei>("glDisablei");const auto enabledi=gpu_proc<IsEnabledi>("glIsEnabledi");
  const auto scissori=gpu_proc<Scissori>("glScissorIndexed");const auto viewporti=gpu_proc<Viewporti>("glViewportIndexedf");
  GLint draw_buffers{},viewports{};glGetIntegerv(0x8824 /*MAX_DRAW_BUFFERS*/,&draw_buffers);
  glGetIntegerv(0x825b /*MAX_VIEWPORTS*/,&viewports);
  gpu_check(draw_buffers>=1&&viewports>=1&&glGetError()==GL_NO_ERROR,"actual indexed GPU state limits queried");
  std::printf("NATIVE_CAMERA_GPU_INDEXED_STATE draw_buffers=%d viewports=%d\n",draw_buffers,viewports);
  prior.bind_fbo(draw_framebuffer,0);
  if(draw_buffers>1){
   using DrawBuffers=void(APIENTRY*)(GLsizei,const GLenum*);
   const auto draw_buffers_fn=gpu_proc<DrawBuffers>("glDrawBuffers");
   const GLenum mapping[]{GL_BACK_LEFT,GL_FRONT_LEFT};draw_buffers_fn(2,mapping);
   const auto optional_error=glGetError();
   std::printf("NATIVE_CAMERA_GPU_DEFAULT_TWO_BUFFER_MAP admitted=%u gl_error=%u\n",optional_error==GL_NO_ERROR,unsigned(optional_error));
   if(optional_error!=GL_NO_ERROR)glDrawBuffer(GL_FRONT);
  }
  std::vector<GLint> default_mapping(static_cast<std::size_t>(draw_buffers));
  for(GLint i=0;i<draw_buffers;++i)glGetIntegerv(0x8825 /*DRAW_BUFFER0*/+i,&default_mapping[i]);
  prior.bind_fbo(draw_framebuffer,prior.fbo[0]);
  gpu_check(glGetError()==GL_NO_ERROR,"default draw-buffer mapping independently captured");
  viewporti(0,2.25f,3.5f,19.75f,17.25f);glScissor(1,2,3,4);glEnable(GL_SCISSOR_TEST);glEnable(GL_DITHER);
  glColorMask(GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);glDepthMask(GL_FALSE);
  if(draw_buffers>1)maski(1,GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
  if(viewports>1){scissori(1,9,10,3,4);disablei(GL_SCISSOR_TEST,1);viewporti(1,23,24,25,26);}
  glEnable(0x8c89 /*RASTERIZER_DISCARD*/);glEnable(0x8db9 /*FRAMEBUFFER_SRGB*/);
  glClearColor(0.125f,0.25f,0.5f,0.75f);glClearDepth(0.625);
  gpu_check(x.cameras->begin_update(h,f)&&x.cameras->is_updating(h,f),"real camera begins owned default-display pass");
  gpu_binding(draw_binding,0,"begin binds actual default draw framebuffer");
  gpu_binding(read_binding,static_cast<GLint>(prior.fbo[1]),"begin leaves independent read framebuffer unchanged");
  gpu_viewport({5,7,13,11},"begin installs explicit lower-left physical viewport");
  gpu_check(!x.cameras->begin_update(second,f)&&f.kind==RetailRendererCameraError::native_target&&
            f.display.error==DisplayServiceError::resource_busy&&!x.cameras->is_updating(second,f),
            "second actual camera cannot steal active host target");
  gpu_check(!a->restore_abandoned_render_target(df)&&df.error==DisplayServiceError::resource_busy,
            "host recovery cannot steal live camera ownership");
  gpu_check(!x.cameras->begin_update(h,f)&&f.kind==RetailRendererCameraError::reentrant,"same-camera nested begin refused");
  gpu_check(!x.cameras->set_render_target_policy(h,{{0,0,8,8},0.5f},f)&&f.kind==RetailRendererCameraError::reentrant,
            "active target policy cannot change");
  gpu_check(!x.cameras->set_frame_buffer_raster(h,color,f)&&f.kind==RetailRendererCameraError::reentrant,
            "active color target cannot change");
  gpu_check(!x.cameras->set_z_buffer_raster(h,depth,f)&&f.kind==RetailRendererCameraError::reentrant,
            "active depth target cannot change");
  gpu_check(!x.cameras->clear(h,0,0,0,1,4,f)&&f.kind==RetailRendererCameraError::render_services_required,
            "unsupported stencil clear fails by name");
  gpu_check(!x.cameras->clear(h,std::numeric_limits<float>::quiet_NaN(),0,0,1,1,f)&&
            f.kind==RetailRendererCameraError::native_target&&f.display.error==DisplayServiceError::invalid_clear,
            "nonfinite clear color refused before command submission");
  glEnable(0xffffffffu);
  gpu_check(!x.cameras->clear(h,0,0,0,1,1,f)&&f.kind==RetailRendererCameraError::native_target&&
            f.display.error==DisplayServiceError::native_operation_failed,"incoming actual GL error refuses clear");
  gpu_check(x.cameras->clear(h,0.2f,0.4f,0.6f,0.8f,3,f),"real scissored GPU color/depth clear");
  std::array<GLfloat,4> clear_color{};GLdouble clear_depth{};GLboolean mask[4]{},depth_mask{};GLint scissor[4]{};
  glGetFloatv(GL_COLOR_CLEAR_VALUE,clear_color.data());glGetDoublev(GL_DEPTH_CLEAR_VALUE,&clear_depth);
  glGetBooleanv(GL_COLOR_WRITEMASK,mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);glGetIntegerv(GL_SCISSOR_BOX,scissor);
  gpu_check(clear_color==std::array<GLfloat,4>{0.125f,0.25f,0.5f,0.75f}&&clear_depth==0.625,
            "GPU clear restores host clear color/depth values");
  gpu_check(mask[0]==GL_FALSE&&mask[1]==GL_TRUE&&mask[2]==GL_FALSE&&mask[3]==GL_TRUE&&depth_mask==GL_FALSE,
            "GPU clear restores host color and depth write masks");
  gpu_check(glIsEnabled(GL_SCISSOR_TEST)&&glIsEnabled(GL_DITHER)&&scissor[0]==1&&scissor[1]==2&&scissor[2]==3&&scissor[3]==4,
            "GPU clear restores host scissor and dither");
  gpu_check(glIsEnabled(0x8c89)&&glIsEnabled(0x8db9),"GPU clear restores rasterizer discard and framebuffer sRGB enable");
  if(draw_buffers>1){
   GLboolean mask1[4]{};booleani(GL_COLOR_WRITEMASK,1,mask1);
   gpu_check(mask1[0]==GL_TRUE&&mask1[1]==GL_FALSE&&mask1[2]==GL_TRUE&&mask1[3]==GL_FALSE,
             "GPU clear preserves distinct indexed color mask1");
  }
  if(viewports>1){
   std::array<GLint,4> scissor1{};std::array<GLfloat,4> viewport1{};
   integeri(GL_SCISSOR_BOX,1,scissor1.data());floati(GL_VIEWPORT,1,viewport1.data());
   gpu_check(!enabledi(GL_SCISSOR_TEST,1)&&scissor1==std::array<GLint,4>{9,10,3,4}&&
             viewport1==std::array<GLfloat,4>{23,24,25,26},"GPU begin/clear preserves indexed scissor1 and viewport1");
  }
  gpu_binding(read_binding,static_cast<GLint>(prior.fbo[1]),"GPU clear leaves read framebuffer unchanged");
  gpu_check(x.cameras->end_update(h,f)&&!x.cameras->is_updating(h,f),"real camera ends owned GPU pass");
  gpu_binding(draw_binding,static_cast<GLint>(prior.fbo[0]),"end restores actual nonzero prior draw framebuffer");
  gpu_binding(read_binding,static_cast<GLint>(prior.fbo[1]),"end preserves independent actual read framebuffer");
  prior.bind_fbo(draw_framebuffer,0);
  bool mapping_restored=true;for(GLint i=0;i<draw_buffers;++i){GLint actual{};glGetIntegerv(0x8825+i,&actual);mapping_restored&=actual==default_mapping[i];}
  gpu_check(mapping_restored,"end restores full admitted default-object draw-buffer mapping");
  prior.bind_fbo(draw_framebuffer,prior.fbo[0]);gpu_binding(GL_DRAW_BUFFER,color_attachment,"prior nonzero framebuffer draw-buffer state retained");
  std::array<GLfloat,4> restored_viewport{};floati(GL_VIEWPORT,0,restored_viewport.data());
  gpu_check(restored_viewport==std::array<GLfloat,4>{2.25f,3.5f,19.75f,17.25f},"end restores fractional host viewport0 exactly");
  if(viewports>1){
   std::array<GLfloat,4> viewport1{};floati(GL_VIEWPORT,1,viewport1.data());
   gpu_check(viewport1==std::array<GLfloat,4>{23,24,25,26},"end leaves independent indexed viewport1 unchanged");
  }
  prior.bind_fbo(read_framebuffer,0);glReadBuffer(GL_BACK);
  std::vector<unsigned char> rgba(64*48*4);std::vector<float> depths(64*48);
  glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
  a->record_diagnostic_readback(2);gpu_check(glGetError()==GL_NO_ERROR,"real GPU color and depth readback succeeded");
  unsigned inside{},outside{};bool color_matches=true,depth_matches=true;
  for(unsigned y=0;y<48;++y)for(unsigned xx=0;xx<64;++xx){
   const bool in=xx>=5&&xx<18&&y>=7&&y<18;in?++inside:++outside;
   const std::array<int,4> expected=in?std::array<int,4>{51,102,153,204}:std::array<int,4>{17,34,68,255};
   for(unsigned k=0;k<4;++k)color_matches&=std::abs(int(rgba[(y*64+xx)*4+k])-expected[k])<=1;
   depth_matches&=std::abs(depths[y*64+xx]-(in?0.375f:0.8125f))<=2.0e-6f;
  }
  gpu_check(inside==143&&outside==2929&&color_matches,"actual RGBA bytes within1 inside viewport; outside pixels retained");
  gpu_check(depth_matches,"explicit0.375 GPU depth inside viewport; outside depth retained");
  save_gpu_bytes(output,"camera-target.rgba8.bin",rgba.data(),rgba.size());
  save_gpu_bytes(output,"camera-target.depth32f.bin",depths.data(),depths.size()*sizeof(float));
  prior.bind_fbo(read_framebuffer,prior.fbo[1]);
  const auto telemetry=a->telemetry();gpu_check(telemetry.diagnostic_readbacks==2&&telemetry.frame_readbacks==0&&
    telemetry.cpu_composited_pixels==0&&telemetry.fallback_pixels==0,"GPU-only clear, explicit diagnostic readbacks; no CPU/fallback pixels");
  glEnable(0xffffffffu);
  gpu_check(!x.cameras->begin_update(h,f)&&f.kind==RetailRendererCameraError::native_target&&
            f.display.error==DisplayServiceError::native_operation_failed&&!x.cameras->is_updating(h,f),
            "incoming actual GL error refuses begin before ownership publication");
  gpu_binding(draw_binding,static_cast<GLint>(prior.fbo[0]),"failed begin preserves prior framebuffer");
  floati(GL_VIEWPORT,0,restored_viewport.data());
  gpu_check(restored_viewport==std::array<GLfloat,4>{2.25f,3.5f,19.75f,17.25f},"failed begin preserves fractional prior viewport");
  prior.bind_fbo(draw_framebuffer,0);glDrawBuffer(GL_BACK);prior.bind_fbo(draw_framebuffer,prior.fbo[0]);
  gpu_check(x.cameras->begin_update(second,f)&&x.cameras->end_update(second,f),"second camera works after first owner ends");
  prior.bind_fbo(draw_framebuffer,0);gpu_binding(GL_DRAW_BUFFER,GL_BACK,"ordinary aggregate GL_BACK draw-buffer state restored");
  prior.bind_fbo(draw_framebuffer,prior.fbo[0]);
  gpu_check(glGetError()==GL_NO_ERROR,"positive target case ends without GPU errors");
 }
 {
  ++gpu_scenarios;CameraGpuFixture x(root);auto a=native_display();DisplayServiceFailure df;
  gpu_check(a->acquire_context(df),"acquire hidden GL context for 180ED0 scene render");
  gpu_check(renderer_required::construct_core_plugin_0403(*x.core,x.native.f),"GPU actual0403 constructor");
  auto frames=x.core->frames(x.native.f);
  gpu_check(bool(frames),"GPU real persistent frame owner");
  RetailGameSessionFailure gsf;
  auto* world_cams=x.native.startup.game_session->render_cameras(gsf);
  gpu_check(world_cams!=nullptr,"session render_cameras available");
  RetailCameraWorldFailure wf;
  gpu_check(world_cams->store_initial_count_from_127260(wf),"admit 127260 camera count");
  RetailCameraInit180ED0Config cfg;
  cfg.width=640;cfg.height=448;cfg.fov_degrees=60.0f;cfg.near_clip=0.5f;cfg.far_clip=50.0f;
  cfg.create_scene_camera=false;cfg.primary_has_color=true;cfg.primary_has_depth=true;
  cfg.depth_inputs={0.0f,1.0f};cfg.display=a;
  cfg.target_policy=RetailRendererCameraTargetPolicy{{8,6,48,36},1.0f};
  RetailCameraProducerFailure pf;
  auto init=initialize_cameras_180ed0(x.native.startup.game_session,*x.cameras,*frames,*x.rasters,
   *world_cams,cfg,pf);
  gpu_check(init.has_value(),"180ED0 camera initialization succeeded for GPU scene render");
  RetailRendererFrameFailure ff;
  RetailRendererMatrixValue cam_modelling{{{1,0,0},{0,1,0},{0,0,1},{0,0,-4.0f}},0x20000u};
  gpu_check(frames->set_modelling(init->primary.frame,cam_modelling,ff),"set 180ED0 camera frame translation");
  RetailRendererCameraFailure f;
  gpu_check(x.cameras->synchronize(init->primary.camera,*frames,f),"synchronize 180ED0 camera view and frustum");
  const auto obs=x.cameras->observe(init->primary.camera,f);
  gpu_check(obs&&obs->synchronized_view_available,"synchronized view and frustum available");
  for(const auto& plane:obs->frustum_planes){
   const float dist=plane.d; // origin (0,0,0) is at z_cam=+4 inside [0.5, 50]
   gpu_check(dist>0.0f,"origin lies inside all 6 synchronized frustum planes");
  }
  gpu_check(x.cameras->begin_update(init->primary.camera,f)&&
            x.cameras->clear(init->primary.camera,0.1f,0.1f,0.2f,1.0f,3u,f),
            "begin_update and clear succeeded on 180ED0 camera");
  const GLboolean prior_depth=glIsEnabled(GL_DEPTH_TEST);
  GLint prior_matrix_mode=GL_MODELVIEW;
  glGetIntegerv(GL_MATRIX_MODE,&prior_matrix_mode);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadMatrixf(obs->projection_matrix.elements.data());
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadMatrixf(obs->view_matrix.elements.data());
  glBegin(GL_TRIANGLES);
  glColor3f(1.0f,0.2f,0.1f);glVertex3f(-2.0f,-2.0f,0.0f);
  glColor3f(0.1f,1.0f,0.2f);glVertex3f(2.0f,-2.0f,0.0f);
  glColor3f(0.2f,0.3f,1.0f);glVertex3f(0.0f,2.0f,0.0f);
  glEnd();
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(static_cast<GLenum>(prior_matrix_mode));
  if(!prior_depth)glDisable(GL_DEPTH_TEST);
  gpu_check(x.cameras->end_update(init->primary.camera,f),"end_update succeeded after 3D scene draw");

  std::vector<unsigned char> rgba(64*48*4);std::vector<float> depths(64*48);
  gpu_check(x.cameras->begin_update(init->primary.camera,f),"rebind 180ED0 FBO for diagnostic readback");
  glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
  a->record_diagnostic_readback(2);
  gpu_check(x.cameras->end_update(init->primary.camera,f),"end diagnostic readback pass");
  unsigned lit_pixels=0;
  for(unsigned y=6;y<42;++y)for(unsigned xx=8;xx<56;++xx){
   const std::size_t idx=(y*64+xx);
   const unsigned r=rgba[idx*4+0],g=rgba[idx*4+1];
   if((r>100||g>100)&&depths[idx]<0.99f)++lit_pixels;
  }
  gpu_check(lit_pixels>100,"3D triangle rendered through 180ED0 synchronized view/projection produced measurable color & depth output");

  // World-to-camera scene render pipeline with frustum culling and GL state restoration.
  gpu_check(renderer_required::construct_core_plugin_0412(*x.core,x.native.f),"GPU actual0412 constructor");
  auto world_mod=x.core->world_module(x.native.f);
  RetailRendererWorldFailure rwf;
  auto world_h=world_mod?world_mod->create_world(rwf):std::optional<NativeRetailWorldHandle>{};
  auto world_ptr=world_h?world_mod->resolve_world(*world_h,rwf):std::shared_ptr<NativeRetailWorld>{};
  gpu_check(bool(world_ptr),"create native RetailWorld for scene render");
  gpu_check(attach_camera_to_world_1cf300(*x.cameras,*frames,init->primary.camera,init->primary.world_link,world_ptr,pf),
            "1CF300 attach camera to NativeRetailWorld");

  auto in_frame=frames->create_base_frame(ff);
  auto out_frame=frames->create_base_frame(ff);
  gpu_check(in_frame&&out_frame,"create object frames for world scene render");
  RetailRendererMatrixValue in_mod{{{1,0,0},{0,1,0},{0,0,1},{0,0,0}},0x20003u};
  RetailRendererMatrixValue out_mod{{{1,0,0},{0,1,0},{0,0,1},{100.0f,0,0}},0x20000u};
  gpu_check(frames->set_modelling(*in_frame,in_mod,ff)&&frames->set_modelling(*out_frame,out_mod,ff),
            "set object modelling matrices (1 inside frustum, 1 culled)");

  RetailWorldSceneObject obj_in;
  obj_in.id=1;obj_in.frame=*in_frame;obj_in.local_center={0,0,0};obj_in.bounding_radius=2.5f;
  obj_in.triangles={
   {{-1.5f,-1.5f,0.0f},0.0f,0.0f,{0.1f,1.0f,0.2f,1.0f}},
   {{ 1.5f,-1.5f,0.0f},0.0f,0.0f,{0.1f,1.0f,0.2f,1.0f}},
   {{ 0.0f, 1.5f,0.0f},0.0f,0.0f,{0.1f,1.0f,0.2f,1.0f}}
  };
  RetailWorldSceneObject obj_out;
  obj_out.id=2;obj_out.frame=*out_frame;obj_out.local_center={0,0,0};obj_out.bounding_radius=2.0f;
  obj_out.triangles={
   {{-1.5f,-1.5f,0.0f},0.0f,0.0f,{1.0f,0.05f,0.05f,1.0f}},
   {{ 1.5f,-1.5f,0.0f},0.0f,0.0f,{1.0f,0.05f,0.05f,1.0f}},
   {{ 0.0f, 1.5f,0.0f},0.0f,0.0f,{1.0f,0.05f,0.05f,1.0f}}
  };
  gpu_check(world_ptr->add_scene_object(std::move(obj_in),rwf)&&
            world_ptr->add_scene_object(std::move(obj_out),rwf),
            "add visible and culled objects to NativeRetailWorld");

  glMatrixMode(GL_PROJECTION);
  const GLboolean saved_depth=glIsEnabled(GL_DEPTH_TEST);
  GLint saved_depth_func=GL_LESS,saved_matrix_mode=GL_MODELVIEW,saved_mv_depth=0;
  glGetIntegerv(GL_DEPTH_FUNC,&saved_depth_func);
  glGetIntegerv(GL_MATRIX_MODE,&saved_matrix_mode);
  glGetIntegerv(GL_MODELVIEW_STACK_DEPTH,&saved_mv_depth);

  auto stats=render_world_scene_frame(*x.cameras,*frames,init->primary.camera,init->primary.world_link,
   world_ptr,{0.05f,0.05f,0.1f,1.0f},3u,true,pf);
  gpu_check(stats.has_value()&&stats->total_objects==2&&stats->visible_objects==1&&
            stats->culled_objects==1&&stats->submitted_triangles==1,
            "world scene render culled outside object and submitted inside object");
  GLint after_depth_func=0,after_matrix_mode=0,after_mv_depth=0;
  glGetIntegerv(GL_DEPTH_FUNC,&after_depth_func);
  glGetIntegerv(GL_MATRIX_MODE,&after_matrix_mode);
  glGetIntegerv(GL_MODELVIEW_STACK_DEPTH,&after_mv_depth);
  gpu_check(glIsEnabled(GL_DEPTH_TEST)==saved_depth&&after_depth_func==saved_depth_func&&
            after_matrix_mode==saved_matrix_mode&&after_mv_depth==saved_mv_depth,
            "render_world_scene_frame restored host GL state");

  gpu_check(x.cameras->begin_update(init->primary.camera,f),"rebind FBO for world scene readback");
  glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
  a->record_diagnostic_readback(2);
  gpu_check(x.cameras->end_update(init->primary.camera,f),"end world scene readback pass");
  unsigned green_pixels=0,red_pixels=0;
  for(unsigned y=6;y<42;++y)for(unsigned xx=8;xx<56;++xx){
   const std::size_t idx=(y*64+xx);
   const unsigned r=rgba[idx*4+0],g=rgba[idx*4+1];
   if(g>150&&r<80&&depths[idx]<0.99f)++green_pixels;
   if(r>150&&g<80)++red_pixels;
  }
  gpu_check(green_pixels>80&&red_pixels==0,"world scene rendered visible green object and culled red object");

  gpu_check(detach_camera_from_world_1cf350(init->primary.world_link,world_ptr,wf),"1CF350 detach camera from world");
  gpu_check(!render_world_scene_frame(*x.cameras,*frames,init->primary.camera,init->primary.world_link,
   world_ptr,{0.05f,0.05f,0.1f,1.0f},3u,false,pf),"detached camera refused by render_world_scene_frame");

  // Retail RenderWare geometry stream ingestion + 0408/040a material & texture binding + GL cleanup.
  gpu_check(renderer_required::construct_core_plugin_0408(*x.core,x.native.f)&&
            renderer_required::construct_core_plugin_040a(*x.core,x.native.f),"GPU actual 0408/040a constructors");
  auto tex_mod=x.core->textures(x.native.f);
  auto mat_mod=x.core->materials(x.native.f);
  gpu_check(tex_mod&&mat_mod,"0408 texture and 040a material modules available");

  RetailRendererRasterFailure rrf;
  auto tex_raster=x.rasters->create_raster({2,2,32,4,0x0500,0x0500},rrf);
  gpu_check(bool(tex_raster),"create 2x2 RGBA8 texture raster");
  {
   auto px=x.rasters->lock_pixels(*tex_raster,rrf);
   gpu_check(px&&px->size()>=16,"lock 2x2 texture pixels");
   for(std::size_t i=0;i<4;++i){
    (*px)[i*4+0]=std::byte{0};
    (*px)[i*4+1]=std::byte{24};
    (*px)[i*4+2]=std::byte{255};
    (*px)[i*4+3]=std::byte{255};
   }
   gpu_check(x.rasters->unlock_pixels(*tex_raster,rrf),"unlock 2x2 texture pixels");
  }
  RetailRendererTextureFailure rtf;
  auto tex_h=tex_mod->create_texture(
   {"test_blue","",RetailRendererTextureFilter::nearest,RetailRendererTextureAddress::clamp,RetailRendererTextureAddress::clamp},
   *tex_raster,rtf);
  gpu_check(bool(tex_h),"create 0408 native texture bound to raster");

  // Build fail-closed test streams + valid RW Clump -> GeometryList -> Geometry -> MatList -> Material -> Texture.
  auto push_u16=[](std::vector<std::byte>& buf,std::uint16_t v){
   const auto* p=reinterpret_cast<const std::byte*>(&v);buf.insert(buf.end(),p,p+2);
  };
  auto push_u32=[](std::vector<std::byte>& buf,std::uint32_t v){
   const auto* p=reinterpret_cast<const std::byte*>(&v);buf.insert(buf.end(),p,p+4);
  };
  auto push_f32=[](std::vector<std::byte>& buf,float v){
   const auto* p=reinterpret_cast<const std::byte*>(&v);buf.insert(buf.end(),p,p+4);
  };
  auto wrap_chunk=[&](std::uint32_t type,const std::vector<std::byte>& payload){
   std::vector<std::byte> out;
   push_u32(out,type);push_u32(out,static_cast<std::uint32_t>(payload.size()));push_u32(out,0x1803FFFFu);
   out.insert(out.end(),payload.begin(),payload.end());
   return out;
  };

  // Truncated stream must fail closed.
  std::vector<std::byte> bad_stream;
  push_u32(bad_stream,0x000Fu);push_u32(bad_stream,9999u);push_u32(bad_stream,0x1803FFFFu);
  gpu_check(!ingest_rw_geometry_stream(bad_stream,500u,*frames,*mat_mod,*tex_mod,*world_ptr,{},rwf),
            "truncated RW stream rejected fail-closed");

  // Out-of-bounds vertex index stream must fail closed.
  std::vector<std::byte> oob_st;
  push_u32(oob_st,0x00u);push_u32(oob_st,1u);push_u32(oob_st,3u);push_u32(oob_st,1u);
  push_u16(oob_st,1u);push_u16(oob_st,99u);push_u16(oob_st,0u);push_u16(oob_st,2u);
  push_f32(oob_st,0.0f);push_f32(oob_st,0.0f);push_f32(oob_st,0.0f);push_f32(oob_st,1.0f);
  push_u32(oob_st,1u);push_u32(oob_st,0u);
  for(int i=0;i<9;++i)push_f32(oob_st,0.0f);
  auto oob_geom=wrap_chunk(0x000Fu,wrap_chunk(0x0001u,oob_st));
  gpu_check(!ingest_rw_geometry_stream(oob_geom,501u,*frames,*mat_mod,*tex_mod,*world_ptr,{},rwf),
            "out-of-bounds RW vertex index rejected fail-closed");

  // Build valid RW Geometry (1 blue-textured triangle in front of camera at z=-0.5).
  std::vector<std::byte> tex_str;
  for(char ch:std::string_view("test_blue"))tex_str.push_back(static_cast<std::byte>(ch));
  tex_str.push_back(std::byte{0});
  std::vector<std::byte> tex_st_p;push_u32(tex_st_p,0x00000102u);
  auto tex_payload=wrap_chunk(0x0001u,tex_st_p);
  auto tex_name_chunk=wrap_chunk(0x0002u,tex_str);
  tex_payload.insert(tex_payload.end(),tex_name_chunk.begin(),tex_name_chunk.end());
  auto rw_tex=wrap_chunk(0x0006u,tex_payload);

  std::vector<std::byte> mat_st_p;
  push_u32(mat_st_p,0u);
  mat_st_p.push_back(std::byte{255});mat_st_p.push_back(std::byte{255});
  mat_st_p.push_back(std::byte{255});mat_st_p.push_back(std::byte{255});
  push_u32(mat_st_p,0u);push_u32(mat_st_p,1u);
  push_f32(mat_st_p,1.0f);push_f32(mat_st_p,1.0f);push_f32(mat_st_p,1.0f);
  auto mat_payload=wrap_chunk(0x0001u,mat_st_p);
  mat_payload.insert(mat_payload.end(),rw_tex.begin(),rw_tex.end());
  auto rw_mat=wrap_chunk(0x0007u,mat_payload);

  std::vector<std::byte> ml_st_p;push_u32(ml_st_p,1u);push_u32(ml_st_p,0xFFFFFFFFu);
  auto ml_payload=wrap_chunk(0x0001u,ml_st_p);
  ml_payload.insert(ml_payload.end(),rw_mat.begin(),rw_mat.end());
  auto rw_matlist=wrap_chunk(0x0008u,ml_payload);

  std::vector<std::byte> geom_st_p;
  push_u32(geom_st_p,0x04u);push_u32(geom_st_p,1u);push_u32(geom_st_p,3u);push_u32(geom_st_p,1u);
  push_f32(geom_st_p,0.0f);push_f32(geom_st_p,0.0f);
  push_f32(geom_st_p,1.0f);push_f32(geom_st_p,0.0f);
  push_f32(geom_st_p,0.5f);push_f32(geom_st_p,1.0f);
  push_u16(geom_st_p,1u);push_u16(geom_st_p,0u);push_u16(geom_st_p,0u);push_u16(geom_st_p,2u);
  push_f32(geom_st_p,0.0f);push_f32(geom_st_p,0.0f);push_f32(geom_st_p,-0.5f);push_f32(geom_st_p,2.0f);
  push_u32(geom_st_p,1u);push_u32(geom_st_p,0u);
  push_f32(geom_st_p,-1.2f);push_f32(geom_st_p,-1.2f);push_f32(geom_st_p,-0.5f);
  push_f32(geom_st_p, 1.2f);push_f32(geom_st_p,-1.2f);push_f32(geom_st_p,-0.5f);
  push_f32(geom_st_p, 0.0f);push_f32(geom_st_p, 1.2f);push_f32(geom_st_p,-0.5f);
  auto geom_payload=wrap_chunk(0x0001u,geom_st_p);
  geom_payload.insert(geom_payload.end(),rw_matlist.begin(),rw_matlist.end());
  auto rw_geom=wrap_chunk(0x000Fu,geom_payload);
  std::vector<std::byte> gl_st_p;push_u32(gl_st_p,1u);
  auto gl_payload=wrap_chunk(0x0001u,gl_st_p);
  gl_payload.insert(gl_payload.end(),rw_geom.begin(),rw_geom.end());
  auto rw_clump=wrap_chunk(0x0010u,wrap_chunk(0x001Au,gl_payload));

  gpu_check(ingest_rw_geometry_stream(rw_clump,100u,*frames,*mat_mod,*tex_mod,*world_ptr,{},rwf),
            "ingest valid RW clump geometry with material/texture");
  gpu_check(attach_camera_to_world_1cf300(*x.cameras,*frames,init->primary.camera,init->primary.world_link,world_ptr,pf),
            "reattach camera to world for textured render");
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,0);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  auto tex_stats=render_world_scene_frame(*x.cameras,*frames,init->primary.camera,init->primary.world_link,
   world_ptr,{0.05f,0.05f,0.1f,1.0f},3u,true,pf,mat_mod.get(),tex_mod.get(),x.rasters.get());
  gpu_check(tex_stats.has_value()&&tex_stats->visible_objects==2&&tex_stats->culled_objects==1&&
            tex_stats->bound_textures>=1,"textured world scene rendered visible + ingested objects and bound texture");
  GLint after_tex_binding=-1,after_tex_env=0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D,&after_tex_binding);
  glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&after_tex_env);
  gpu_check(!glIsEnabled(GL_TEXTURE_2D)&&after_tex_binding==0&&after_tex_env==GL_REPLACE,
            "render_world_scene_frame restored GL_TEXTURE_2D, binding, and GL_TEXTURE_ENV_MODE");

  gpu_check(x.cameras->begin_update(init->primary.camera,f),"rebind FBO for textured readback");
  glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
  a->record_diagnostic_readback(2);
  gpu_check(x.cameras->end_update(init->primary.camera,f),"end textured readback pass");
  unsigned blue_pixels=0;
  for(unsigned y=6;y<42;++y)for(unsigned xx=8;xx<56;++xx){
   const std::size_t idx=(y*64+xx);
   const unsigned r=rgba[idx*4+0],g=rgba[idx*4+1],b=rgba[idx*4+2];
   if(b>200&&r<40&&g<60&&depths[idx]<0.99f)++blue_pixels;
  }
  gpu_check(blue_pixels>60,"ingested RW textured triangle produced measurable blue/depth pixels");

  gpu_check(!world_ptr->gpu_texture_cache().empty(),"world cached uploaded GL texture handle");
  const GLuint cached_tex=static_cast<GLuint>(world_ptr->gpu_texture_cache()[0].gl_texture_id);
  gpu_check(glIsTexture(cached_tex)==GL_TRUE,"cached GL texture handle is live before teardown");
  world_ptr->release_gpu_resources();
  gpu_check(glIsTexture(cached_tex)==GL_FALSE&&world_ptr->gpu_texture_cache().empty(),
            "release_gpu_resources deleted GL texture handle with zero leaks");

  // TXD (0x0016/0x0015) + RpWorld BSP (0x000B/0x000A/0x0009) ingestion and sector-level culling.
  std::vector<std::byte> bad_txd;
  push_u32(bad_txd,0x0016u);push_u32(bad_txd,8u);push_u32(bad_txd,0x1803FFFFu);
  std::vector<std::byte> bad_txd_st;push_u16(bad_txd_st,1u);push_u16(bad_txd_st,0u);
  auto bad_txd_hdr=wrap_chunk(0x0001u,bad_txd_st);
  // Claim 100x100 pixels in a 90-byte struct to trigger bounds check failure.
  std::vector<std::byte> bad_tn(90u,std::byte{0});
  std::uint16_t bw=100,bh=100;
  std::memcpy(bad_tn.data()+80,&bw,2);std::memcpy(bad_tn.data()+82,&bh,2);
  bad_tn[84]=std::byte{32};
  auto bad_tn_chunk=wrap_chunk(0x0015u,wrap_chunk(0x0001u,bad_tn));
  std::vector<std::byte> bad_txd_body;
  bad_txd_body.insert(bad_txd_body.end(),bad_txd_hdr.begin(),bad_txd_hdr.end());
  bad_txd_body.insert(bad_txd_body.end(),bad_tn_chunk.begin(),bad_tn_chunk.end());
  auto full_bad_txd=wrap_chunk(0x0016u,bad_txd_body);
  const auto tex_count_before=tex_mod->active_count();
  gpu_check(!ingest_rw_tex_dictionary_stream(full_bad_txd,*x.rasters,*tex_mod,rtf)&&
            tex_mod->active_count()==tex_count_before,
            "malformed TXD rejected fail-closed with zero leaked textures");

  // Valid TXD with 2x2 gold texture "bsp_gold".
  std::vector<std::byte> tn_p(88u+16u,std::byte{0});
  std::uint32_t filter_addr=0x00003301u; // nearest + clamp/clamp
  std::memcpy(tn_p.data()+4,&filter_addr,4);
  std::memcpy(tn_p.data()+8,"bsp_gold",8);
  std::uint16_t gw=2,gh=2;
  std::memcpy(tn_p.data()+80,&gw,2);std::memcpy(tn_p.data()+82,&gh,2);
  tn_p[84]=std::byte{32};
  for(std::size_t i=0;i<4;++i){
   tn_p[88+i*4+0]=std::byte{255};
   tn_p[88+i*4+1]=std::byte{220};
   tn_p[88+i*4+2]=std::byte{0};
   tn_p[88+i*4+3]=std::byte{255};
  }
  auto txd_body=bad_txd_hdr;
  auto tn_chunk=wrap_chunk(0x0015u,wrap_chunk(0x0001u,tn_p));
  txd_body.insert(txd_body.end(),tn_chunk.begin(),tn_chunk.end());
  auto valid_txd=wrap_chunk(0x0016u,txd_body);
  gpu_check(ingest_rw_tex_dictionary_stream(valid_txd,*x.rasters,*tex_mod,rtf),
            "ingest valid TXD texture dictionary");
  auto gold_found=tex_mod->find_texture("bsp_gold",rtf);
  gpu_check(gold_found&&!gold_found->empty(),"TXD registered named texture bsp_gold");

  // Malformed RpWorld BSP stream rejected fail-closed.
  std::vector<std::byte> bad_world_st(52u,std::byte{0});
  auto bad_world=wrap_chunk(0x000Bu,wrap_chunk(0x0001u,bad_world_st));
  gpu_check(!ingest_rw_world_bsp_stream(bad_world,1000u,*mat_mod,*tex_mod,*world_ptr,rwf)&&
            world_ptr->bsp_sectors().empty(),"empty/malformed RpWorld BSP rejected fail-closed");

  // Valid RpWorld BSP: 1 PlaneSect root -> 2 AtomicSect leaves (1 visible gold, 1 culled at x=120).
  std::vector<std::byte> bsp_tex_str;
  for(char ch:std::string_view("bsp_gold"))bsp_tex_str.push_back(static_cast<std::byte>(ch));
  bsp_tex_str.push_back(std::byte{0});
  auto bsp_tex_p=wrap_chunk(0x0001u,tex_st_p);
  auto bsp_tex_nc=wrap_chunk(0x0002u,bsp_tex_str);
  bsp_tex_p.insert(bsp_tex_p.end(),bsp_tex_nc.begin(),bsp_tex_nc.end());
  auto bsp_rw_tex=wrap_chunk(0x0006u,bsp_tex_p);
  auto bsp_mat_p=wrap_chunk(0x0001u,mat_st_p);
  bsp_mat_p.insert(bsp_mat_p.end(),bsp_rw_tex.begin(),bsp_rw_tex.end());
  auto bsp_rw_mat=wrap_chunk(0x0007u,bsp_mat_p);
  auto bsp_ml_p=wrap_chunk(0x0001u,ml_st_p);
  bsp_ml_p.insert(bsp_ml_p.end(),bsp_rw_mat.begin(),bsp_rw_mat.end());
  auto bsp_matlist=wrap_chunk(0x0008u,bsp_ml_p);

  auto make_atomic_sect=[&](float ox,float oz){
   std::vector<std::byte> as;
   push_u32(as,0u);push_u32(as,1u);push_u32(as,3u);
   push_f32(as,ox-1.5f);push_f32(as,-1.5f);push_f32(as,oz-0.5f);
   push_f32(as,ox+1.5f);push_f32(as, 1.5f);push_f32(as,oz+0.5f);
   push_u32(as,0u);push_u32(as,0u);
   push_f32(as,ox-1.2f);push_f32(as,-1.2f);push_f32(as,oz);
   push_f32(as,ox+1.2f);push_f32(as,-1.2f);push_f32(as,oz);
   push_f32(as,ox+0.0f);push_f32(as, 1.2f);push_f32(as,oz);
   push_f32(as,0.0f);push_f32(as,0.0f);
   push_f32(as,1.0f);push_f32(as,0.0f);
   push_f32(as,0.5f);push_f32(as,1.0f);
   push_u16(as,0u);push_u16(as,0u);push_u16(as,1u);push_u16(as,2u);
   return wrap_chunk(0x0009u,wrap_chunk(0x0001u,as));
  };
  std::vector<std::byte> ps;
  push_u32(ps,0u);push_f32(ps,50.0f);push_u32(ps,1u);push_u32(ps,1u);push_f32(ps,0.0f);push_f32(ps,120.0f);
  auto plane_body=wrap_chunk(0x0001u,ps);
  auto left_sec=make_atomic_sect(0.0f,-0.25f);
  auto right_sec=make_atomic_sect(120.0f,-0.25f);
  plane_body.insert(plane_body.end(),left_sec.begin(),left_sec.end());
  plane_body.insert(plane_body.end(),right_sec.begin(),right_sec.end());
  auto root_plane=wrap_chunk(0x000Au,plane_body);

  std::vector<std::byte> ws_p;
  push_u32(ws_p,1u);
  push_f32(ws_p,-2.0f);push_f32(ws_p,-2.0f);push_f32(ws_p,-1.0f);
  push_f32(ws_p,125.0f);push_f32(ws_p,2.0f);push_f32(ws_p,1.0f);
  push_u32(ws_p,2u);push_u32(ws_p,6u);push_u32(ws_p,1u);push_u32(ws_p,2u);
  push_u32(ws_p,0u);push_u32(ws_p,0x04u);
  auto world_body=wrap_chunk(0x0001u,ws_p);
  world_body.insert(world_body.end(),bsp_matlist.begin(),bsp_matlist.end());
  world_body.insert(world_body.end(),root_plane.begin(),root_plane.end());
  auto valid_world=wrap_chunk(0x000Bu,world_body);

  world_ptr->clear_scene_objects();
  gpu_check(ingest_rw_world_bsp_stream(valid_world,1000u,*mat_mod,*tex_mod,*world_ptr,rwf),
            "ingest valid RpWorld BSP stream");
  auto bsp_stats=render_world_scene_frame(*x.cameras,*frames,init->primary.camera,init->primary.world_link,
   world_ptr,{0.05f,0.05f,0.1f,1.0f},3u,true,pf,mat_mod.get(),tex_mod.get(),x.rasters.get());
  gpu_check(bsp_stats.has_value()&&bsp_stats->total_sectors==2&&bsp_stats->visible_sectors==1&&
            bsp_stats->culled_sectors==1&&bsp_stats->submitted_triangles==1&&bsp_stats->bound_textures>=1,
            "BSP world render culled outside sector and rendered visible gold-textured sector");
  gpu_check(x.cameras->begin_update(init->primary.camera,f),"rebind FBO for BSP readback");
  glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
  a->record_diagnostic_readback(2);
  gpu_check(x.cameras->end_update(init->primary.camera,f),"end BSP readback pass");
  unsigned gold_pixels=0;
  for(unsigned y=6;y<42;++y)for(unsigned xx=8;xx<56;++xx){
   const std::size_t idx=(y*64+xx);
   const unsigned r=rgba[idx*4+0],g=rgba[idx*4+1],b=rgba[idx*4+2];
   if(r>200&&g>170&&b<50&&depths[idx]<0.99f)++gold_pixels;
  }
  gpu_check(gold_pixels>60,"BSP visible sector rendered measurable gold-textured pixels");
  gpu_check(!world_ptr->gpu_texture_cache().empty(),"BSP cached uploaded GL texture handle");
  const GLuint bsp_tex=static_cast<GLuint>(world_ptr->gpu_texture_cache()[0].gl_texture_id);
  world_ptr->release_gpu_resources();
  gpu_check(glIsTexture(bsp_tex)==GL_FALSE&&world_ptr->gpu_texture_cache().empty(),
            "BSP release_gpu_resources deleted GL texture handle");
  world_ptr->clear_bsp_sectors();

  // PS2 PAL4 + PAL8 (CSM1 CLUT unswizzle + GS alpha 0x80->0xFF) + fail-closed archive selector.
  {
   RetailRendererWorldFailure arch_f;
   gpu_check(!load_retail_scene_streams_from_volume(
    x.native.startup.content,"missing_scene.txd","",2000u,*x.rasters,*tex_mod,*mat_mod,*world_ptr,arch_f),
    "archive stream selector rejects missing TXD resource fail-closed");

   // Malformed truncated PAL4 TXD must fail closed with zero leaks.
   std::vector<std::byte> bad_pal4(88u+10u,std::byte{0});
   std::memcpy(bad_pal4.data()+8,"bad_pal4",8);
   std::uint32_t p4_fmt=0x4500u;std::memcpy(bad_pal4.data()+72,&p4_fmt,4);
   std::uint16_t p4_w=4,p4_h=4;std::memcpy(bad_pal4.data()+80,&p4_w,2);std::memcpy(bad_pal4.data()+82,&p4_h,2);
   bad_pal4[84]=std::byte{4};
   auto bad_pal4_txd=wrap_chunk(0x0016u,[&]{
    auto b=bad_txd_hdr;auto tn=wrap_chunk(0x0015u,wrap_chunk(0x0001u,bad_pal4));
    b.insert(b.end(),tn.begin(),tn.end());return b;
   }());
   const auto before_pal_cnt=tex_mod->active_count();
   gpu_check(!ingest_rw_tex_dictionary_stream(bad_pal4_txd,*x.rasters,*tex_mod,rtf)&&
             tex_mod->active_count()==before_pal_cnt,
             "truncated PAL4 palette/pixel TXD rejected fail-closed");

   // Valid PAL4 ("pal4_cyan": 2x2, pal[3]=cyan GS alpha 0x80->0xFF, packed 0x03 so low nibble=3)
   // + Valid PAL8 ("pal8_magenta": 2x2, CSM1 raw[18]=magenta GS alpha 0x80->0xFF for logical idx 10).
   std::vector<std::byte> p4_tn(88u+64u+2u,std::byte{0});
   std::uint32_t fa_nearest=0x00003301u;std::memcpy(p4_tn.data()+4,&fa_nearest,4);
   std::memcpy(p4_tn.data()+8,"pal4_cyan",9);
   std::memcpy(p4_tn.data()+72,&p4_fmt,4);
   std::uint16_t tw=2,th=2;std::memcpy(p4_tn.data()+80,&tw,2);std::memcpy(p4_tn.data()+82,&th,2);
   p4_tn[84]=std::byte{4};
   p4_tn[88u+3u*4u+0u]=std::byte{0};p4_tn[88u+3u*4u+1u]=std::byte{240};
   p4_tn[88u+3u*4u+2u]=std::byte{255};p4_tn[88u+3u*4u+3u]=std::byte{0x80};
   p4_tn[88u+64u+0u]=std::byte{0x03};p4_tn[88u+64u+1u]=std::byte{0x03};

   std::vector<std::byte> p8_tn(88u+1024u+4u,std::byte{0});
   std::memcpy(p8_tn.data()+4,&fa_nearest,4);
   std::memcpy(p8_tn.data()+8,"pal8_magenta",12);
   std::uint32_t p8_fmt=0x2500u;std::memcpy(p8_tn.data()+72,&p8_fmt,4);
   std::memcpy(p8_tn.data()+80,&tw,2);std::memcpy(p8_tn.data()+82,&th,2);
   p8_tn[84]=std::byte{8};
   // Logical index 10 (0x0A) maps to raw CSM1 slot 18 (0x12) via 10 ^ 0x18 = 18.
   p8_tn[88u+18u*4u+0u]=std::byte{255};p8_tn[88u+18u*4u+1u]=std::byte{0};
   p8_tn[88u+18u*4u+2u]=std::byte{220};p8_tn[88u+18u*4u+3u]=std::byte{0x80};
   for(std::size_t i=0;i<4;++i)p8_tn[88u+1024u+i]=std::byte{10};

   std::vector<std::byte> txd2_st;push_u16(txd2_st,2u);push_u16(txd2_st,0u);
   auto txd2_body=wrap_chunk(0x0001u,txd2_st);
   auto c_p4=wrap_chunk(0x0015u,wrap_chunk(0x0001u,p4_tn));
   auto c_p8=wrap_chunk(0x0015u,wrap_chunk(0x0001u,p8_tn));
   txd2_body.insert(txd2_body.end(),c_p4.begin(),c_p4.end());
   txd2_body.insert(txd2_body.end(),c_p8.begin(),c_p8.end());
   auto pal_txd=wrap_chunk(0x0016u,txd2_body);

   gpu_check(ingest_rw_tex_dictionary_stream(pal_txd,*x.rasters,*tex_mod,rtf),
             "ingest PAL4 + PAL8 CSM1 TXD dictionary");
   auto h_p4=tex_mod->find_texture("pal4_cyan",rtf);
   auto h_p8=tex_mod->find_texture("pal8_magenta",rtf);
   gpu_check(h_p4&&!h_p4->empty()&&h_p8&&!h_p8->empty(),"PAL4 and PAL8 named textures registered");
   auto r_p4=tex_mod->raster(*h_p4,rtf);auto r_p8=tex_mod->raster(*h_p8,rtf);
   gpu_check(r_p4&&r_p8,"PAL4 and PAL8 rasters accessible");
   {
    auto px4=x.rasters->lock_pixels(*r_p4,rrf);
    gpu_check(px4&&px4->size()>=16&&std::to_integer<unsigned>((*px4)[0])==0u&&
              std::to_integer<unsigned>((*px4)[1])==240u&&
              std::to_integer<unsigned>((*px4)[2])==255u&&
              std::to_integer<unsigned>((*px4)[3])==255u,
              "PAL4 pixel 0 decoded low nibble with GS alpha 0x80->0xFF");
    (void)x.rasters->unlock_pixels(*r_p4,rrf);
    auto px8=x.rasters->lock_pixels(*r_p8,rrf);
    gpu_check(px8&&px8->size()>=16&&std::to_integer<unsigned>((*px8)[0])==255u&&
              std::to_integer<unsigned>((*px8)[1])==0u&&
              std::to_integer<unsigned>((*px8)[2])==220u&&
              std::to_integer<unsigned>((*px8)[3])==255u,
              "PAL8 pixel 0 decoded CSM1 CLUT unswizzle + GS alpha 0x80->0xFF");
    (void)x.rasters->unlock_pixels(*r_p8,rrf);
   }

   // Render PAL8 magenta onto RpWorld BSP sector on GPU.
   std::vector<std::byte> m_str;
   for(char ch:std::string_view("pal8_magenta"))m_str.push_back(static_cast<std::byte>(ch));
   m_str.push_back(std::byte{0});
   auto m_tp=wrap_chunk(0x0001u,tex_st_p);
   auto m_nc=wrap_chunk(0x0002u,m_str);
   m_tp.insert(m_tp.end(),m_nc.begin(),m_nc.end());
   auto m_rwt=wrap_chunk(0x0006u,m_tp);
   auto m_mp=wrap_chunk(0x0001u,mat_st_p);
   m_mp.insert(m_mp.end(),m_rwt.begin(),m_rwt.end());
   auto m_rwm=wrap_chunk(0x0007u,m_mp);
   auto m_mlp=wrap_chunk(0x0001u,ml_st_p);
   m_mlp.insert(m_mlp.end(),m_rwm.begin(),m_rwm.end());
   auto m_matlist=wrap_chunk(0x0008u,m_mlp);

   std::vector<std::byte> m_world_body=wrap_chunk(0x0001u,ws_p);
   m_world_body.insert(m_world_body.end(),m_matlist.begin(),m_matlist.end());
   m_world_body.insert(m_world_body.end(),root_plane.begin(),root_plane.end());
   auto m_world=wrap_chunk(0x000Bu,m_world_body);

   gpu_check(ingest_rw_world_bsp_stream(m_world,2000u,*mat_mod,*tex_mod,*world_ptr,rwf),
             "ingest RpWorld BSP referencing PAL8 texture");
   auto m_stats=render_world_scene_frame(*x.cameras,*frames,init->primary.camera,init->primary.world_link,
    world_ptr,{0.05f,0.05f,0.1f,1.0f},3u,true,pf,mat_mod.get(),tex_mod.get(),x.rasters.get());
   gpu_check(m_stats.has_value()&&m_stats->visible_sectors==1&&m_stats->culled_sectors==1&&m_stats->bound_textures>=1,
             "PAL8 BSP sector rendered with culling and texture bind");
   gpu_check(x.cameras->begin_update(init->primary.camera,f),"rebind FBO for PAL8 readback");
   glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
   glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
   a->record_diagnostic_readback(2);
   gpu_check(x.cameras->end_update(init->primary.camera,f),"end PAL8 readback pass");
   unsigned magenta_pixels=0;
   for(unsigned y=6;y<42;++y)for(unsigned xx=8;xx<56;++xx){
    const std::size_t idx=(y*64+xx);
    const unsigned r=rgba[idx*4+0],g=rgba[idx*4+1],b=rgba[idx*4+2];
    if(r>200&&g<50&&b>170&&depths[idx]<0.99f)++magenta_pixels;
   }
   gpu_check(magenta_pixels>60,"PAL8 CSM1-unswizzled BSP sector rendered measurable magenta pixels");
   const GLuint m_tex_id=static_cast<GLuint>(world_ptr->gpu_texture_cache()[0].gl_texture_id);
   world_ptr->release_gpu_resources();
   gpu_check(glIsTexture(m_tex_id)==GL_FALSE,"PAL8 release_gpu_resources deleted GL texture handle");
   world_ptr->clear_bsp_sectors();
  }

  gpu_check(detach_camera_from_world_1cf350(init->primary.world_link,world_ptr,wf),"final 1CF350 detach");
  world_ptr->clear_scene_objects();
  gpu_check(frames->destroy_leaf(*out_frame,ff)&&frames->destroy_leaf(*in_frame,ff),"destroy world scene object frames");
  gpu_check(world_mod->destroy_world(*world_h,rwf),"destroy NativeRetailWorld");
  renderer_required::destroy_core_plugin_040a(*x.core);
  renderer_required::destroy_core_plugin_0408(*x.core);
  renderer_required::destroy_core_plugin_0412(*x.core);

  destroy_camera_producer_17ebb0(*x.cameras,*frames,*x.rasters,init->primary);
  renderer_required::destroy_core_plugin_0403(*x.core);
  gpu_check(glGetError()==GL_NO_ERROR,"180ED0 GPU scene render case ends without GL errors");
 }
 {
  ++gpu_scenarios;CameraGpuFixture x(root);auto a=native_display();RetailRendererCameraFailure f;RetailRendererRasterFailure rf;
  const auto color=x.backed(2,a),depth=x.backed(1,a);const auto h=x.camera(color,depth);
  bool wrong_thread=false;std::thread thread([&]{RetailRendererCameraFailure q;
   wrong_thread=!x.cameras->begin_update(h,q)&&q.kind==RetailRendererCameraError::wrong_thread;});thread.join();
  gpu_check(wrong_thread,"camera target refuses foreign thread before any GPU operation");
  const auto live_texture=x.raster(5),unbacked=x.raster(2),stale=x.raster(2);
  gpu_check(!x.cameras->set_frame_buffer_raster(h,depth,f)&&f.kind==RetailRendererCameraError::raster_binding,
            "live depth raster rejected from color role");
  gpu_check(!x.cameras->set_z_buffer_raster(h,color,f)&&f.kind==RetailRendererCameraError::raster_binding,
            "live color raster rejected from depth role");
  CameraGpuFixture foreign(root);const auto foreign_color=foreign.raster(2),foreign_depth=foreign.raster(1);
  gpu_check(!x.cameras->set_frame_buffer_raster(h,foreign_color,f)&&f.kind==RetailRendererCameraError::raster_binding&&
            !x.cameras->set_z_buffer_raster(h,foreign_depth,f)&&f.kind==RetailRendererCameraError::raster_binding,
            "both raster fields reject actual foreign Session owners");
  auto observed_color=x.cameras->frame_buffer_raster(h,f);auto observed_depth=x.cameras->z_buffer_raster(h,f);
  gpu_check(observed_color&&observed_color->same_identity(color)&&observed_depth&&observed_depth->same_identity(depth),
            "refused role/Session setters preserve prior raster identities");
  const auto zero_color=x.backed(2,a,0,448),zero_depth=x.backed(1,a,640,0);
  glViewport(2,3,9,10);
  gpu_check(x.cameras->set_frame_buffer_raster(h,zero_color,f)&&!x.cameras->begin_update(h,f)&&
            f.kind==RetailRendererCameraError::raster_binding&&!x.cameras->is_updating(h,f),
            "zero logical color extent refused before GPU ownership");
  gpu_check(x.cameras->set_frame_buffer_raster(h,color,f)&&x.cameras->set_z_buffer_raster(h,zero_depth,f)&&
            !x.cameras->begin_update(h,f)&&f.kind==RetailRendererCameraError::raster_binding&&!x.cameras->is_updating(h,f),
            "zero logical depth extent refused before GPU ownership");
  gpu_binding(draw_binding,0,"zero-extent refusals leave default framebuffer unchanged");
  gpu_viewport({2,3,9,10},"zero-extent refusals leave host viewport unchanged");
  gpu_check(x.cameras->set_z_buffer_raster(h,depth,f),"restore positive logical depth extent");
  gpu_check(x.cameras->set_frame_buffer_raster(h,live_texture,f)&&!x.cameras->begin_update(h,f)&&
            f.kind==RetailRendererCameraError::render_services_required,"live type5 target requires separate render-to-texture service");
  gpu_check(x.cameras->set_frame_buffer_raster(h,unbacked,f)&&!x.cameras->begin_update(h,f)&&
            f.kind==RetailRendererCameraError::raster_binding&&f.raster.kind==RetailRendererRasterError::missing_backing,
            "unbacked live color cannot begin successful no-op");
  gpu_check(x.rasters->destroy_raster(stale,rf)&&!x.cameras->set_frame_buffer_raster(h,stale,f)&&
            f.kind==RetailRendererCameraError::raster_binding&&f.raster.kind==RetailRendererRasterError::retired_raster,
            "stale raster rejected before color publication");
  gpu_check(x.cameras->set_frame_buffer_raster(h,color,f),"restore admitted color");
  const auto mismatch=x.backed(1,a,320,224);
  gpu_check(x.cameras->set_z_buffer_raster(h,mismatch,f)&&!x.cameras->begin_update(h,f)&&
            f.kind==RetailRendererCameraError::raster_binding,"different logical color/depth extents refused");
  auto b=native_display();const auto other_depth=x.backed(1,b);DisplayServiceFailure df;
  gpu_check(a->acquire_context(df)&&x.cameras->set_z_buffer_raster(h,other_depth,f)&&!x.cameras->begin_update(h,f)&&
            f.kind==RetailRendererCameraError::raster_binding,"different actual display backing refused");
  gpu_check(x.cameras->set_z_buffer_raster(h,depth,f)&&x.cameras->set_render_target_policy(h,{{0,0,65,48},0.375f},f)&&
            !x.cameras->begin_update(h,f)&&f.kind==RetailRendererCameraError::native_target&&
            f.display.error==DisplayServiceError::invalid_dimensions,"out-of-physical-target viewport refused");
  gpu_check(!x.cameras->set_render_target_policy(h,{{0,0,0,8},0.375f},f)&&f.kind==RetailRendererCameraError::native_target&&
            !x.cameras->set_render_target_policy(h,{{0,0,8,8},1.25f},f)&&f.kind==RetailRendererCameraError::native_target,
            "zero viewport and out-of-domain depth policy refused");
  gpu_check(x.cameras->set_render_target_policy(h,{{5,7,13,11},std::nullopt},f)&&x.cameras->begin_update(h,f),
            "optional depth policy does not invent a clear-depth value");
  gpu_check(!x.cameras->clear(h,0,0,0,1,2,f)&&f.kind==RetailRendererCameraError::native_target,
            "requested depth clear needs explicit normalized depth policy");
  gpu_check(x.cameras->clear(h,0.25f,0.5f,0.75f,1,1,f)&&x.cameras->end_update(h,f),"color-only clear needs no inferred depth convention");
  const auto color_only=x.camera(color);
  gpu_check(x.cameras->begin_update(color_only,f)&&x.cameras->clear(color_only,0.1f,0.2f,0.3f,1,1,f),
            "actual color-only target permits native color clear without depth raster");
  gpu_check(!x.cameras->clear(color_only,0,0,0,1,2,f)&&f.kind==RetailRendererCameraError::native_target&&
            x.cameras->end_update(color_only,f),"color-only target refuses invented depth attachment");
  gpu_check(x.cameras->set_render_target_policy(h,{{5,7,13,11},0.375f},f)&&x.cameras->begin_update(h,f),"begin wrong-context cleanup scenario");
  gpu_check(b->acquire_context(df)&&!x.cameras->clear(h,0,0,0,1,1,f)&&f.kind==RetailRendererCameraError::native_target,
            "active clear refuses actual wrong GL context");
  gpu_check(!x.cameras->end_update(h,f)&&f.kind==RetailRendererCameraError::native_target&&x.cameras->is_updating(h,f),
            "wrong-context end retains unfinished ownership");
  gpu_check(a->acquire_context(df)&&x.cameras->end_update(h,f),"same actual context performs explicit cleanup");
  b.reset();
  gpu_check(x.cameras->set_z_buffer_raster(h,other_depth,f)&&!x.cameras->begin_update(h,f)&&
            f.kind==RetailRendererCameraError::raster_binding&&f.raster.kind==RetailRendererRasterError::stale_display,
            "expired actual display capability cannot be reused by camera begin");
  gpu_check(x.cameras->set_z_buffer_raster(h,depth,f),"restore live depth capability after stale display refusal");
  gpu_check(x.cameras->begin_update(h,f)&&x.rasters->destroy_raster(depth,rf),"retire logical depth while pass is active");
  gpu_check(!x.cameras->clear(h,0,0,0,1,3,f)&&f.kind==RetailRendererCameraError::raster_binding,
            "stale active raster refuses subsequent clear");
  gpu_check(x.cameras->end_update(h,f)&&!x.cameras->is_updating(h,f),"end cleans host pass despite stale logical raster");
  gpu_check(!x.cameras->begin_update(h,f)&&f.kind==RetailRendererCameraError::raster_binding,
            "later begin cannot reuse retired depth raster");
 }
 {
  ++gpu_scenarios;CameraGpuFixture x(root);auto a=native_display();RetailRendererCameraFailure f;DisplayServiceFailure df;
  const auto color=x.backed(2,a),depth=x.backed(1,a);const auto h=x.camera(color,depth),other=x.camera(color,depth);
  glViewport(3,4,21,19);
  gpu_check(x.cameras->begin_update(h,f)&&x.cameras->destroy_base_camera(h,f),"current-context camera destruction ends actual pass");
  gpu_viewport({3,4,21,19},"camera destroy restores previous host viewport");
  gpu_check(x.cameras->begin_update(other,f),"another actual camera follows released owner");
  auto b=native_display();
  gpu_check(!x.cameras->destroy_base_camera(other,f)&&f.kind==RetailRendererCameraError::native_target&&
            x.cameras->observe(other,f).has_value(),"wrong-context camera destruction refuses before record release");
  gpu_check(a->acquire_context(df)&&x.cameras->destroy_base_camera(other,f),"restore actual context before camera release");
  const auto retiring=x.camera(color,depth);
  gpu_check(x.cameras->begin_update(retiring,f),"current-context module-retirement pass begins");
  renderer_required::destroy_core_plugin_0405(*x.core);
  gpu_viewport({3,4,21,19},"current-context module retirement restores previous viewport");
  gpu_check(!x.cameras->observe(retiring,f)&&f.kind==RetailRendererCameraError::retired,"retired module invalidates camera record");
  gpu_check(renderer_required::construct_core_plugin_0405(*x.core,x.native.f),"construct successor real camera module");
  x.cameras=x.core->camera_module(x.native.f);const auto successor=x.camera(color,depth);
  gpu_check(x.cameras->begin_update(successor,f)&&x.cameras->end_update(successor,f),"retired current-context owner left no stolen pass");
 }
 {
  ++gpu_scenarios;CameraGpuFixture x(root);auto a=native_display();RetailRendererCameraFailure f;DisplayServiceFailure df;
  const auto color=x.backed(2,a),depth=x.backed(1,a);const auto h=x.camera(color,depth);
  glViewport(4,5,22,20);gpu_check(x.cameras->begin_update(h,f),"abandonment scenario owns actual native target");
  auto b=native_display();renderer_required::destroy_core_plugin_0405(*x.core);
  gpu_check(!x.cameras->observe(h,f)&&f.kind==RetailRendererCameraError::retired,"wrong-context module retirement expires camera owner");
  gpu_check(renderer_required::construct_core_plugin_0405(*x.core,x.native.f),"abandonment successor actual0405 module");
  x.cameras=x.core->camera_module(x.native.f);const auto successor=x.camera(color,depth);
  gpu_check(a->acquire_context(df),"host reacquires abandoned target context");
  gpu_check(!x.cameras->begin_update(successor,f)&&f.kind==RetailRendererCameraError::native_target&&
            f.display.error==DisplayServiceError::resource_busy,"successor cannot steal abandoned pass");
  gpu_check(a->restore_abandoned_render_target(df),"explicit host recovery restores expired-owner pass");
  gpu_viewport({4,5,22,20},"abandoned owner recovery restores previous viewport");
  gpu_check(x.cameras->begin_update(successor,f)&&x.cameras->end_update(successor,f),"successor begins only after explicit host recovery");
 }
 std::printf("PASS_NATIVE_CAMERA_GPU_TARGET scenarios=%u checks=%u; actual0405/0407/native hidden displays,physical viewport,scissored RGBA/depth readback,state restoration,exclusive camera ownership,Session/type/lifetime/error refusal; no scene/frustum or whole-game claim\n",gpu_scenarios,gpu_checks);
}
}
#endif
namespace mka::retail::renderer_required {
#define OTHER(ID,VALUE) bool construct_core_plugin_##ID(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(VALUE,c,f);} \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&)noexcept{fixture_destroy(VALUE);}
OTHER(0409,0x409)
OTHER(040b,0x40b)
#undef OTHER
// Actual0403/0404/0405/registration are linked. Unimplemented renderer math
// must never execute in a base-camera ownership/default-value check.
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&){std::abort();}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&){std::abort();}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&){std::abort();}
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState&,std::shared_ptr<NativeRetailRendererCore> c,StartupFailure& f){
 ++suffix_calls;RetailRendererCoreFailure cf;check(!c->construct_registered(cf),"explicit later0408 refusal after actual0403..0407");
 f={StartupError::memory_owners,"test-only0408/full renderer suffix refusal"};return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
static_assert(!std::is_default_constructible_v<NativeRetailRendererCameraModule>);
static_assert(!std::is_copy_constructible_v<NativeRetailRendererCameraModule>);
int main(int argc,char** argv)try{
 check(argc==3||argc==4,"root/depth corpus and optional explicit ignored GPU diagnostic directory required");
 check(std::fegetround()==FE_TONEAREST,"source binary32 fixture scope requires existing nearest rounding; settings not changed");
 const std::filesystem::path root(argv[1]);
 camera_frame_association_cases(root);
 {
  Fixture x(root);auto c=x.core();check(!c->camera_module(x.f)&&c->module_0405_count(x.f)==0,"module absent before real constructor");
  auto m=x.module();RetailRendererCameraFailure f;check(m&&c->module_0405_count(x.f)==1&&m->live_base_cameras(f)==0,"actual0405 native publication/count/empty resource");
  check(!renderer_required::construct_core_plugin_0405(*c,x.f)&&x.f.kind==RetailRendererCoreError::reentrant&&c->camera_module(x.f)==m,"duplicate never replaces a live resource");
  const auto baseline=x.live();
  check(!m->create_base_camera(std::nullopt,f)&&f.kind==RetailRendererCameraError::device_depth_required&&x.live()==baseline,"missing depth provider is a named blocker, not BSS/default GPU");
  check(!m->create_base_camera(RetailRendererCameraDepthInputs{std::numeric_limits<float>::infinity(),1},f)&&
    f.kind==RetailRendererCameraError::unverified_numeric_domain&&x.live()==baseline,"unverified depth inputs fail before native effects");
  std::ifstream input(argv[2]);std::string header;std::getline(input,header);std::uint32_t near,far,scale,shift;
  while(input>>near>>far>>scale>>shift){
   auto h=m->create_base_camera(RetailRendererCameraDepthInputs{std::bit_cast<float>(near),std::bit_cast<float>(far)},f);
   check(bool(h),"real camera creation for independent source depth case");auto o=m->observe(*h,f);
   check(o&&std::bit_cast<std::uint32_t>(o->depth_scale)==scale&&std::bit_cast<std::uint32_t>(o->depth_shift)==shift,"independent exact-rational per-SSE expected bits");
   check(o->view_window.x==1&&o->view_window.y==1&&o->inverse_window.x==1&&o->inverse_window.y==1&&
    std::bit_cast<std::uint32_t>(o->near_clip)==0x3d4ccccdu&&o->far_clip==10&&o->fog_plane==5&&
    o->view_offset.x==0&&o->view_offset.y==0&&o->projection_type==1&&!o->synchronized_view_available,"original initialized defaults, no invented view/frustum");
   check(!m->require_render_ready(*h,f)&&f.kind==RetailRendererCameraError::render_services_required,"base camera is not reported as a working renderer");
   check(m->destroy_base_camera(*h,f)&&x.live()==baseline,"actual payload release while weak handle survives");
   check(!m->observe(*h,f)&&f.kind==RetailRendererCameraError::retired_camera,"retired camera cannot alias reusable storage");++golden_cases;
  }check(golden_cases==7,"all seven independently generated cases executed");
  auto h=create(m);check(m->set_view_window(h,{2,-4},f),"actual source scalar window update");
  auto o=m->observe(h,f);check(o&&o->view_window.x==2&&o->view_window.y==-4&&o->inverse_window.x==0.5f&&o->inverse_window.y==-0.25f,"negative/positive reciprocal semantics");
  const auto minus_zero=std::bit_cast<float>(std::uint32_t{0x80000000u});
  check(m->set_view_window(h,{0,minus_zero},f),"source zero-width SSE semantics are not clamped");
  o=m->observe(h,f);check(o&&std::isinf(o->inverse_window.x)&&!std::signbit(o->inverse_window.x)&&
    std::isinf(o->inverse_window.y)&&std::signbit(o->inverse_window.y),"signed-zero reciprocal signs preserved");
  check(m->destroy_base_camera(h,f),"actual window-case destruction");
  auto replacement=create(m);check(!replacement.same_identity(h)&&m->destroy_base_camera(replacement,f),"native handle identity survives allocation reuse safely");
  std::vector<NativeRetailRendererCameraHandle> many;many.reserve(64);
  for(unsigned i=0;i<64;++i)many.push_back(create(m));
  check(m->live_base_cameras(f)==64&&x.live()==baseline+64,"64 real native payloads; no four-object pool cap/layout");
  for(const auto& v:many)check(m->destroy_base_camera(v,f),"real resource payload destruction");
  check(x.live()==baseline,"all payloads physically released with64 weak handles retained");
  bool wrong=false;std::thread other([&]{RetailRendererCameraFailure q;wrong=!m->live_base_cameras(q)&&q.kind==RetailRendererCameraError::wrong_thread;});other.join();check(wrong,"foreign thread cannot use actual module");
  Fixture foreign(root);auto fm=foreign.module();auto fh=create(fm);
  check(!m->destroy_base_camera(fh,f)&&f.kind==RetailRendererCameraError::foreign_owner&&fm->destroy_base_camera(fh,f),"native resource identity not mixed across owners");
  renderer_required::destroy_core_plugin_0405(*foreign.core());renderer_required::destroy_core_plugin_0405(*c);
  check(c->module_0405_count(x.f)==0&&!c->camera_module(x.f)&&!m->live_base_cameras(f)&&f.kind==RetailRendererCameraError::retired,"actual module close/count/retained-handle invalidation");
 }
 {
  bool done=false;
  for(int cut=0;cut<16;++cut){Fixture x(root);auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized,"real CoreInit for constructor cuts");
   const auto baseline=x.live();fail_after=cut;const bool ok=renderer_required::construct_core_plugin_0405(*c,x.f);fail_after=-1;
   if(ok){done=true;renderer_required::destroy_core_plugin_0405(*c);break;}++module_cuts;
   check((x.f.kind==RetailRendererCoreError::allocation||x.f.kind==RetailRendererCoreError::allocator)&&x.live()==baseline,"actual constructor allocation refusal cleanup");
   check(c->module_0405_count(x.f)==0&&!c->camera_module(x.f),"failed factory publishes no count/resource");
   check(renderer_required::construct_core_plugin_0405(*c,x.f),"actual factory retry");renderer_required::destroy_core_plugin_0405(*c);
  }check(done&&module_cuts,"all reached constructor cuts");
  done=false;
  for(int cut=0;cut<16;++cut){Fixture x(root);auto m=x.module();RetailRendererCameraFailure f;auto keep=create(m);check(m->set_view_window(keep,{2,4},f),"existing live record before allocation cut");
   const auto baseline=x.live();fail_after=cut;auto h=m->create_base_camera(explicit_fixture_depth(),f);fail_after=-1;
   if(h){done=true;check(m->destroy_base_camera(*h,f)&&m->destroy_base_camera(keep,f),"actual successful allocation endpoint");renderer_required::destroy_core_plugin_0405(*x.core());break;}++camera_cuts;
   check((f.kind==RetailRendererCameraError::allocation||f.kind==RetailRendererCameraError::allocator)&&x.live()==baseline,"failed camera admission releases actual storage");
   check(m->live_base_cameras(f)==1&&m->observe(keep,f)->inverse_window.y==0.25f,"existing camera state preserved on failed admission");
   auto retry=create(m);check(m->destroy_base_camera(retry,f)&&m->destroy_base_camera(keep,f),"real allocation retry");renderer_required::destroy_core_plugin_0405(*x.core());
  }check(done&&camera_cuts,"all reached camera allocation cuts");
 }
 {
  Fixture x(root);later=Later::refuse;
  check(main_binding::initialize_retail_first_scene(x.main,x.sf)==RetailStartupGate::native_failure&&suffix_calls==1,"actual Main/Core constructor prefix executes0405 then later fixture refuses");
  RetailRendererCameraFailure f;check(captured&&!captured->observe(captured_camera,f)&&f.kind==RetailRendererCameraError::retired&&x.core()->module_0405_count(x.f)==0,"real later-failure unwind retires actual camera payload/count once");captured.reset();
 }
 {
  Fixture x(root);x.tasks();auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized,"actual Core transfer setup");
  later=Later::transfer;transfer_main=&x.main;bool moved=false;
  try{(void)c->construct_registered(x.f);}catch(const detail::NativeMainReentry& e){moved=e.owner==&x.main&&e.value==79;}
  RetailRendererCameraFailure f;check(moved&&captured&&captured->observe(captured_camera,f).has_value()&&c->camera_module(x.f)==captured,"legitimate Main transfer preserves actual0405 prefix owner");
  check(main_binding::retail_task_exists(x.main,0x7654,x.sf)==false,"next source frame still performs actual native task lookup");
  auto next=create(captured);check(captured->destroy_base_camera(next,f)&&c->destroy_constructed(x.f),"continued ownership and one real prefix cleanup");
  check(!captured->live_base_cameras(f),"transfer cleanup invalidates retained module");captured.reset();transfer_main=nullptr;later=Later::refuse;
 }
 {
  std::shared_ptr<NativeRetailRendererCameraModule> held;std::weak_ptr<NativeRetailGameSession> session;NativeRetailRendererCameraHandle weak;later=Later::allow;
  {Fixture x(root);auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized&&c->construct_registered(x.f),"explicit later-factory fixtures around actual0405 lifetime");
   held=c->camera_module(x.f);weak=create(held);session=x.startup.game_session;}
  RetailRendererCameraFailure f;check(session.expired()&&!held->observe(weak,f)&&f.kind==RetailRendererCameraError::retired,"external camera/module handles cannot retain usable Session/game storage");later=Later::refuse;
 }
 {
  Fixture x(root);auto m=x.module();RetailRendererCameraFailure f;
  auto h=create(m);
  check(!m->frame_buffer_raster(h, f).has_value() || m->frame_buffer_raster(h, f)->empty(), "camera initial frame buffer raster is empty");
  check(!m->z_buffer_raster(h, f).has_value() || m->z_buffer_raster(h, f)->empty(), "camera initial z buffer raster is empty");
  check(!m->is_updating(h, f), "camera initial updating state is false");

  // Lifecycle check: begin_update without frame buffer raster fails with clear error
  check(!m->begin_update(h, f) && f.kind == RetailRendererCameraError::render_services_required,
   "begin_update without frame buffer raster rejected");
  check(!m->end_update(h, f) && f.kind == RetailRendererCameraError::render_services_required,
   "end_update without active begin_update rejected");
  check(!m->clear(h, 0.0f, 0.0f, 0.0f, 1.0f, 0x01, f) && f.kind == RetailRendererCameraError::render_services_required,
   "clear without active begin_update rejected");

  // Unowned fixture identities cannot supply production raster capabilities.
  auto color_raster = NativeRetailRasterHandle::test_handle();
  auto depth_raster = NativeRetailRasterHandle::test_handle();
  check(!m->set_frame_buffer_raster(h,color_raster,f)&&f.kind==RetailRendererCameraError::raster_binding,
        "unowned test color raster rejected");
  check(!m->set_z_buffer_raster(h,depth_raster,f)&&f.kind==RetailRendererCameraError::raster_binding,
        "unowned test depth raster rejected");
  check(m->frame_buffer_raster(h,f)->empty()&&m->z_buffer_raster(h,f)->empty(),"refused fixture rasters publish no fields");
  check(!m->begin_update(h,f)&&f.kind==RetailRendererCameraError::render_services_required&&!m->is_updating(h,f),
        "unbound target cannot begin successful no-op");

  check(m->destroy_base_camera(h, f), "cleanup test camera");
 }
#ifdef _WIN32
 std::optional<std::filesystem::path> diagnostic;
 if(argc==4){
  const auto repository=std::filesystem::canonical(std::filesystem::path(__FILE__)).parent_path().parent_path();
  const auto requested=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[3]));
  bool ignored=false;
  for(const auto& allowed:{repository/"b"/"phase7-capture-run",repository/"build"/"oracle-telemetry"}){
   const auto relative=requested.lexically_relative(allowed);
   if(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=".."&&relative!=".")ignored=true;
  }
  check(ignored,"GPU diagnostic output must be explicit child of existing ignored capture/telemetry root");
  std::filesystem::create_directories(requested);diagnostic=requested;
 }
 camera_gpu_target_cases(root,diagnostic);
#else
 check(argc==3,"native GPU diagnostics require Windows display service");
#endif
 {
  // 17EBB0 / 180ED0 producer, frame dirty-queue sync, callback ordering, and partial-failure rollback.
  Fixture x(root);
  auto c=x.core();
  auto initialized=c->initialize_core(x.f);check(initialized&&*initialized,"producer core init");
  check(renderer_required::construct_core_plugin_0403(*c,x.f)&&
        renderer_required::construct_core_plugin_0405(*c,x.f)&&
        renderer_required::construct_core_plugin_0407(*c,x.f),"producer plugins init");
  auto m=c->camera_module(x.f);
  auto frames=c->frames(x.f);
  auto rasters=c->rasters(x.f);
  RetailGameSessionFailure gsf;
  auto* world_cams=x.startup.game_session->render_cameras(gsf);
  check(world_cams!=nullptr,"session render_cameras available");
  RetailRendererCameraFailure cf;RetailRendererFrameFailure ff;RetailRendererRasterFailure rf;
  // 1. Frame hierarchy dirty-queue sync + view/frustum computation.
  auto parent_f=frames->create_base_frame(ff);auto child_f=frames->create_base_frame(ff);
  check(parent_f&&child_f&&frames->add_child(*parent_f,*child_f,ff),"create parent/child frames");
  RetailRendererMatrixValue p_mod{{{1,0,0},{0,1,0},{0,0,1},{0,0,-5.0f}},0x20000u};
  RetailRendererMatrixValue c_mod{{{1,0,0},{0,1,0},{0,0,1},{0,0,2.0f}},0x20000u};
  check(frames->set_modelling(*parent_f,p_mod,ff)&&frames->set_modelling(*child_f,c_mod,ff),"set parent/child modelling");
  check(!frames->require_fully_synchronized(*child_f,ff)&&ff.kind==RetailRendererFrameError::synchronization_required,"dirty frame refuses require_fully_synchronized");
  auto cam=create(m);
  check(m->set_frame(cam,*child_f,cf),"bind child frame to camera");
  struct SyncLog { std::vector<int> order; bool reject_second{false}; } log;
  auto cb1=[](const NativeRetailRendererCameraHandle&,const RetailRendererBaseCameraObservation& obs,void* u)noexcept->bool{
   auto* l=static_cast<SyncLog*>(u);l->order.push_back(1);return obs.synchronized_view_available;
  };
  auto cb2=[](const NativeRetailRendererCameraHandle&,const RetailRendererBaseCameraObservation&,void* u)noexcept->bool{
   auto* l=static_cast<SyncLog*>(u);l->order.push_back(2);return !l->reject_second;
  };
  auto t1=m->register_sync_callback(cb1,&log,cf);auto t2=m->register_sync_callback(cb2,&log,cf);
  check(t1&&t2,"register two sync callbacks");
  check(m->synchronize(cam,*frames,cf),"synchronize camera and frame hierarchy");
  check(log.order==std::vector<int>{1,2},"sync callbacks invoked in deterministic registration order");
  check(frames->require_fully_synchronized(*child_f,ff),"frame hierarchy clean after synchronize");
  auto child_obs=frames->observe(*child_f,ff);
  check(child_obs&&std::fabs(child_obs->world.affine.translation.z-(-3.0f))<1e-5f,"child world translation accumulated (-5 + 2 = -3)");
  auto cam_obs=m->observe(cam,cf);
  check(cam_obs&&cam_obs->synchronized_view_available&&std::fabs(cam_obs->view_matrix.elements[14]-3.0f)<1e-5f,"view matrix translation inverted to +3");
  log.reject_second=true;log.order.clear();
  check(!m->synchronize(cam,*frames,cf)&&cf.kind==RetailRendererCameraError::render_services_required,"callback refusal fails synchronize");
  cam_obs=m->observe(cam,cf);
  check(cam_obs&&!cam_obs->synchronized_view_available,"refused callback clears synchronized_view_available");
  check(m->unregister_sync_callback(*t2,cf)&&m->unregister_sync_callback(*t1,cf),"unregister sync callbacks");
  check(m->destroy_base_camera(cam,cf)&&frames->destroy_root_hierarchy(*parent_f,ff),"cleanup frame/camera hierarchy");

  // 2. Partial-failure rollback for 17EBB0 and 180ED0 (must leave 0 leaked cameras/frames/rasters).
  RetailCameraProducerFailure pf;
  RetailCameraProducer17EBB0Config bad_cfg;bad_cfg.width=0;bad_cfg.height=48;bad_cfg.depth_inputs={0.0f,1.0f};
  check(!create_camera_producer_17ebb0(x.startup.game_session,*m,*frames,*rasters,bad_cfg,pf),"17EBB0 rejects zero extent");
  check(m->live_base_cameras(cf)==0&&frames->observe_module(ff)->live_base_frames==0&&rasters->live_rasters(rf)==0,"17EBB0 zero-extent failure leaks nothing");

  RetailCameraInit180ED0Config init_cfg;
  init_cfg.width=640;init_cfg.height=448;init_cfg.depth_inputs={0.0f,1.0f};
  // Deliberately omit store_initial_count_from_127260 so publish_camera_link_after_180ed0 fails mid-way.
  check(!initialize_cameras_180ed0(x.startup.game_session,*m,*frames,*rasters,*world_cams,init_cfg,pf),
   "180ED0 fails cleanly when 127260 count producer is absent");
  check(m->live_base_cameras(cf)==0&&frames->observe_module(ff)->live_base_frames==0&&rasters->live_rasters(rf)==0,
   "180ED0 partial-failure rollback leaves zero leaked cameras, frames, or rasters");

  RetailCameraWorldFailure wf;
  check(world_cams->store_initial_count_from_127260(wf)&&
        world_cams->store_count_from_127110(2,wf),"admit 2 camera slots");
  init_cfg.create_scene_camera=true;init_cfg.scene_has_color=true;init_cfg.scene_has_depth=true;
  auto init_ok=initialize_cameras_180ed0(x.startup.game_session,*m,*frames,*rasters,*world_cams,init_cfg,pf);
  check(init_ok.has_value()&&init_ok->scene.has_value(),"180ED0 initializes primary + scene cameras");
  auto p_obs=m->observe(init_ok->primary.camera,cf);auto s_obs=m->observe(init_ok->scene->camera,cf);
  check(p_obs&&std::fabs(p_obs->near_clip-0.25f)<1e-6f&&std::fabs(p_obs->far_clip-125.0f)<1e-5f&&p_obs->projection_type==1u,"180ED0 primary clip/projection defaults");
  check(s_obs&&std::fabs(s_obs->near_clip-0.25f)<1e-6f&&std::fabs(s_obs->far_clip-125.0f)<1e-5f&&s_obs->projection_type==1u,"180ED0 scene clip/projection defaults");
  destroy_camera_producer_17ebb0(*m,*frames,*rasters,*init_ok->scene);
  destroy_camera_producer_17ebb0(*m,*frames,*rasters,init_ok->primary);
  check(m->live_base_cameras(cf)==0&&frames->observe_module(ff)->live_base_frames==0&&rasters->live_rasters(rf)==0,"180ED0 cleanup leaves zero leaks");
  renderer_required::destroy_core_plugin_0407(*c);
  renderer_required::destroy_core_plugin_0405(*c);
  renderer_required::destroy_core_plugin_0403(*c);
 }
 std::printf("PASS_NATIVE0405_CAMERA_RESOURCE_BASE; actual Core/Main prefix,64 real records,source defaults/window/7 exact SSE cases,physical release,allocation cuts module=%u camera=%u,thread/foreign/lifetime/Main transfer/task lookup; depth provider/plugins/frame/sync/scene/game OPEN; hidden native display GPU checks separate,no audio\n",module_cuts,camera_cuts);
 return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL native0405 camera: %s\n",e.what());return 1;}
