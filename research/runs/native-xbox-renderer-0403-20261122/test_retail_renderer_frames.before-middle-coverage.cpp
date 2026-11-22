#include "mka/retail_renderer_frames.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int fail_after=-1;}
void* operator new(std::size_t n){
 if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;
 if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
enum class SuffixFixture {refuse_at_0404,transfer_at_0404,allow_unported_factories};
SuffixFixture suffix_fixture{SuffixFixture::refuse_at_0404};
NativeRetailMainState* transfer_main{};
std::shared_ptr<NativeRetailRendererFrames> captured;
NativeRetailRendererFrameHandle captured_frame;
unsigned ctor_cuts{},frame_cuts{},suffix_calls{};
bool other_construct(unsigned id,NativeRetailRendererCore& core,RetailRendererCoreFailure&){
 if(id==0x404&&suffix_fixture!=SuffixFixture::allow_unported_factories){
  RetailRendererCoreFailure cf;captured=core.frames(cf);check(bool(captured),"0404 fixture sees actual prior0403 resource");
  RetailRendererFrameFailure f;auto frame=captured->create_base_frame(f);
  check(frame&&captured->mark_dirty(*frame,f),"actual base allocation/list before later fixture refusal");captured_frame=*frame;
  if(suffix_fixture==SuffixFixture::transfer_at_0404)reenter_native_retail_main(*transfer_main,67);
  return false;
 }
 if(suffix_fixture!=SuffixFixture::allow_unported_factories)std::abort();
 return true; // Explicit other-factory lifetime fixture, not renderer behavior.
}
void other_destroy(unsigned)noexcept{
 if(suffix_fixture!=SuffixFixture::allow_unported_factories)std::abort();
}
struct Fixture {
 NativeRetailStartupState startup;mka::input::ControllerInput input;NativeRetailMainState main;
 StartupFailure sf;RetailRendererCoreFailure f;RetailRendererAllocatorFailure af;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
 explicit Fixture(const std::filesystem::path& path):startup(open(path),{}),main(startup,input){
  check(initialize_native_retail_game_session(startup,sf)&&initialize_retail_memory_owners(startup,sf),"actual Session/native memory owners");
  allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual Permanent capability");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& path){
  mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(path,f);
  check(bool(v),"actual native content root");return std::move(*v);
 }
 std::shared_ptr<NativeRetailRendererCore> core(){
  auto c=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);
  check(bool(c),"actual persistent Core");return c;
 }
 std::shared_ptr<NativeRetailRendererFrames> module(){
  auto c=core();auto initialized=c->initialize_core(f);check(initialized&&*initialized,"actual CoreInit and selected raster registration");
  check(renderer_required::construct_core_plugin_0403(*c,f),"ACTUAL0403 native factory");
  auto m=c->frames(f);check(bool(m),"actual0403 publication");return m;
 }
 std::size_t live(){
  RetailMemoryOwnerFailure f;auto o=startup.memory_owners->observe(RetailMemoryRole::permanent,f);
  check(bool(o),"actual Permanent observation");return o->heap.actual_live_allocations;
 }
 void task_roots(){
  RetailGamePoolFailure pf;startup.game_pool=std::shared_ptr<NativeRetailGamePool>(NativeRetailGamePool::initialize(3,pf));
  RetailGameSessionFailure sf;RetailTaskDomainFailure df;auto* tasks=startup.game_session->task_domain(sf);
  check(startup.game_pool&&tasks&&tasks->reset_task_roots_from_12f0d0(startup.game_pool,df),"actual task lookup roots for Main-transfer continuation");
 }
};
NativeRetailRendererFrameHandle create(const std::shared_ptr<NativeRetailRendererFrames>& m){
 RetailRendererFrameFailure f;auto h=m->create_base_frame(f);check(bool(h),"real base-frame allocation");return *h;
}
void queue(const std::shared_ptr<NativeRetailRendererFrames>& m,
 std::initializer_list<NativeRetailRendererFrameHandle> expected){
 RetailRendererFrameFailure f;auto actual=m->dirty_roots(f);
 check(actual&&actual->size()==expected.size(),"independent dirty-root length");
 std::size_t i{};for(const auto& h:expected)check((*actual)[i++].same_identity(h),"independent source dirty-root order");
}
}
namespace mka::retail::renderer_required {
#define FIXTURE(ID,VALUE) \
 bool construct_core_plugin_##ID(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return other_construct(VALUE,c,f);} \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&)noexcept{other_destroy(VALUE);}
FIXTURE(0404,0x404) FIXTURE(0405,0x405) FIXTURE(0406,0x406) FIXTURE(0407,0x407)
FIXTURE(0408,0x408) FIXTURE(0409,0x409) FIXTURE(0412,0x412) FIXTURE(040a,0x40a) FIXTURE(040b,0x40b)
#undef FIXTURE
// Math is not used by base birth/link/list ownership and must not be invented.
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&){std::abort();}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&){std::abort();}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&){std::abort();}
//0403 and object-extension registration/cleanup are production, not fixtures.
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState&,
 std::shared_ptr<NativeRetailRendererCore> core,StartupFailure& f){
 ++suffix_calls;RetailRendererCoreFailure cf;
 check(!core->construct_registered(cf),"explicit0404 suffix fixture refusal after actual0403");
 f={StartupError::memory_owners,"test-only later0404/full renderer suffix refused"};return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
static_assert(!std::is_default_constructible_v<NativeRetailRendererFrames>);
static_assert(!std::is_copy_constructible_v<NativeRetailRendererFrames>);
int main(int argc,char** argv)try{
 check(argc==2,"existing CPU-only content root required");const std::filesystem::path root(argv[1]);
 {
  Fixture x(root);auto c=x.core();check(!c->frames(x.f)&&c->module_0403_count(x.f)==0,"module absent before real constructor");
  auto m=x.module();RetailRendererFrameFailure f;check(c->module_0403_count(x.f)==1,"real module lifetime publication");
  check(!renderer_required::construct_core_plugin_0403(*c,x.f)&&x.f.kind==RetailRendererCoreError::reentrant,"duplicate cannot replace a live resource");
  check(m->observe_module(f)->live_base_frames==0&&m->observe_module(f)->dirty_roots==0,"source empty resource/list birth");
  const auto baseline=x.live();auto a=create(m),b=create(m);
  const auto initial=m->observe(a,f);const RetailRendererAffine identity{{1,0,0},{0,1,0},{0,0,1},{0,0,0}};
  check(initial&&initial->modelling.affine==identity&&initial->world.affine==identity&&
    initial->modelling.properties==0x20003&&initial->world.properties==0x20003&&
    initial->private_flags==0&&initial->parent.empty()&&initial->first_child.empty()&&initial->root.same_identity(a),
    "original identity bits/properties and independent empty links/self-root");
  check(x.live()==baseline+2&&m->require_fully_synchronized(a,f),"two actual heap-backed base records and source clean birth");
  check(m->mark_dirty(a,f)&&m->mark_dirty(a,f),"actual dirty list insertion/repeat");
  queue(m,{a});check(m->observe(a,f)->private_flags==0x0f,"root/node flags retain exact low bits");
  check(!m->require_fully_synchronized(a,f)&&f.kind==RetailRendererFrameError::synchronization_required,"dirty work never reports stale matrices ready");
  check(m->destroy_leaf(a,f)&&m->destroy_leaf(b,f),"actual payload release");
  check(x.live()==baseline,"weak handles do NOT retain actual NativeAllocation payloads");
  check(!m->observe(a,f)&&f.kind==RetailRendererFrameError::retired_frame,"stale handle refused after physical release");
  auto replacement=create(m);check(!replacement.same_identity(a),"native allocation reuse never aliases retired identity");
  check(m->destroy_leaf(replacement,f),"replacement actual cleanup");
  std::vector<NativeRetailRendererFrameHandle> many;many.reserve(120);
  for(unsigned i=0;i<120;++i)many.push_back(create(m));
  check(m->observe_module(f)->live_base_frames==120,"host growth exceeds source50-entry block with no console ceiling");
  for(auto i:{0u,60u,119u})check(m->mark_dirty(many[i],f),"stable records during host owner growth");
  queue(m,{many[119],many[60],many[0]});
  for(const auto& h:many)check(m->destroy_root_hierarchy(h,f),"actual root payload destruction across host growth");
  check(x.live()==baseline&&m->observe_module(f)->live_base_frames==0,"all120 payloads physically released while weak handles remain");
  renderer_required::destroy_core_plugin_0403(*c);
  check(c->module_0403_count(x.f)==0&&!c->frames(x.f)&&!m->observe_module(f)&&f.kind==RetailRendererFrameError::retired,"real native module retirement/count");
 }
 {
  Fixture x(root);auto c=x.core();auto m=x.module();RetailRendererFrameFailure f;
  auto a=create(m),b=create(m),d=create(m),child=create(m);
  check(m->mark_dirty(b,f)&&m->add_child(a,b,f)&&m->add_child(b,child,f),"source unlink-old-root, prepend child, rewrite subtree roots");
  queue(m,{a});check(m->observe(child,f)->root.same_identity(a)&&m->observe(b,f)->private_flags==0x0c,"real descendant root/flags");
  check(m->mark_dirty(d,f)&&m->mark_dirty(child,f),"new root prepends; duplicate descendant never reorders");
  queue(m,{d,a});
  check(m->detach(b,f),"actual detach root rewrite/queue");queue(m,{b,d,a});
  check(m->observe(child,f)->root.same_identity(b)&&m->observe(b,f)->parent.empty(),"detached subtree becomes own root");
  check(m->add_child(d,b,f),"actual reparent removes former dirty entry");queue(m,{d,a});
  check(m->observe(child,f)->root.same_identity(d),"all subtree roots rewritten");
  check(!m->add_child(b,d,f)&&f.kind==RetailRendererFrameError::cycle,"native invalid-cycle precondition fails before effects");queue(m,{d,a});
  check(!m->destroy_leaf(d,f)&&f.kind==RetailRendererFrameError::nonleaf_single_delete_required,"unknown nonleaf single-delete contract is named, not guessed");
  check(!m->destroy_root_hierarchy(b,f)&&f.kind==RetailRendererFrameError::attached_hierarchy_required,"attached hierarchy requires verified caller detach");
  check(m->add_child(a,child,f),"parented child detaches then prepends under new root");queue(m,{d,a});
  check(m->observe(b,f)->first_child.empty()&&m->observe(child,f)->root.same_identity(a),"old/new child-chain preservation");
  check(m->destroy_root_hierarchy(d,f),"postorder real root hierarchy retirement");queue(m,{a});
  check(!m->observe(b,f)&&m->observe(child,f).has_value(),"unrelated tree stays alive");
  bool wrong=false;std::thread other([&]{RetailRendererFrameFailure q;wrong=!m->mark_dirty(a,q)&&q.kind==RetailRendererFrameError::wrong_thread;});other.join();
  check(wrong,"foreign thread cannot mutate actual resource");queue(m,{a});
  Fixture foreign(root);auto other_module=foreign.module();auto other_frame=create(other_module);
  check(!m->add_child(a,other_frame,f)&&f.kind==RetailRendererFrameError::foreign_owner,"foreign native resource refused");
  check(other_module->destroy_leaf(other_frame,f),"foreign test resource cleanup");
  renderer_required::destroy_core_plugin_0403(*foreign.core());
  check(m->destroy_root_hierarchy(a,f)&&m->destroy_root_hierarchy({},f),"source root/null hierarchy cases");
  queue(m,{});renderer_required::destroy_core_plugin_0403(*c);
 }
 {
  bool end=false;
  for(int cut=0;cut<16;++cut){
   Fixture x(root);auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized,"allocation fixture real CoreInit");
   const auto baseline=x.live();fail_after=cut;const bool ok=renderer_required::construct_core_plugin_0403(*c,x.f);fail_after=-1;
   if(ok){end=true;renderer_required::destroy_core_plugin_0403(*c);break;}
   ++ctor_cuts;check(x.f.kind==RetailRendererCoreError::allocation||x.f.kind==RetailRendererCoreError::allocator,"module allocation refusal remains named");
   check(c->module_0403_count(x.f)==0&&!c->frames(x.f)&&x.live()==baseline,"failed constructor publishes no resource/count or leaked payload");
   check(renderer_required::construct_core_plugin_0403(*c,x.f),"real constructor retry");renderer_required::destroy_core_plugin_0403(*c);
  }
  check(end&&ctor_cuts>0,"all reached module allocation cuts");
  end=false;
  for(int cut=0;cut<16;++cut){
   Fixture x(root);auto m=x.module();RetailRendererFrameFailure f;const auto baseline=x.live();
   fail_after=cut;auto h=m->create_base_frame(f);fail_after=-1;
   if(h){end=true;check(m->destroy_leaf(*h,f),"actual successful allocation endpoint");renderer_required::destroy_core_plugin_0403(*x.core());break;}
   ++frame_cuts;check(f.kind==RetailRendererFrameError::allocation||f.kind==RetailRendererFrameError::allocator,"base allocation error retained");
   check(m->observe_module(f)->live_base_frames==0&&m->observe_module(f)->dirty_roots==0&&x.live()==baseline,"failed base publication leaks no payload/list identity");
   auto retry=create(m);check(m->destroy_leaf(retry,f)&&x.live()==baseline,"base allocation retry/weak-handle physical release");
   renderer_required::destroy_core_plugin_0403(*x.core());
  }
  check(end&&frame_cuts>0,"all reached base allocation cuts");
 }
 {
  Fixture x(root);suffix_fixture=SuffixFixture::refuse_at_0404;
  check(main_binding::initialize_retail_first_scene(x.main,x.sf)==RetailStartupGate::native_failure&&suffix_calls==1,"actual Main/Core registered construction reaches real0403 then explicit later refusal");
  RetailRendererFrameFailure f;check(captured&&!captured->observe(captured_frame,f)&&f.kind==RetailRendererFrameError::retired&&x.core()->module_0403_count(x.f)==0,"later constructor refusal really retires prior0403 payload/list/count");
  captured.reset();
 }
 {
  Fixture x(root);x.task_roots();auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized,"actual transfer fixture CoreInit");
  suffix_fixture=SuffixFixture::transfer_at_0404;transfer_main=&x.main;bool moved=false;
  try{(void)c->construct_registered(x.f);}catch(const detail::NativeMainReentry& e){moved=e.owner==&x.main&&e.value==67;}
  RetailRendererFrameFailure f;
  check(moved&&c->frames(x.f)==captured&&captured->observe(captured_frame,f).has_value(),"legitimate Main transfer retains actual0403 owner/records");
  check(main_binding::retail_task_exists(x.main,0x7654,x.sf)==false,"next source frame can still perform actual native task lookup");
  auto next=create(captured);check(captured->destroy_leaf(next,f),"resource stays usable after transfer, not poisoned");
  check(c->destroy_constructed(x.f)&&!captured->observe_module(f),"retained successful prefix retires once on actual cleanup");
  captured.reset();transfer_main=nullptr;suffix_fixture=SuffixFixture::refuse_at_0404;
 }
 {
  std::shared_ptr<NativeRetailRendererFrames> retained;NativeRetailRendererFrameHandle weak;
  std::weak_ptr<NativeRetailGameSession> session;suffix_fixture=SuffixFixture::allow_unported_factories;
  {Fixture x(root);auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized&&c->construct_registered(x.f),"explicit other-factory fixtures around actual0403 lifetime");
   retained=c->frames(x.f);weak=create(retained);session=x.startup.game_session;
  }
  RetailRendererFrameFailure f;check(session.expired()&&!retained->observe(weak,f)&&f.kind==RetailRendererFrameError::retired,"external resource/weak handles cannot retain Session or usable game data");
  suffix_fixture=SuffixFixture::refuse_at_0404;
 }
 std::printf("PASS_NATIVE0403_RESOURCE_BASE_FRAMES; actual Core factory/Main prefix,120 real allocations,birth/hierarchy/dirty order,physical release,thread/foreign/lifetime/Main transfer; allocation cuts module=%u frame=%u; plugins/clone/sync/GPU/game OPEN; no window/audio\n",ctor_cuts,frame_cuts);
 return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL native0403 frames: %s\n",e.what());return 1;}
