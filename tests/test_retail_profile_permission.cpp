#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}}
// Other genuinely required menu descendants are guards, not implementations.
namespace mka::retail {
bool invoke_retail_profile_select_from_2f1630(NativeRetailMainState&,std::string_view,RetailProfileMenuFailure&){std::abort();}
bool publish_retail_profile_participant_from_189690(NativeRetailMainState&,const std::shared_ptr<NativeRetailInputParticipant>&,RetailProfileMenuFailure&){std::abort();}
bool apply_retail_profile_participant_from_188720(NativeRetailMainState&,std::uint32_t,RetailProfileMenuFailure&){std::abort();}
}
int main(int argc,char** argv)try{
 check(argc==2,"exclusive content fixture path");const auto root=std::filesystem::absolute(argv[1]);check(!std::filesystem::exists(root),"new fixture only");std::filesystem::create_directories(root);
 mka::content::ContentVolumeFailure cf;auto volume=mka::content::NativeContentVolume::open(root,cf);check(bool(volume),"real native content owner");
 NativeRetailStartupState startup(std::move(*volume),{});StartupFailure sf;check(initialize_native_retail_game_session(startup,sf),"actual Session birth");
 mka::input::ControllerInput input;NativeRetailMainState main(startup,input);RetailGameSessionFailure gf;auto controls=startup.game_session->controls(gf);auto* records=startup.game_session->saved_profile_records(gf);RetailSavedProfileRecordsFailure rf;RetailProfileMenuFailure f;
 check(controls&&records,"one actual Session controls/profile owner");
 for(unsigned p=0;p<2;++p){auto* active=records->active_player(p,rf);check(active&&active->value.phase()==RetailSavedChildPhase::zero_born,"actual BSS active child");check(observe_retail_profile_permission_from_4eee60(main,p,f)==1u,"BSS customization bit0clear returns1");}
 check(decide_retail_profile_entry_from_1892d0(main,62,0,0,f)==1u,"actual connected key62 returns before unborn aggregate/task owners");
 check(!observe_retail_profile_permission_from_4eee60(main,2,f)&&f.kind==RetailProfileMenuError::invalid_role,"invalid245BD0 selector refuses null-dereference route");
 RetailGameControlsFailure ctl;
 for(unsigned flags:{0u,1u,2u,8u,128u,255u}){check(controls->store_selection_flags_from_source(static_cast<std::uint8_t>(flags),ctl),"actual byte fixture");check(observe_retail_profile_permission_from_4eee60(main,0,f)==1u,"exact source selection bits with BSS bit0clear");if(flags&4u)check(observe_retail_profile_permission_from_4eee60(main,0xffffffffu,f)==1u,"bit2 early override skips invalid source pointer selector");else check(!observe_retail_profile_permission_from_4eee60(main,0xffffffffu,f),"unrelated flag bits do not bypass selector");}
 check(controls->store_selection_flags_from_source(0,ctl),"clear source flags");auto* active=records->active_player(0,rf);RetailSavedProfileChildFailure child;std::uint32_t working=71;check(active->value.reset_child_body_from_245dd0(0,working,child),"actual typed active reset");check(active->value.customization_flags()==4&&observe_retail_profile_permission_from_4eee60(main,0,f)==1u,"customizationbit2 is distinct from globalbit2 and permissionbit0");
 std::vector<std::uint8_t> prefix(retail_profile_image_prefix_size),encoded(retail_profile_child_image_size),tail(retail_profile_image_tail_size);RetailProfileImageEncodedParts parts{prefix,{},tail};for(auto& c:parts.children)c=encoded;RetailProfileImageFailure imf;auto image=NativeRetailProfileImage::encode_card_write_222da0(parts,RetailProfileImageFooterMode::ordinary_checksum,imf);check(bool(image),"encoded external file fixture");check(!active->value.import_from_image(*image,0,child),"unknown loaded projection remains required");check(!observe_retail_profile_permission_from_4eee60(main,0,f)&&f.kind==RetailProfileMenuError::required_child,"blocked imported child never becomes previous default success");check(controls->store_selection_flags_from_source(4,ctl)&&observe_retail_profile_permission_from_4eee60(main,0,f)==1u,"source override precedes blocked child observation");
 check(controls->store_selection_flags_from_source(0,ctl),"clear override");auto retained=startup.game_session;startup.game_session.reset();check(!observe_retail_profile_permission_from_4eee60(main,0,f)&&f.kind==RetailProfileMenuError::session,"missing actual Session fails without scalar fallback");startup.game_session=retained;
 bool refused=false;std::thread t([&]{RetailProfileMenuFailure q;refused=!observe_retail_profile_permission_from_4eee60(main,0,q)&&q.kind==RetailProfileMenuError::main_lifetime;});t.join();check(refused,"wrong thread refuses before native reads");
 std::puts("PASS real4EEE60 admitted BSS/default,global-bit2 order,unknown-import refusal,actual1892D0 caller and owner errors;populated task continuation uninjected;no endpoint/game acceptance");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"permission: %s\n",e.what());return 1;}
