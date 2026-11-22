#include "mka/retail_renderer_allocator.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local bool fail_new=false;}
void* operator new(std::size_t n){if(fail_new)throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
unsigned calls{};int effect{};
std::shared_ptr<NativeRetailRendererAllocatorService> retained;
void bytes(const mka::memory::NativeAllocation& a,std::size_t n,unsigned start){auto* p=static_cast<const unsigned char*>(a.data());for(std::size_t i=0;i<n;++i)check(p[i]==static_cast<unsigned char>(start+i),"source realloc prefix bytes");}
}
static_assert(!std::is_copy_constructible_v<NativeRetailRendererAllocatorService>);
static_assert(!std::is_move_constructible_v<NativeRetailRendererAllocatorService>);
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_1a6dd0(NativeRetailMainState& main,
 std::shared_ptr<NativeRetailRendererAllocatorService> service,StartupFailure& f){
 ++calls;RetailGameSessionFailure gf;RetailCameraWorldFailure cf;RetailMemoryOwnerFailure mf;
 auto* cameras=main.startup.game_session->render_cameras(gf);auto observed=cameras->observe(cf);
 const auto heap=main.startup.memory_owners->observe(RetailMemoryRole::permanent,mf);
 check(observed&&observed->count==1&&observed->count_producer_admitted,"127270 actual count1 before allocator consumer");
 check(service&&heap&&service->owner_identity()==heap->heap.identity,"actual existing Permanent capability supplied to renderer suffix");
 retained=std::move(service);
 if(effect==1){f={StartupError::game_session,"test-only complete renderer remainder refusal"};return RetailStartupGate::native_failure;}
 if(effect==2)reenter_native_retail_main(main,13);
 if(effect==3)return RetailStartupGate::source_refused;
 return RetailStartupGate::ready; // Explicit fixture,not a renderer implementation.
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
int main(int argc,char** argv)try{
 check(argc==2,"requires existing content root for CPU-only Main owner");
 mka::content::ContentVolumeFailure vf;auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),vf);check(bool(volume),"actual native volume");
 NativeRetailStartupState startup(std::move(*volume),{});StartupFailure sf;
 check(initialize_native_retail_game_session(startup,sf),"actual GameSession");mka::input::ControllerInput input;NativeRetailMainState main(startup,input);
 check(scene_bootstrap_required::continue_first_scene_at_220900(main,sf)==RetailStartupGate::native_failure&&calls==0,"unadmitted camera domain cannot enter source continuation");
 check(main_binding::initialize_retail_first_scene(main,sf)==RetailStartupGate::native_failure&&sf.kind==StartupError::memory_owners&&calls==0,"missing actual memory owner refuses after count producer");
 check(initialize_retail_memory_owners(startup,sf),"actual native memory group");
 fail_new=true;bool allocation_failed=main_binding::initialize_retail_first_scene(main,sf)==RetailStartupGate::native_failure;fail_new=false;
 check(allocation_failed&&sf.kind==StartupError::allocation&&calls==0,"native service allocation failure before renderer suffix");
 effect=1;check(main_binding::initialize_retail_first_scene(main,sf)==RetailStartupGate::native_failure&&calls==1,"after-effect renderer error preserved");
 effect=2;bool reentry=false;try{main_binding::initialize_retail_first_scene(main,sf);}catch(const detail::NativeMainReentry& r){reentry=r.owner==&main&&r.value==13;}check(reentry&&calls==2,"actual Main reentry propagates");
 effect=3;check(main_binding::initialize_retail_first_scene(main,sf)==RetailStartupGate::source_refused&&sf.kind==StartupError::none,"source refusal remains distinct");
 effect=0;check(main_binding::initialize_retail_first_scene(main,sf)==RetailStartupGate::ready&&calls==4,"next entry remains usable;only renderer suffix fixture completes");
 RetailRendererAllocatorFailure f;RetailMemoryOwnerFailure mf;auto heap=startup.memory_owners->observe(RetailMemoryRole::permanent,mf);check(heap&&retained->owner_identity()==heap->heap.identity,"shared existing heap identity");
 auto zero=retained->allocate(0,f);check(zero&&*zero&&zero->requested_size()==0&&zero->rounded_size()==16,"source backend zero request has real minimum payload");
 std::memset(zero->data(),0x55,zero->rounded_size());check(retained->reallocate_in_place(*zero,12,f),"zero-request payload reallocates by actual old capacity");for(unsigned n=0;n<12;++n)check(static_cast<unsigned char*>(zero->data())[n]==0x55,"old native capacity copied");check(retained->release(*zero,f),"zero-origin actual release");
 mka::memory::NativeAllocation absent;check(retained->release(absent,f),"source freeNULL no effect");check(retained->reallocate_in_place(absent,0,f)&&absent&&absent.rounded_size()==16,"reallocNULL0 takes allocate branch");check(retained->reallocate_in_place(absent,0,f)&&!absent,"realloc nonnull0 takes free branch");
 auto a=retained->allocate(17,f);check(a&&a->requested_owner()==heap->heap.identity&&a->actual_owner()==heap->heap.identity&&a->rounded_size()==32,"ordinary native Permanent allocation");
 for(unsigned n=0;n<32;++n)static_cast<unsigned char*>(a->data())[n]=static_cast<unsigned char>(40+n);
 check(retained->reallocate_in_place(*a,60,f)&&a->requested_size()==60,"grow actual allocation");bytes(*a,32,40);
 check(retained->reallocate_in_place(*a,5,f)&&a->requested_size()==5,"shrink actual allocation");bytes(*a,5,40);
 const auto pointer=a->data();fail_new=true;bool failed=!retained->reallocate_in_place(*a,80,f);fail_new=false;check(failed&&a->data()==pointer&&a->requested_size()==5,"failed replacement retains old allocation");bytes(*a,5,40);
 check(!retained->reallocate_in_place(*a,std::numeric_limits<std::size_t>::max(),f)&&a->data()==pointer,"host size overflow retains old allocation");bytes(*a,5,40);
 auto foreign=startup.memory_owners->allocate_ordinary(RetailMemoryRole::section,23,mf);check(bool(foreign),"real different old owner");for(unsigned n=0;n<23;++n)static_cast<unsigned char*>(foreign->data())[n]=static_cast<unsigned char>(90+n);
 check(retained->reallocate_in_place(*foreign,48,f)&&foreign->actual_owner()==heap->heap.identity,"source realloc migrates old actual owner to Permanent");bytes(*foreign,23,90);
 auto cleared=retained->allocate_zeroed_source_words(3,7,f);check(cleared&&cleared->requested_size()==32,"source calloc product rounded16");for(unsigned n=0;n<32;++n)check(static_cast<unsigned char*>(cleared->data())[n]==0,"complete rounded source payload cleared");
 for(const auto count:{0u,0x10000u,0x80000000u,0xffffffffu}){
  const auto stride=count==0xffffffffu?1u:count==0x10000u?0x10000u:2u;
  auto wrap=retained->allocate_zeroed_source_words(count,stride,f);check(wrap&&wrap->requested_size()==0&&wrap->rounded_size()==16,"original32-bit calloc product/round boundary");
 }
 auto low=retained->allocate_zeroed_source_words(0x80000001u,2,f);check(low&&low->requested_size()==16,"low32 product2 rounds16");
 auto large=retained->allocate(2*1024*1024+3,f);check(large&&large->requested_size()>0x100000,"native request exceeds original renderer arena ceiling");
 const auto before=calls;bool refused=false;std::thread wrong([&]{StartupFailure f;refused=scene_bootstrap_required::continue_first_scene_at_220900(main,f)==RetailStartupGate::native_failure;});wrong.join();check(refused&&calls==before,"Main continuation thread admission before effects");
 auto session=startup.game_session;startup.game_session.reset();check(scene_bootstrap_required::continue_first_scene_at_220900(main,sf)==RetailStartupGate::native_failure&&calls==before,"missing canonical Session refuses");startup.game_session=session;
 main.lifetime->retire();check(scene_bootstrap_required::continue_first_scene_at_220900(main,sf)==RetailStartupGate::native_failure&&calls==before,"retired Main refuses new renderer continuation");
 // Heap capability has its own native lifetime;it never borrows the Main/group.
 auto group=NativeRetailMemoryOwners::create(mf);check(bool(group),"second actual isolated memory group for lifetime fixture");auto independent=NativeRetailRendererAllocatorService::create(*group,f);check(bool(independent),"retained actual heap capability");const auto identity=independent->owner_identity();group.reset();bool worker_ok=false;std::thread worker([&]{RetailRendererAllocatorFailure wf;auto allocation=independent->allocate(73,wf);worker_ok=allocation&&allocation->requested_owner()==identity&&independent->release(*allocation,wf);});worker.join();check(worker_ok,"real heap capability survives group handle and works on native worker");
 check(retained->release(*a,f)&&retained->release(*foreign,f)&&retained->release(*cleared,f)&&retained->release(*large,f)&&retained->release(*low,f),"actual allocations retire without callback tokens");retained.reset();
 std::puts("PASS connected220900 native allocator: actual Permanent ownership,source calloc/realloc,host growth,error/reentry/lifetime;entire renderer1A6DD0 suffix fixture-only");return 0;
}catch(const std::exception& e){fail_new=false;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
