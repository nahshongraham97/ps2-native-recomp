#include "mka/retail_camera_world.hpp"
#include "mka/retail_game_session.hpp"
#include <new>
#include <stdexcept>
namespace mka::retail {
namespace {
bool session_access(const NativeRetailGameSession* session,RetailCameraWorldFailure& f) noexcept {
 f={};RetailGameSessionFailure sf;
 if(!session||!session->validate_current_thread(sf)){f={RetailCameraWorldError::session,"native camera/world Session thread/lifetime"};return false;}
 return true;
}
}
NativeRetailCameraWorldLink::NativeRetailCameraWorldLink(const std::shared_ptr<NativeRetailGameSession>& s) noexcept:session_(s),identity_(s.get()){}
bool NativeRetailCameraWorldLink::access(RetailCameraWorldFailure& f) const noexcept {
 const auto session=session_.lock();return session_access(session.get(),f);
}
std::shared_ptr<NativeRetailCameraWorldLink> NativeRetailCameraWorldLink::create_initial(
 const std::shared_ptr<NativeRetailGameSession>& session,RetailCameraWorldFailure& f) noexcept {
 if(!session_access(session.get(),f))return {};
 try{return std::shared_ptr<NativeRetailCameraWorldLink>(new NativeRetailCameraWorldLink(session));}
 catch(const std::bad_alloc&){f={RetailCameraWorldError::allocation,"native camera world-link allocation"};}
 catch(...){f={RetailCameraWorldError::native_exception,"native camera world-link allocation"};}return {};
}
bool NativeRetailCameraWorldLink::attach_after_frame_update_from_1cf300(
 std::shared_ptr<NativeRetailRenderWorld> world,RetailCameraWorldFailure& f) noexcept {
 if(!access(f))return false;
 if(world&&world->owner_session()!=identity_){f={RetailCameraWorldError::foreign_owner,"1CF300 foreign native world"};return false;}
 world_=world;world_published_=bool(world);return true;
}
std::optional<std::shared_ptr<NativeRetailRenderWorld>> NativeRetailCameraWorldLink::world_from_1cf380(RetailCameraWorldFailure& f) const noexcept {
 if(!access(f))return {};
 if(!world_published_)return std::shared_ptr<NativeRetailRenderWorld>{};
 auto world=world_.lock();if(!world){f={RetailCameraWorldError::expired_world,"1CF380 world producer retired without detaching camera"};return {};}
 return world;
}
std::optional<std::shared_ptr<NativeRetailRenderWorld>> NativeRetailCameraWorldLink::remove_from_1cf350(
 const std::shared_ptr<NativeRetailRenderWorld>& argument,RetailCameraWorldFailure& f) noexcept {
 if(!access(f))return {};
 if(argument&&argument->owner_session()!=identity_){f={RetailCameraWorldError::foreign_owner,"1CF350 foreign native argument"};return {};}
 if(!world_published_)return std::shared_ptr<NativeRetailRenderWorld>{};
 // Source checks attached world for nonzero, not equality with argument.
 world_.reset();world_published_=false;update_word_=0;return argument;
}
bool NativeRetailRenderCameras::access(RetailCameraWorldFailure& f) const noexcept {return session_access(session_,f);}
bool NativeRetailRenderCameras::store_count_from_127110(std::int32_t count,RetailCameraWorldFailure& f) noexcept {
 if(!access(f))return false;
 try {
  if(count>0&&static_cast<std::size_t>(count)>cameras_.size())cameras_.resize(static_cast<std::size_t>(count));
  count_=count;return true;
 }catch(const std::bad_alloc&){f={RetailCameraWorldError::allocation,"native camera record growth before count store"};}
 catch(const std::length_error&){f={RetailCameraWorldError::allocation,"native camera record host capacity"};}
 catch(...){f={RetailCameraWorldError::native_exception,"native camera record growth"};}return false;
}
bool NativeRetailRenderCameras::store_initial_count_from_127260(RetailCameraWorldFailure& f) noexcept {
 if(!store_count_from_127110(1,f))return false;admitted_=true;return true;
}
bool NativeRetailRenderCameras::publish_camera_link_after_180ed0(std::size_t index,
 std::shared_ptr<NativeRetailCameraWorldLink> camera,RetailCameraWorldFailure& f) noexcept {
 if(!access(f))return false;
 if(!admitted_){f={RetailCameraWorldError::missing_producer,"180ED0 requires actual127260 count producer"};return false;}
 if(count_<=0||index>=static_cast<std::size_t>(count_)||index>=cameras_.size()){f={RetailCameraWorldError::missing_record,"180ED0 record not admitted by actual count"};return false;}
 if(!camera||camera->identity_!=session_||!camera->access(f)){if(f.kind==RetailCameraWorldError::none)f={RetailCameraWorldError::foreign_owner,"180ED0 actual camera subowner missing/foreign"};return false;}
 cameras_[index].camera=camera;cameras_[index].published=true;return true;
}
bool NativeRetailRenderCameras::publish_world_after_186404(std::shared_ptr<NativeRetailRenderWorld> world,RetailCameraWorldFailure& f) noexcept {
 if(!access(f))return false;
 if(world&&world->owner_session()!=session_){f={RetailCameraWorldError::foreign_owner,"186404 actual world belongs to other Session"};return false;}
 world_=std::move(world);return true;
}
bool NativeRetailRenderCameras::detach_cameras_from_181a30(RetailCameraWorldFailure& f) noexcept {
 if(!access(f))return false;
 if(!admitted_){f={RetailCameraWorldError::missing_producer,"181A30 actual127260 camera count producer missing"};return false;}
 suspended_=true; //126720 before127100, including count<=0.
 //127100 reloads the signed count on each loop test; actual children are
 // direct plugin fields with no callback, iterator snapshot or task dispatch.
 for(std::int64_t index=0;index<count_;++index){
  const auto& record=cameras_[static_cast<std::size_t>(index)];
  if(!record.published||!world_)continue;
  const auto camera=record.camera.lock();
  if(!camera){f={RetailCameraWorldError::expired_camera,"181A30 actual camera producer retired without clearing record"};return false;}
  const auto attached=camera->world_from_1cf380(f);if(!attached)return false;
  if(*attached&&!camera->remove_from_1cf350(world_,f))return false;
 }
 return true;
}
std::optional<RetailRenderCameraObservation> NativeRetailRenderCameras::observe(RetailCameraWorldFailure& f) const noexcept {
 if(!access(f))return {};return RetailRenderCameraObservation{count_,admitted_,suspended_,bool(world_),cameras_.size()};
}
}
