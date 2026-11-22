#include "mka/retail_renderer_core.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
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
namespace {thread_local bool fail_new=false;thread_local unsigned new_attempts=0;}
void* operator new(std::size_t n){++new_attempts;if(fail_new)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void lifetime_probe(NativeRetailRendererCore&);bool lifetime_tail_probe{};int lifetime_action{};NativeRetailMainState* lifetime_main{};
unsigned tail_calls{},tail_cleanup_calls{},suffix_calls{},event_count{};int tail_mode{},suffix_mode{};
NativeRetailMainState* callback_transfer_main{};bool callback_native_exception{};
int events[256]{};unsigned failed_plugin{};bool allow_plugin_fixtures{},recursive_refused{};
void event(int value)noexcept{if(event_count>=256)std::abort();events[event_count++]=value;}
bool fixture_construct(unsigned id,NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!allow_plugin_fixtures)std::abort();event(static_cast<int>(id));
 if(id==0x401){RetailRendererCoreFailure nested;auto r=core.register_factory({123,0},{construct_native_renderer_error,destroy_native_renderer_error},nested);recursive_refused=!r&&nested.kind==RetailRendererCoreError::reentrant;}
 if(id==0x401&&callback_transfer_main)reenter_native_retail_main(*callback_transfer_main,23);
 if(id==0x401&&callback_native_exception)throw std::runtime_error("test-only native constructor exception");
 return id!=failed_plugin;
}
void fixture_destroy(unsigned id)noexcept{if(!allow_plugin_fixtures)std::abort();event(-static_cast<int>(id));}
struct Fixture {
 NativeRetailStartupState startup; mka::input::ControllerInput samples; NativeRetailMainState main;
 RetailRendererCoreFailure f;RetailRendererAllocatorFailure af;StartupFailure sf;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
 explicit Fixture(const std::filesystem::path& path):startup(open(path),{}),main(startup,samples){
  check(initialize_native_retail_game_session(startup,sf),"actual Session");check(initialize_retail_memory_owners(startup,sf),"actual memory owners");
  allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual Permanent allocator");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& p){mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(p,f);check(bool(v),"actual content root");return std::move(*v);}
 std::shared_ptr<NativeRetailRendererCore> core(){auto c=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);check(bool(c),f.operation);return c;}
};
NativeRetailStartupState* released_startup{};
std::shared_ptr<NativeRetailRendererCore>* released_core{};
std::weak_ptr<NativeRetailGameSession> probe_session;
bool observed_session_live{},observed_registry_live{};unsigned lifetime_destroyed{};
void lifetime_probe(NativeRetailRendererCore& core){
 if(!released_startup)return;
 released_startup->game_record_reset.reset();released_startup->game_session.reset();released_core->reset();
 observed_session_live=!probe_session.expired();RetailRendererCoreFailure f;observed_registry_live=bool(core.registered(f));
 released_startup=nullptr;
}
bool lifetime_plain_construct(NativeRetailRendererCore&,RetailRendererCoreFailure&){return true;}
bool lifetime_construct(NativeRetailRendererCore& core,RetailRendererCoreFailure&){
 lifetime_probe(core);if(lifetime_action==2)throw std::runtime_error("lifetime native exception fixture");
 if(lifetime_action==3)reenter_native_retail_main(*lifetime_main,41);return lifetime_action!=1;
}
void lifetime_destroy(NativeRetailRendererCore& core)noexcept{++lifetime_destroyed;lifetime_probe(core);}
}
namespace mka::retail::renderer_required {
bool construct_core_plugin_0401(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0401,c,f);}
void destroy_core_plugin_0401(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0401);}
bool construct_core_plugin_0402(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0402,c,f);}
void destroy_core_plugin_0402(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0402);}
bool construct_core_plugin_0403(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0403,c,f);}
void destroy_core_plugin_0403(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0403);}
bool construct_core_plugin_0404(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0404,c,f);}
void destroy_core_plugin_0404(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0404);}
bool construct_core_plugin_0405(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0405,c,f);}
void destroy_core_plugin_0405(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0405);}
bool construct_core_plugin_0406(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0406,c,f);}
void destroy_core_plugin_0406(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0406);}
bool construct_core_plugin_0407(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0407,c,f);}
void destroy_core_plugin_0407(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0407);}
bool construct_core_plugin_0408(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0408,c,f);}
void destroy_core_plugin_0408(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0408);}
bool construct_core_plugin_0409(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0409,c,f);}
void destroy_core_plugin_0409(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0409);}
bool construct_core_plugin_0412(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0412,c,f);}
void destroy_core_plugin_0412(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0412);}
bool construct_core_plugin_040a(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x040a,c,f);}
void destroy_core_plugin_040a(NativeRetailRendererCore&)noexcept{fixture_destroy(0x040a);}
bool construct_core_plugin_040b(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x040b,c,f);}
void destroy_core_plugin_040b(NativeRetailRendererCore&)noexcept{fixture_destroy(0x040b);}
void discard_object_extensions_after_core_init(NativeRetailRendererCore&)noexcept{++tail_cleanup_calls;}
std::optional<bool> register_object_extensions_1b72b0(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(lifetime_tail_probe){lifetime_probe(core);
  if(lifetime_action==2)throw std::runtime_error("lifetime setup exception fixture");
  if(lifetime_action==3)reenter_native_retail_main(*lifetime_main,41);
 }
 ++tail_calls; // Explicit ENTIRE1B72B0 fixture,not production object/stream policy.
 if(tail_mode==1)return false;
 if(tail_mode==2){f.kind=RetailRendererCoreError::construction;f.operation="test-only whole1B72B0 refusal";return {};}
 if(tail_mode==3)throw std::bad_alloc();
 if(tail_mode==4){auto nested=core.initialize_core(f);check(!nested&&f.kind==RetailRendererCoreError::reentrant,"initialization reentry guard");return false;}
 return true;
}
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState& main,std::shared_ptr<NativeRetailRendererCore> core,StartupFailure& f){
 ++suffix_calls;RetailRendererCoreFailure cf;const auto rows=core->registered(cf);
 check(rows&&rows->size()==14,"all14 actual core metadata registrations precede entire suffix fixture");
 RetailGameSessionFailure gf;RetailCameraWorldFailure wf;const auto camera=main.startup.game_session->render_cameras(gf)->observe(wf);
 check(camera&&camera->count==1,"actual sourcecount1 before CoreInit");
 if(suffix_mode==1){f={StartupError::game_session,"test-only entire225D40 suffix failure"};return RetailStartupGate::native_failure;}
 if(suffix_mode==2)reenter_native_retail_main(main,19);
 return RetailStartupGate::ready; // Fixture only;no EngineOpen/Start/game success.
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
static_assert(!std::is_copy_constructible_v<NativeRetailRendererCore>);
static_assert(!std::is_move_constructible_v<NativeRetailRendererCore>);
static_assert(!std::is_default_constructible_v<NativeRetailRendererCore>);
int main(int argc,char** argv)try{
 check(argc==2,"existing root for CPU-only components");const std::filesystem::path path(argv[1]);
 const bool expected_retained=true; // The actual Session must survive every callout.
 for(int scenario=0;scenario<6;++scenario){
  const int phase=scenario%3;const bool emergency_retained=scenario<3;
  Fixture diagnostic(path);auto original=diagnostic.core();auto emergency=original;auto* raw=original.get();
  if(phase!=1)check(bool(raw->register_factory({0x777,0},{lifetime_construct,lifetime_destroy},diagnostic.f)),"diagnostic callback registration");
  if(phase==2)check(raw->construct_registered(diagnostic.f),"diagnostic prefix before destroy");
  probe_session=diagnostic.startup.game_session;released_startup=&diagnostic.startup;released_core=&original;observed_session_live=observed_registry_live=false;lifetime_tail_probe=phase==1;const auto destroyed_before=lifetime_destroyed;const auto cleanup_before=tail_cleanup_calls;
  if(!emergency_retained)emergency.reset();
  if(phase==0)check(raw->construct_registered(diagnostic.f),"diagnostic construct completion");
  if(phase==1){auto initialized=raw->initialize_core(diagnostic.f);check(initialized&&*initialized,"diagnostic initialization completion");}
  if(phase==2)check(raw->destroy_constructed(diagnostic.f),"diagnostic destroy completion");
  check(observed_session_live==expected_retained&&observed_registry_live==expected_retained,"operation-long Session/registry observation");
  check(probe_session.expired(),"Session lease ends after operation");lifetime_tail_probe=false;emergency.reset();
  check(phase==1?tail_cleanup_calls==cleanup_before+1:lifetime_destroyed==destroyed_before+1,"one cleanup after last owner release");
 }
 // Last-owner release also covers refusal, ordinary exceptions and real Main transfer.
 for(int phase=0;phase<2;++phase)for(int action=1;action<=3;++action){
  if(phase==1&&action==1)continue;
  Fixture detached(path);auto original=detached.core();auto* raw=original.get();
  if(phase==0){check(bool(raw->register_factory({0x776,0},{lifetime_plain_construct,lifetime_destroy},detached.f)),"prefix lifetime registration");check(bool(raw->register_factory({0x777,0},{lifetime_construct,lifetime_destroy},detached.f)),"callout lifetime registration");}
  probe_session=detached.startup.game_session;released_startup=&detached.startup;released_core=&original;observed_session_live=observed_registry_live=false;
  lifetime_tail_probe=phase==1;lifetime_action=action;lifetime_main=&detached.main;const auto destroyed_before=lifetime_destroyed;const auto cleanup_before=tail_cleanup_calls;
  bool moved=false;try{if(phase==0)check(!raw->construct_registered(detached.f),"constructor failure result");else check(!raw->initialize_core(detached.f),"setup failure result");}
  catch(const detail::NativeMainReentry& e){moved=e.owner==&detached.main&&e.value==41;}
  check(observed_session_live&&observed_registry_live&&probe_session.expired(),"owner retained through failing callout then released");
  check(moved==(action==3),"actual Main transfer identity preserved");
  if(action==2)check(detached.f.kind==(phase==0?RetailRendererCoreError::after_effect_unknown:RetailRendererCoreError::native_exception),"ordinary exception remains distinct from transfer");
  if(action==3)check(detached.f.kind==RetailRendererCoreError::none,"Main transfer never poisons failure");
  check(phase==0?lifetime_destroyed==destroyed_before+1:tail_cleanup_calls==cleanup_before+1,"successful prefix/setup cleaned once on unwind");
  lifetime_tail_probe=false;lifetime_action=0;lifetime_main=nullptr;
 }
 Fixture x(path);auto core=x.core();check(core==x.core(),"Session persists same core identity");check(core->allocator_identity()==x.allocator->owner_identity(),"existing actual Permanent heap");
 const auto error_callbacks=RetailRendererPluginCallbacks{construct_native_renderer_error,destroy_native_renderer_error};
 check(*core->error_domain_count(x.f)==0,"source BSS modulecount before constructors");
 auto first=core->register_factory({0x40f,8},error_callbacks,x.f);check(bool(first),"actual errorfactory registration");
 auto duplicate=core->register_factory({0x40f,99},{},x.f);check(duplicate&&*duplicate==*first&&(*duplicate)->spec().source_payload_bytes==8,"duplicate keeps first real entry and factory");
 check(core->construct_registered(x.f)&&*core->error_domain_count(x.f)==1,"real errorconstructor invoked from native registry");
 auto error=core->construct_error_domain(x.f);check(error&&*core->error_domain_count(x.f)==2,"actual retained error-domain factory count");
 check(error->store({9,17},x.f)&&error->store({3,0x80000017u},x.f),"first-error stores");
 check(error->take(x.f)==RetailRendererErrorRecord{9,17},"first error retained then take resets");
 check(error->store({8,0x80000017u},x.f)&&error->take(x.f)==RetailRendererErrorRecord{0,0x80000017u},"highbit error forces pluginzero");
 check(error->store({0,0},x.f)&&error->store({4,7},x.f)&&error->take(x.f)==RetailRendererErrorRecord{0,0},"zero code is a stored error not empty sentinel");
 check(error->store({7,0x80000000u},x.f)&&error->store({4,7},x.f)&&error->take(x.f)==RetailRendererErrorRecord{4,7},"source sentinel remains empty after highbit sentinel store");
 check(!core->register_factory({0x40f,8},error_callbacks,x.f)&&x.f.kind==RetailRendererCoreError::registration_closed,"postconstruction registration refused before duplicate lookup");
 check(error->take(x.f)==RetailRendererErrorRecord{0,0x80000017u},"source registration diagnostic reaches actual error latch");
 check(core->destroy_constructed(x.f)&&*core->error_domain_count(x.f)==1,"source reverse teardown releases actual errorfactory instance");error.reset();check(*core->error_domain_count(x.f)==0,"external native error owner releases modulecount");
 auto file=core->open_resource("XBOX_WORD_JOIN.json",x.f);check(bool(file),"real existing retained content file service");char opening{};mka::content::ContentVolumeFailure read_failure;check(file->read(&opening,1,read_failure).bytes==1&&opening=='{',"native file reads actual pinned source record");check(!core->open_resource("missing-renderer-file.bin",x.f)&&x.f.kind==RetailRendererCoreError::content,"native missing file fails without fallback");
 auto public_volume=std::optional<mka::content::NativeContentVolume>(std::move(x.startup.content));public_volume.reset();check(bool(core->open_resource("XBOX_WORD_JOIN.json",x.f)),"same actual opened root retained without public volume");
 Fixture registration_failure(path);auto fc=registration_failure.core();auto original=fc->register_factory({0x40f,8},error_callbacks,registration_failure.f);check(bool(original),"original native entry before failure");
 fail_new=true;auto failed_entry=fc->register_factory({0x402,24},{renderer_required::construct_core_plugin_0402,renderer_required::destroy_core_plugin_0402},registration_failure.f);fail_new=false;check(!failed_entry&&registration_failure.f.kind==RetailRendererCoreError::allocator&&fc->registered(registration_failure.f)->size()==1,"entry allocation failure preserves prior source order");
 check(!fc->register_factory({0x402,24},{},registration_failure.f)&&registration_failure.f.kind==RetailRendererCoreError::unbound_factory,"missing native callbacks fail by name,not default success");
 Fixture initialization(path);auto ic=initialization.core();tail_mode=1;const auto cleanup_before=tail_cleanup_calls;auto refused=ic->initialize_core(initialization.f);check(refused&&!*refused&&ic->registered(initialization.f)->empty()&&tail_cleanup_calls==cleanup_before+1,"source whole-tail failure unwinds all core metadata");
 tail_mode=2;check(!ic->initialize_core(initialization.f)&&initialization.f.kind==RetailRendererCoreError::construction,"native whole-tail refusal remains named");
 tail_mode=3;check(!ic->initialize_core(initialization.f)&&initialization.f.kind==RetailRendererCoreError::allocation,"required tail allocation failure unwinds");
 tail_mode=4;auto rec=ic->initialize_core(initialization.f);check(rec&&!*rec,"recursive core initialization refused");
 tail_mode=0;const auto start_new=new_attempts;fail_new=true;auto allocation=ic->initialize_core(initialization.f);fail_new=false;
 check(!allocation&&new_attempts-start_new>=14&&ic->registered(initialization.f)->empty(),"all14 source registration attempts even after native allocation failure;no entries leak");
 check(ic->initialize_core(initialization.f)==true,"CoreInit reaches only explicit whole-tail fixture");
 constexpr RetailRendererPluginSpec expected[]{{0x40f,8},{0x401,24},{0x40d,0},{0x402,24},{0x403,4},{0x404,4},{0x405,4},{0x406,544},{0x407,100},{0x408,52},{0x409,96},{0x412,4},{0x40a,124},{0x40b,40}};
 const auto specs=ic->registered(initialization.f);check(specs&&specs->size()==14,"literal return1 no extra pluginrow");for(unsigned i=0;i<14;++i)check((*specs)[i]==expected[i],"independent original registration order/sizeIDs");
 const auto before_tail=tail_calls;check(ic->initialize_core(initialization.f)==false&&tail_calls==before_tail,"actual alreadyinitialized refusal persists");
 allow_plugin_fixtures=true;failed_plugin=0x403;event_count=0;check(!ic->construct_registered(initialization.f)&&*ic->error_domain_count(initialization.f)==0&&*ic->module_040d_count(initialization.f)==0,"source constructor failure reverses prior successes including actual errorfactory");
 const int expected_failure[]{0x401,0x402,0x403,-0x402,-0x401};check(event_count==5,"failed callback itself is not destroyed by source unwind");for(unsigned i=0;i<5;++i)check(events[i]==expected_failure[i],"forward construct/reverse unwind order");check(recursive_refused,"callback registry mutation refused");
 failed_plugin=0;event_count=0;check(ic->construct_registered(initialization.f)&&*ic->error_domain_count(initialization.f)==1&&*ic->module_040d_count(initialization.f)==1,"successful source callback protocol with explicitly fixture otherfamilies");check(ic->destroy_constructed(initialization.f)&&*ic->error_domain_count(initialization.f)==0&&*ic->module_040d_count(initialization.f)==0,"reverse explicit teardown");allow_plugin_fixtures=false;
 Fixture callback_transfer(path);auto tc=callback_transfer.core();tail_mode=0;check(tc->initialize_core(callback_transfer.f)==true,"native registry ready before callback Main transfer");allow_plugin_fixtures=true;callback_transfer_main=&callback_transfer.main;bool callback_transferred=false;
 try{tc->construct_registered(callback_transfer.f);}catch(const detail::NativeMainReentry& e){callback_transferred=e.owner==&callback_transfer.main&&e.value==23;}callback_transfer_main=nullptr;
 check(callback_transferred&&callback_transfer.f.kind==RetailRendererCoreError::none,"legitimate callback Main transfer propagates without named error");
 const auto inspected=tc->registered(callback_transfer.f);const auto prefix_count=tc->error_domain_count(callback_transfer.f);check(inspected&&inspected->size()==14&&prefix_count&&*prefix_count==1,"Main transfer leaves registry inspectable and real constructed prefix retained");
 check(tc->destroy_constructed(callback_transfer.f)&&*tc->error_domain_count(callback_transfer.f)==0,"Busy guard released and prior actual errorfactory prefix cleans up after Main transfer");
 check(tc->construct_registered(callback_transfer.f)&&tc->destroy_constructed(callback_transfer.f)&&*tc->error_domain_count(callback_transfer.f)==0,"registry can construct and release again after legitimate transfer cleanup");
 Fixture native_exception(path);auto ec=native_exception.core();check(ec->initialize_core(native_exception.f)==true,"separate actual registry before native constructor exception");callback_native_exception=true;
 check(!ec->construct_registered(native_exception.f)&&native_exception.f.kind==RetailRendererCoreError::after_effect_unknown,"ordinary native exception remains an error-only partial-effect latch");callback_native_exception=false;
 check(!ec->registered(native_exception.f)&&native_exception.f.kind==RetailRendererCoreError::after_effect_unknown&&!ec->destroy_constructed(native_exception.f),"native exception remains blocked rather than being mistaken for legitimate Main transfer");allow_plugin_fixtures=false;
 Fixture moved_content(path);auto removed_root=std::optional<mka::content::NativeContentVolume>(std::move(moved_content.startup.content));removed_root.reset();check(main_binding::initialize_retail_first_scene(moved_content.main,moved_content.sf)==RetailStartupGate::native_failure&&moved_content.sf.kind==StartupError::content&&moved_content.sf.content.kind==mka::content::ContentVolumeError::moved_volume,"connected content failure preserves actual cause after count producer");
 Fixture connected(path);tail_mode=0;suffix_mode=1;const auto before_suffix=suffix_calls;
 check(main_binding::initialize_retail_first_scene(connected.main,connected.sf)==RetailStartupGate::native_failure&&suffix_calls==before_suffix+1,"connected actual count/allocator/CoreInit then whole225D40 error fixture");
 check(main_binding::initialize_retail_first_scene(connected.main,connected.sf)==RetailStartupGate::source_refused&&suffix_calls==before_suffix+1,"coreinitialized state survives suffix failure and Main reentry");
 Fixture transfer(path);suffix_mode=2;bool transferred=false;try{main_binding::initialize_retail_first_scene(transfer.main,transfer.sf);}catch(const detail::NativeMainReentry& e){transferred=e.owner==&transfer.main&&e.value==19;}check(transferred,"actual Main transfer propagates after real CoreInit");
 check(main_binding::initialize_retail_first_scene(transfer.main,transfer.sf)==RetailStartupGate::source_refused,"persistent core source state after transfer");suffix_mode=0;
 bool wrong=false;std::thread worker([&]{RetailRendererCoreFailure f;wrong=!ic->registered(f)&&f.kind==RetailRendererCoreError::wrong_thread;});worker.join();check(wrong,"registry owner-thread admission");
 auto foreign_volume=Fixture::open(path);check(!NativeRetailRendererCore::for_session(initialization.startup.game_session,initialization.allocator,foreign_volume,initialization.f)&&initialization.f.kind==RetailRendererCoreError::owner,"same path reopened owner is not actual retained volume capability");
 auto old_session=initialization.startup.game_session;auto old_heap=initialization.allocator;
 Fixture other(path);check(!NativeRetailRendererCore::for_session(initialization.startup.game_session,other.allocator,initialization.startup.content,initialization.f),"foreign actual Permanent heap refused");
 std::weak_ptr<NativeRetailGameSession> retired=old_session;old_session.reset();initialization.startup.game_record_reset.reset();initialization.startup.game_session.reset();check(retired.expired(),"actual reset companion also released before Session destruction");check(!ic->registered(initialization.f)&&initialization.f.kind==RetailRendererCoreError::owner,"retained registry refuses after Session destruction");
 std::puts("PASS_NATIVE_RENDERER_REGISTRY_ERROR_DOMAIN_SOURCE; otherfactories/1B72B0/225D40 are explicit fixtures;no platform start");return 0;
}catch(const std::exception& e){fail_new=false;std::fprintf(stderr,"renderer core: %s\n",e.what());return 1;}
