#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>
// Actual imported platform initialization is forbidden in this CPU-only test.
extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }
using namespace mka::retail;
namespace {
void check(bool okay,const char* label) { if(!okay) throw std::runtime_error(label); }
unsigned calls{};
int effect{};
bool expected_fifty{};
void factors(const NativeRetailMainState& main,bool fifty) {
    check(std::bit_cast<std::uint32_t>(main.timing_factors[0])==(fifty?0x3f8c378cu:0x3f800000u),"122B4C root bits");
    check(std::bit_cast<std::uint32_t>(main.timing_factors[1])==(fifty?0x3f555555u:0x3f800000u),"122B38 inverse bits");
    check(std::bit_cast<std::uint32_t>(main.timing_factors[2])==(fifty?0x3f99999au:0x3f800000u),"122B40 scale bits");
}
}
namespace mka::retail::scene_bootstrap_required {
// Test-only suffix fixture: observes the real prefix, never claims scene reset.
bool continue_scene_reset_at_181a30(NativeRetailMainState& main,StartupFailure& f) {
    ++calls;
    factors(main,expected_fifty);
    RetailGameSessionFailure sf;
    check(main.startup.game_session->game_records_values(sf)->word_64e2a0==1,"1861C0 store precedes suffix");
    check(main.milliseconds_per_frame==17,"122AD0 does not write frame milliseconds");
    if(effect==1) { f={StartupError::game_session,"test-only181A30 failure"};return false; }
    if(effect==2) reenter_native_retail_main(main,7);
    return true;
}
}
int main(int argc,char** argv) { try {
    check(argc==2,"requires an existing content directory");
    mka::content::ContentVolumeFailure vf;
    auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),vf);
    check(bool(volume),"real native content owner");
    NativeRetailStartupState startup(std::move(*volume),{});
    StartupFailure f;
    check(initialize_native_retail_game_session(startup,f),"real Session");
    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup,samples);
    main.milliseconds_per_frame=17;
    RetailGameSessionFailure sf;
    const auto session=startup.game_session;
    auto settings=session->settings(sf);auto controls=session->controls(sf);
    RetailMainSettingsFailure tf;RetailGameControlsFailure cf;
    auto* values=session->game_records_values(sf);
    check(main_binding::initialize_retail_post_scene_timing(main,f)&&calls==1,"production prefix actual call");factors(main,false);
    check(settings->store_video_rate_from_220460(0,tf),"actual50Hz producer");
    expected_fifty=true;
    values->word_64e2a0=99;
    check(main_binding::initialize_retail_post_scene_timing(main,f)&&calls==2,"repeat prefix source store");factors(main,true);
    check(settings->store_override_word_from_source(9,tf)&&controls->store_selection_flags_from_source(1,cf),"actual override owners");
    expected_fifty=false;
    check(main_binding::initialize_retail_post_scene_timing(main,f)&&calls==3,"override branch");factors(main,false);
    check(controls->store_selection_flags_from_source(2,cf),"other flags do not override");
    expected_fifty=true;
    effect=1;
    check(!main_binding::initialize_retail_post_scene_timing(main,f)&&calls==4&&values->word_64e2a0==1,"after-effect error preserves phase");factors(main,true);
    effect=2;bool transfer=false;
    try { main_binding::initialize_retail_post_scene_timing(main,f); }
    catch(const detail::NativeMainReentry& r) {transfer=r.owner==&main&&r.value==7;}
    check(transfer&&calls==5&&values->word_64e2a0==1,"actual native Main reentry propagates");
    effect=0;check(main_binding::initialize_retail_post_scene_timing(main,f)&&calls==6,"reentry does not poison next source entry");
    startup.game_session.reset();values->word_64e2a0=77;
    check(!main_binding::initialize_retail_post_scene_timing(main,f)&&calls==6&&values->word_64e2a0==77,"missing Session no store/call");
    startup.game_session=session;
    bool refused=false;std::thread other([&]{ StartupFailure local;refused=!main_binding::initialize_retail_post_scene_timing(main,local); });other.join();
    check(refused&&calls==6&&values->word_64e2a0==77,"wrong thread no store/call");
    main.lifetime->retire();
    check(!main_binding::initialize_retail_post_scene_timing(main,f)&&calls==6&&values->word_64e2a0==77,"retired lifetime no store/call");
    std::puts("PASS production1861B0 prefix: source phase/timing, real Session, errors, thread, lifetime, reentry; suffix fixture only");return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;} }
