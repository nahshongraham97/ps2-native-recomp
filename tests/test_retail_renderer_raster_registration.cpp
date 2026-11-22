#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
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
 if(fail_after==0)throw std::bad_alloc();
 if(fail_after>0)--fail_after;
 if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
unsigned suffix_calls{},allocation_cuts{};bool callback_refused{};
struct Fixture {
 NativeRetailStartupState startup;mka::input::ControllerInput input;NativeRetailMainState main;
 StartupFailure sf;RetailRendererCoreFailure f;RetailRendererAllocatorFailure af;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
 explicit Fixture(const std::filesystem::path& path):startup(open(path),{}),main(startup,input){
  check(initialize_native_retail_game_session(startup,sf),"actual native Session");
  check(initialize_retail_memory_owners(startup,sf),"actual native memory owners");
  allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);
  check(bool(allocator),"actual native Permanent heap");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& path){
  mka::content::ContentVolumeFailure f;auto volume=mka::content::NativeContentVolume::open(path,f);
  check(bool(volume),"actual content volume");return std::move(*volume);
 }
 std::shared_ptr<NativeRetailRendererCore> core(){
  auto result=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);
  check(bool(result),"actual Session-owned Core");return result;
 }
};
bool reject_constructor(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 auto result=renderer_required::register_object_extensions_1b72b0(core,f);
 callback_refused=!result&&f.kind==RetailRendererCoreError::registration_closed;
 return false; // Explicit component fixture; construction must stop before unported modules.
}
void never_destroy(NativeRetailRendererCore&)noexcept{std::abort();}
}
namespace mka::retail::renderer_required {
// Still-unimplemented factories/math must NEVER execute in this metadata test.
#define UNPORTED(ID) \
 bool construct_core_plugin_##ID(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();} \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&)noexcept{std::abort();}
UNPORTED(0403) UNPORTED(0404) UNPORTED(0405) UNPORTED(0406) UNPORTED(0407)
UNPORTED(0408) UNPORTED(0409) UNPORTED(0412) UNPORTED(040a) UNPORTED(040b)
#undef UNPORTED
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&){std::abort();}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&){std::abort();}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&){std::abort();}
// NO object-extension registration/cleanup fixtures: both are production.
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState&,
 std::shared_ptr<NativeRetailRendererCore> core,StartupFailure& f){
 ++suffix_calls;RetailRendererCoreFailure cf;auto definition=core->raster_extension(cf);
 check(bool(definition),"actual CoreInit publishes real metadata before still-required suffix");
 check(definition->observe(cf)==RetailRendererRasterExtensionSpec{0x40c,false,false,false},
       "original ID and all three explicit null callbacks");
 f={StartupError::game_session,"test-only entire EngineOpen/Start suffix refusal"};
 return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
static_assert(!std::is_copy_constructible_v<NativeRetailRendererRasterExtensionRegistration>);
static_assert(!std::is_default_constructible_v<NativeRetailRendererRasterExtensionRegistration>);
int main(int argc,char** argv)try{
 check(argc==2,"existing content root required");const std::filesystem::path root(argv[1]);
 {
  Fixture x(root);auto core=x.core();
  check(!core->raster_extension(x.f)&&x.f.kind==RetailRendererCoreError::owner,"no fabricated pre-registration handle");
  check(main_binding::initialize_retail_first_scene(x.main,x.sf)==RetailStartupGate::native_failure&&suffix_calls==1,
        "actual connected first-scene reaches real CoreInit registration then explicit suffix refusal");
  auto definition=core->raster_extension(x.f);check(bool(definition),"production metadata retained by actual Core");
  auto initialized=core->initialize_core(x.f);
  check(initialized&&!*initialized&&core->raster_extension(x.f)==definition,"repeated CoreInit preserves actual metadata identity");
  auto errors=core->construct_error_domain(x.f);check(bool(errors),"actual existing error domain");
  auto duplicate=renderer_required::register_object_extensions_1b72b0(*core,x.f);
  check(duplicate&&*duplicate&&core->raster_extension(x.f)==definition,"duplicate returns same native registration, never offset zero");
  auto error=errors->take(x.f);check(error&&error->plugin==0&&error->code==0x80000017u,"actual duplicate high-bit error normalizes plugin to zero");
  bool refused=false;std::thread foreign([&]{RetailRendererCoreFailure f;
   auto r=renderer_required::register_object_extensions_1b72b0(*core,f);
   refused=!r&&f.kind==RetailRendererCoreError::wrong_thread&&!definition->observe(f)&&f.kind==RetailRendererCoreError::wrong_thread;
  });foreign.join();check(refused,"actual Core and metadata wrong-thread refusal");
  renderer_required::discard_object_extensions_after_core_init(*core);
  check(!definition->observe(x.f)&&x.f.kind==RetailRendererCoreError::owner&&!core->raster_extension(x.f),
        "native retirement invalidates retained definition before owner storage release");
  renderer_required::discard_object_extensions_after_core_init(*core);
 }
 {
  Fixture x(root);auto core=x.core();
  check(bool(core->register_factory({0x777,0},{reject_constructor,never_destroy},x.f)),"explicit first construction fixture");
  auto initialized=core->initialize_core(x.f);check(initialized&&*initialized,"actual complete CoreInit registration");
  const auto definition=core->raster_extension(x.f);
  check(!core->construct_registered(x.f)&&callback_refused&&core->raster_extension(x.f)==definition,
        "callout cannot mutate sealed real metadata; failed constructor is not destroyed");
  auto closed=renderer_required::register_object_extensions_1b72b0(*core,x.f);
  check(!closed&&x.f.kind==RetailRendererCoreError::registration_closed,"registration stays sealed after construction attempt");
 }
 {
  bool completed=false;
  for(int cut=0;cut<16;++cut){
   Fixture x(root);auto core=x.core();
   check(bool(core->register_factory({0x777,0},{reject_constructor,never_destroy},x.f)),"unrelated metadata retained before cut");
   fail_after=cut;auto registered=renderer_required::register_object_extensions_1b72b0(*core,x.f);fail_after=-1;
   if(registered){check(*registered&&bool(core->raster_extension(x.f)),"actual successful cut endpoint");completed=true;break;}
   ++allocation_cuts;
   check(x.f.kind==RetailRendererCoreError::allocator||x.f.kind==RetailRendererCoreError::allocation,
         "real host allocation refusal remains named");
   check(!core->raster_extension(x.f)&&core->registered(x.f)->size()==1,"failed registration publishes no identity and preserves unrelated entry");
   auto retry=renderer_required::register_object_extensions_1b72b0(*core,x.f);
   check(retry&&*retry&&bool(core->raster_extension(x.f)),"real allocation retry after complete cleanup");
   renderer_required::discard_object_extensions_after_core_init(*core);
  }
  check(completed&&allocation_cuts>0,"all actual registration allocation cuts reached a real successful endpoint");
 }
 {
  std::shared_ptr<const NativeRetailRendererRasterExtensionRegistration> held;
  std::weak_ptr<NativeRetailGameSession> session;
  {Fixture x(root);auto core=x.core();session=x.startup.game_session;
   auto initialized=core->initialize_core(x.f);check(initialized&&*initialized,"actual lifetime CoreInit");
   held=core->raster_extension(x.f);check(bool(held),"retained native registration handle");
  }
  RetailRendererCoreFailure f;
  check(session.expired()&&!held->observe(f)&&f.kind==RetailRendererCoreError::owner,
        "retained metadata cannot retain or impersonate an expired Session/Core");
 }
 std::printf("PASS_NATIVE_RASTER_REGISTRATION; actual Main/CoreInit tail, ID040C/source-null callbacks, duplicate/sealed/thread/retirement/Session/allocation cuts=%u; suffix refused; no window/audio/device/game; acceptance OPEN\n",allocation_cuts);
 return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL native raster registration: %s\n",e.what());return 1;}
