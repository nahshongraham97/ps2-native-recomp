#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
namespace mka::retail {
namespace {
NativeRetailRenderCameras* domain(NativeRetailMainState& main,StartupFailure& f){
 f={};if(!main.lifetime||!main.lifetime->active_on_owner_thread()||!main.startup.game_session){
  f={StartupError::game_session,"native camera bootstrap actual Main lifetime/Session"};return nullptr;}
 auto* result=main.startup.game_session->render_cameras(f.game);
 if(!result){f.kind=StartupError::game_session;f.operation="native camera bootstrap Session thread";}return result;
}
bool result(bool okay,const RetailCameraWorldFailure& detail,StartupFailure& f){
 if(okay)return true;f.kind=detail.kind==RetailCameraWorldError::allocation?StartupError::allocation:StartupError::game_session;
 f.operation=detail.operation;return false;
}
}
namespace main_binding {
RetailStartupGate initialize_retail_first_scene(NativeRetailMainState& main,StartupFailure& f){
 auto* cameras=domain(main,f);if(!cameras)return RetailStartupGate::native_failure;RetailCameraWorldFailure cf;
 if(!result(cameras->store_initial_count_from_127260(cf),cf,f))return RetailStartupGate::native_failure;
 return scene_bootstrap_required::continue_first_scene_at_220900(main,f);
}
}
namespace scene_bootstrap_required {
bool continue_scene_reset_at_181a30(NativeRetailMainState& main,StartupFailure& f){
 auto* cameras=domain(main,f);if(!cameras)return false;RetailCameraWorldFailure cf;
 if(!result(cameras->detach_cameras_from_181a30(cf),cf,f))return false;
 return continue_scene_reset_at_125d60(main,f);
}
}
}
