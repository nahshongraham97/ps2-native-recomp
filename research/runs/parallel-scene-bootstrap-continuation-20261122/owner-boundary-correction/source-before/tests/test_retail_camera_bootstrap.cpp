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
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local bool fail_new=false;}
void* operator new(std::size_t size){if(fail_new)throw std::bad_alloc();if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
unsigned first_calls{},suffix_calls{};int effect{};
// Explicit test-only world producer, NOT an implementation of1CD6E0.
struct WorldFixture final:NativeRetailRenderWorld {
 explicit WorldFixture(const std::shared_ptr<NativeRetailGameSession>& s):NativeRetailRenderWorld(s){}
};
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_220900(NativeRetailMainState& main,StartupFailure& f){
 ++first_calls;RetailGameSessionFailure sf;RetailCameraWorldFailure cf;
 auto observed=main.startup.game_session->render_cameras(sf)->observe(cf);
 check(observed&&observed->count==1&&observed->count_producer_admitted,"27270 count producer precedes220900");
 if(effect==1){f={StartupError::game_session,"test-only220900 failure"};return RetailStartupGate::native_failure;}
 if(effect==2)reenter_native_retail_main(main,9);if(effect==3)return RetailStartupGate::source_refused;return RetailStartupGate::ready;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState& main,StartupFailure& f){
 ++suffix_calls;RetailGameSessionFailure sf;RetailCameraWorldFailure cf;
 check(main.startup.game_session->render_cameras(sf)->observe(cf)->render_suspended,"126720 precedes125D60");
 check(main.startup.game_session->game_records_values(sf)->word_64e2a0==1,"1861C0 phase retained through actual181A30");
 if(effect==1){f={StartupError::game_session,"test-only125D60 failure"};return false;}
 if(effect==2)reenter_native_retail_main(main,11);return true;
}
}
int main(int argc,char** argv){try{
 check(argc==2,"expected existing content root");mka::content::ContentVolumeFailure vf;
 auto content=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),vf);check(bool(content),"real native content");
 NativeRetailStartupState startup(std::move(*content),{});StartupFailure f;
 check(initialize_native_retail_game_session(startup,f),"actual Session");auto session=startup.game_session;
 mka::input::ControllerInput input;NativeRetailMainState main(startup,input);
 RetailGameSessionFailure sf;RetailCameraWorldFailure cf;auto* cameras=session->render_cameras(sf);
 check(!main_binding::initialize_retail_post_scene_timing(main,f)&&suffix_calls==0&&!cameras->observe(cf)->render_suspended,"born empty domain cannot pass181A30 without actual count producer");
 fail_new=true;bool failed=main_binding::initialize_retail_first_scene(main,f)==RetailStartupGate::native_failure;fail_new=false;
 check(failed&&f.kind==StartupError::allocation&&first_calls==0&&!cameras->observe(cf)->count_producer_admitted,"host allocation refuses before count publication/suffix");
 effect=1;check(main_binding::initialize_retail_first_scene(main,f)==RetailStartupGate::native_failure&&first_calls==1&&cameras->observe(cf)->count==1,"first-scene after-effect error retains actual count1");
 effect=2;bool transfer=false;try{main_binding::initialize_retail_first_scene(main,f);}catch(const detail::NativeMainReentry& x){transfer=x.owner==&main&&x.value==9;}
 check(transfer&&first_calls==2,"first-scene real Main reentry unchanged");effect=0;
 check(main_binding::initialize_retail_first_scene(main,f)==RetailStartupGate::ready&&first_calls==3,"fixture completes only required first-scene remainder");
 effect=3;check(main_binding::initialize_retail_first_scene(main,f)==RetailStartupGate::source_refused&&f.kind==StartupError::none&&cameras->observe(cf)->count==1,"source refusal gate remains distinct after actual count store");effect=0;
 check(main_binding::initialize_retail_post_scene_timing(main,f)&&suffix_calls==1,"source-proved initial null camera record passes after actual count producer");
 check(cameras->store_count_from_127110(6,cf),"game count grows beyond original four records");
 auto a=NativeRetailCameraWorldLink::create_initial(session,cf),b=NativeRetailCameraWorldLink::create_initial(session,cf),c=NativeRetailCameraWorldLink::create_initial(session,cf);
 check(a&&b&&c,"actual typed camera-link subowners; no dummy cameras");
 auto world=std::make_shared<WorldFixture>(session),other=std::make_shared<WorldFixture>(session);
 check(a->attach_after_frame_update_from_1cf300(world,cf)&&b->attach_after_frame_update_from_1cf300(other,cf),"test-only frame-update producers precede final native world stores");
 check(cameras->publish_camera_link_after_180ed0(0,a,cf)&&cameras->publish_camera_link_after_180ed0(4,b,cf)&&cameras->publish_camera_link_after_180ed0(5,c,cf),"real retained record projections including count>4 and null slots");
 check(main_binding::initialize_retail_post_scene_timing(main,f)&&*a->world_from_1cf380(cf)==world,"null current world skips attached camera");
 check(cameras->publish_world_after_186404(world,cf),"explicit test-only completed world producer");
 effect=1;check(!main_binding::initialize_retail_post_scene_timing(main,f)&&!(*a->world_from_1cf380(cf))&&!(*b->world_from_1cf380(cf)),"full181A30 removes both matching/different world links before failed125D60");
 check(cameras->observe(cf)->count==6&&cameras->observe(cf)->retained_records==6&&a->observed_update_word()==0,"detach preserves records/count and clears plugin update word");
 check(a->attach_after_frame_update_from_1cf300(world,cf),"reattach fixture after real frame-update boundary");
 auto returned=a->remove_from_1cf350(other,cf);check(returned&&*returned==other&&!(*a->world_from_1cf380(cf)),"1CF350 returns supplied world and never checks world equality");
 check(cameras->store_count_from_127110(-1,cf),"signed negative count retained");effect=0;
 check(main_binding::initialize_retail_post_scene_timing(main,f)&&cameras->observe(cf)->count==-1,"BLEZ negative skips records after flag store");
 check(cameras->store_count_from_127110(0,cf)&&main_binding::initialize_retail_post_scene_timing(main,f),"BLEZ zero path");
 check(cameras->store_count_from_127110(6,cf)&&a->attach_after_frame_update_from_1cf300(world,cf),"restore source count and actual link");
 effect=2;transfer=false;try{main_binding::initialize_retail_post_scene_timing(main,f);}catch(const detail::NativeMainReentry& x){transfer=x.owner==&main&&x.value==11;}
 check(transfer&&!(*a->world_from_1cf380(cf)),"source detach persists across actual Main reentry");effect=0;
 check(main_binding::initialize_retail_post_scene_timing(main,f),"later source entry remains usable");
 auto foreign=NativeRetailGameSession::create(sf);auto foreign_link=NativeRetailCameraWorldLink::create_initial(foreign,cf);
 check(!cameras->publish_camera_link_after_180ed0(0,foreign_link,cf),"foreign camera subowner refused");
 check(!a->attach_after_frame_update_from_1cf300(std::make_shared<WorldFixture>(foreign),cf),"foreign world refused");
 auto temporary=std::make_shared<WorldFixture>(session);check(c->attach_after_frame_update_from_1cf300(temporary,cf),"temporary fixture world");temporary.reset();
 check(!c->world_from_1cf380(cf)&&cf.kind==RetailCameraWorldError::expired_world,"expired published world is named failure, never empty success");
 auto old_suffix_calls=suffix_calls;
 check(!main_binding::initialize_retail_post_scene_timing(main,f)&&suffix_calls==old_suffix_calls&&cameras->observe(cf)->render_suspended,"expired real world blocks suffix after source prefix effects");
 // Clear a known source association through actual detach, not a fake producer.
 check(bool(c->remove_from_1cf350(world,cf)),"source pointer clear releases stale weak association safely");
 auto abandoned=NativeRetailCameraWorldLink::create_initial(session,cf);
 check(cameras->publish_camera_link_after_180ed0(1,abandoned,cf),"fixture completed camera published for lifetime check");abandoned.reset();
 old_suffix_calls=suffix_calls;
 check(!main_binding::initialize_retail_post_scene_timing(main,f)&&suffix_calls==old_suffix_calls,"expired published camera blocks181A30 instead of becoming a null record");
 auto replacement=NativeRetailCameraWorldLink::create_initial(session,cf);
 check(cameras->publish_camera_link_after_180ed0(1,replacement,cf)&&main_binding::initialize_retail_post_scene_timing(main,f),"later actual producer can replace stale projection without poisoning Session");
 const auto calls=suffix_calls;bool refused=false;std::thread worker([&]{StartupFailure other_failure;refused=!main_binding::initialize_retail_post_scene_timing(main,other_failure);});worker.join();
 check(refused&&suffix_calls==calls,"foreign thread before native effects");
 startup.game_session.reset();check(!main_binding::initialize_retail_post_scene_timing(main,f)&&suffix_calls==calls,"missing actual Session no suffix");startup.game_session=session;
 main.lifetime->retire();check(!main_binding::initialize_retail_post_scene_timing(main,f)&&suffix_calls==calls,"retired Main before native effects");
 NativeRetailMainState next(startup,input);check(next.startup.game_session->render_cameras(sf)==cameras&&cameras->observe(cf)->count==6,"actual camera domain survives Main recreation");
 auto orphan_world=std::make_shared<WorldFixture>(foreign);foreign.reset();check(!orphan_world->owner_session(),"world identity cannot authenticate expired Session");check(!foreign_link->world_from_1cf380(cf),"camera subowner cannot outlive Session access");
 std::puts("PASS actual Session camera bootstrap:127260/181A30 order, admitted/null/populated/count>4, world links, allocation/error/reentry/thread/lifetime; frame/raster/world/task/remainders fixture-only");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
