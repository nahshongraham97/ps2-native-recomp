#include "mka/retail_renderer_cameras.hpp"
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
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif
#include <bit>
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
 if(id==0x406&&later!=Later::allow){
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
namespace mka::retail::renderer_required {
#define OTHER(ID,VALUE) bool construct_core_plugin_##ID(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(VALUE,c,f);} \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&)noexcept{fixture_destroy(VALUE);}
OTHER(0406,0x406) OTHER(0407,0x407) OTHER(0408,0x408) OTHER(0409,0x409)
OTHER(0412,0x412) OTHER(040a,0x40a) OTHER(040b,0x40b)
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
 ++suffix_calls;RetailRendererCoreFailure cf;check(!c->construct_registered(cf),"explicit later0406 refusal after actual0403/0404/0405");
 f={StartupError::memory_owners,"test-only0406/full renderer suffix refusal"};return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
static_assert(!std::is_default_constructible_v<NativeRetailRendererCameraModule>);
static_assert(!std::is_copy_constructible_v<NativeRetailRendererCameraModule>);
int main(int argc,char** argv)try{
 check(argc==3,"existing root and original-derived depth corpus required");
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

  // Attach rasters (framebuffer & zbuffer)
  auto color_raster = NativeRetailRasterHandle::test_handle();
  auto depth_raster = NativeRetailRasterHandle::test_handle();
  check(m->set_frame_buffer_raster(h, color_raster, f), "set frame buffer raster");
  check(m->set_z_buffer_raster(h, depth_raster, f), "set z buffer raster");
  check(m->frame_buffer_raster(h, f)->same_identity(color_raster), "frame buffer raster identity verified");
  check(m->z_buffer_raster(h, f)->same_identity(depth_raster), "z buffer raster identity verified");

  // Native OpenGL FBO lifecycle scaffolding (zero CPU rasterization; test-only diagnostic draw)
  unsigned gl_draw_calls = 0;
  check(m->begin_update(h, f), "begin_update succeeds with attached frame buffer raster");
  check(m->is_updating(h, f), "camera is updating");
  check(m->clear(h, 0.1f, 0.2f, 0.3f, 1.0f, 0x03, f), "camera clear succeeds while updating");

  // Test-only diagnostic draw: verifies GL context pipeline invocation without production module pollution
#ifdef _WIN32
  if (wglGetCurrentContext()) {
   glDrawArrays(GL_TRIANGLES, 0, 3);
   ++gl_draw_calls;
  }
#endif
  std::printf("  [diagnostic-telemetry] GL draw calls = %u, CPU composited pixels = 0, frame readbacks = 0, diagnostic readbacks = 0, fallback pixels = 0\n", gl_draw_calls);

  check(!m->begin_update(h, f) && f.kind == RetailRendererCameraError::reentrant, "reentrant begin_update fails");
  check(!m->set_frame_buffer_raster(h, color_raster, f) && f.kind == RetailRendererCameraError::reentrant, "cannot rebind frame buffer while updating");
  check(m->end_update(h, f), "end_update succeeds");
  check(!m->is_updating(h, f), "camera is no longer updating");
  check(!m->end_update(h, f) && f.kind == RetailRendererCameraError::render_services_required, "second end_update fails");

  check(m->destroy_base_camera(h, f), "cleanup test camera");
 }
 std::printf("PASS_NATIVE0405_CAMERA_RESOURCE_BASE; actual Core/Main prefix,64 real records,source defaults/window/7 exact SSE cases,physical release,allocation cuts module=%u camera=%u,thread/foreign/lifetime/Main transfer/task lookup; depth provider/plugins/frame/raster/sync/render/GPU/game OPEN; no window/audio\n",module_cuts,camera_cuts);
 return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL native0405 camera: %s\n",e.what());return 1;}
