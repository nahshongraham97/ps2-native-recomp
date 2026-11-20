#include "native_logon.hpp"
#include <cmath>
#include <exception>
#include <new>
#include <stdexcept>
#include <type_traits>
namespace mka::native_logon {
namespace {
struct Guard {bool& active;explicit Guard(bool& b):active(b){active=true;}~Guard(){active=false;}};
bool fail(Failure& f,Error e,const char* op){f.kind=e;f.operation=op;return false;}
}
struct NativeLogonState::SkipLease {
    input::NativeGameActionInput* actions;
    const input::ControllerInput* host;
    bool active=true;
};
NativeLogonState::NativeLogonState(video::NativeMovieService& s,input::NativeGameActionInput& a,
    const input::ControllerInput& h,input::NativeGameInputTasks& tasks,Settings v,Effects e):service_(s),actions_(a),host_(h),tasks_(tasks),settings_(v),effects_(std::move(e)){}
NativeLogonState::~NativeLogonState(){
    if(std::this_thread::get_id()!=owner_||in_call_)std::terminate();
    in_call_=true;Failure ignored;invalidate();close_owned(ignored);restore_surround(ignored);
}
bool NativeLogonState::access(Failure& f)const noexcept {
    f={};if(std::this_thread::get_id()!=owner_)return fail(f,Error::wrong_thread,"native LOGON owner thread");
    if(in_call_)return fail(f,Error::invalid_state,"native LOGON reentry");return true;
}
void NativeLogonState::invalidate()noexcept {if(skip_){skip_->active=false;skip_->actions=nullptr;skip_->host=nullptr;}skip_.reset();}
bool NativeLogonState::check_request(Failure& f,bool require_active)noexcept {
    if(!owns_session_)return true;
    video::MovieServiceFailure query;auto actual=service_.request_view(query);
    if(query.kind!=video::MovieServiceError::none){
        f.ownership=std::move(query);owns_session_=false;source_.reset();
        if(f.kind==Error::none)fail(f,Error::service,"native LOGON request observation failure");return false;
    }
    if(!actual){owns_session_=false;source_.reset();
        if(require_active){if(f.kind==Error::none)fail(f,Error::foreign_request,"native LOGON owned request no longer active");return false;}return true;}
    if(actual->generation!=generation_||actual->source_key!=source_key_||actual->source!=source_){
        owns_session_=false;source_.reset();
        f.ownership.kind=video::MovieServiceError::foreign_request;f.ownership.operation="native LOGON foreign live request";
        if(f.kind==Error::none)fail(f,Error::foreign_request,"native LOGON foreign live request");return false;
    }return true;
}
bool NativeLogonState::close_owned(Failure& f)noexcept {
    if(!check_request(f,false))return false;
    if(!owns_session_)return true;
    // Same creating thread; no callbacks occur between the identity check and close.
    owns_session_=false;source_.reset();video::MovieServiceFailure cleanup;
    if(!service_.close(cleanup)){f.cleanup=std::move(cleanup);if(f.kind==Error::none)fail(f,Error::service,"native LOGON resource cleanup failed");return false;}return true;
}
bool NativeLogonState::restore_surround(Failure& f)noexcept {
    if(!surround_owned_)return true;
    // An attempted restore is never retried, even if the effect fails or throws.
    surround_owned_=false;Failure restoration;
    if(invoke(effects_.finish_surround_movie,restoration))return true;
    auto detail=restoration.effect;
    if(!detail.operation||!*detail.operation)detail.operation=restoration.operation;
    f.restoration_failure=detail;
    if(f.kind==Error::none)fail(f,restoration.kind,"native LOGON surround restoration failed");return false;
}
void NativeLogonState::terminate(Failure& f)noexcept {
    invalidate();close_owned(f);restore_surround(f);phase_=Phase::failed;
}
bool NativeLogonState::invoke(const Effect& effect,Failure& f)noexcept {
    try {EffectFailure ef;const bool ok=effect(ef);if(!ok||ef.code!=0||!ef.operation||*ef.operation){f.effect=ef;return fail(f,Error::effect,"native LOGON required game effect failed");}return true;}
    catch(const std::bad_alloc&){return fail(f,Error::allocation,"native LOGON game effect allocation");}
    catch(...){return fail(f,Error::effect,"native LOGON game effect exception");}
}
bool NativeLogonState::native_logon_enter(Failure& f)noexcept {
    if(!access(f))return false;Guard guard(in_call_);
    if(phase_!=Phase::fresh)return fail(f,Error::invalid_state,"native LOGON enter once");
    if(!effects_.prepare_game_world||!effects_.surround_movie||!effects_.finish_surround_movie||!effects_.transition_task)
        return fail(f,Error::missing_effect,"native LOGON unbound game-world effect");
    // Existing Service/control/gain admit all finite binary32 base values.
    if(!std::isfinite(settings_.base_volume))return fail(f,Error::invalid_state,"native LOGON nonfinite base volume");
    if(!actions_.validate_logon_bindings(f.input))return fail(f,Error::input,"native LOGON action mappings");
    if(!actions_.validate_post_movie_targets(f.input))return fail(f,Error::input,"native LOGON post-movie action ownership");
    if(!tasks_.validate_access(f.input.tasks)){
        f.input.kind=input::GameActionError::wrong_thread;f.input.operation="native LOGON task owner thread";
        return fail(f,Error::input,"native LOGON task ownership");}
    auto service_state=service_.snapshot(f.service);if(!service_state)return fail(f,Error::service,"native LOGON service observation");
    if(service_state->active)return fail(f,Error::invalid_state,"native LOGON service already active");
    video::MovieCatalogFailure catalog_failure;auto request=video::RetailMovieCatalog{}.resolve(0,settings_.view,settings_.language,catalog_failure);
    if(!request)return fail(f,Error::invalid_state,"native LOGON catalog request");
    try {source_key_=std::move(request->logical_key);skip_=std::make_shared<SkipLease>(SkipLease{&actions_,&host_,true});}
    catch(...){return fail(f,Error::allocation,"native LOGON skip lease allocation");}
    if(!invoke(effects_.prepare_game_world,f)){terminate(f);return false;}
    // Source preparation clears the SAME persistent intro-skip owner.
    if(!actions_.reset_intro_skip(f.input)){fail(f,Error::input,"native LOGON preparation skip reset");terminate(f);return false;}
    // A failed surround callback owns rollback of its own partial effects.
    if(!invoke(effects_.surround_movie,f)){terminate(f);return false;}
    surround_owned_=true;
    return native_logon_select(f);
}
bool NativeLogonState::native_logon_select(Failure& f)noexcept {
    video::MovieCatalogFailure cf;auto request=video::RetailMovieCatalog{}.resolve(0,settings_.view,settings_.language,cf);
    if(!request||request->logical_key!=source_key_){fail(f,Error::invalid_state,"native LOGON catalog selection");terminate(f);return false;}
    return native_logon_start(*request,f);
}
bool NativeLogonState::native_logon_start(const video::RetailMovieRequest& request,Failure& f)noexcept {
    try {
        // Required game effects may have used the shared Service. Never overwrite
        // the controls or cancel a foreign session they started.
        video::MovieServiceFailure query;auto active=service_.request_view(query);
        if(query.kind!=video::MovieServiceError::none){f.ownership=std::move(query);fail(f,Error::service,"native LOGON pre-start ownership query");terminate(f);return false;}
        if(active){fail(f,Error::foreign_request,"native LOGON foreign request before start");terminate(f);return false;}
        auto lease=skip_;
        if(!service_.set_base_volume(settings_.base_volume,f.service)||
            !service_.install_skip_if_present([lease]()->std::int64_t{
                if(!lease->active)return 0;
                input::GameActionFailure af;auto edge=lease->actions->poll_logon_skip(*lease->host,af);
                if(!edge)throw std::runtime_error(af.operation);return *edge?1:0;
            },f.service)) {fail(f,Error::service,"native LOGON native control settings");terminate(f);return false;}
        if(!service_.start(request.id,request.requested_view,settings_.language,4,f.service)){
            fail(f,Error::service,"native LOGON service start");terminate(f);return false;}
        // Nonallocating active view; own no borrowed key. The key was moved into
        // this State before acquisition. No intervening callback occurs here.
        auto actual=service_.request_view(f.ownership);
        if(!actual||f.ownership.kind!=video::MovieServiceError::none||actual->source_key!=source_key_||!actual->source||actual->generation==0){
            fail(f,Error::foreign_request,"native LOGON successful start missing identity");terminate(f);return false;}
        generation_=actual->generation;source_=std::move(actual->source);owns_session_=true;
        phase_=Phase::playing;return true;
    }catch(...){fail(f,Error::allocation,"native LOGON skip callback allocation");terminate(f);return false;}
}
std::optional<video::MovieControlResult> NativeLogonState::poll(Failure& f)noexcept {
    if(!access(f))return {};Guard guard(in_call_);
    if(phase_!=Phase::playing){fail(f,Error::invalid_state,"native LOGON poll without active movie");return {};}
    if(!check_request(f,true)){terminate(f);return {};}
    auto result=service_.poll(f.service);
    if(result.progress==video::MovieControlProgress::ongoing)return result;
    // Service.poll terminal results already perform actual request teardown.
    // Drop ownership before any continuation/restore callback can start another.
    owns_session_=false;source_.reset();invalidate();
    if(result.progress==video::MovieControlProgress::error){fail(f,Error::service,"native LOGON movie failure");restore_surround(f);phase_=Phase::failed;return result;}
    if(!restore_surround(f)){phase_=Phase::failed;return {};}
    if(!actions_.suspend_after_logon(tasks_,f.input)){
        fail(f,Error::input,"native LOGON post-movie input suspension");phase_=Phase::failed;return {};}
    if(!invoke(effects_.transition_task,f)){
        phase_=Phase::failed;return {};}
    phase_=Phase::complete;return result;
}
bool NativeLogonState::cancel(Failure& f)noexcept {
    if(!access(f))return false;Guard guard(in_call_);invalidate();
    const bool closed=close_owned(f);const bool restored=restore_surround(f);
    if(!closed||!restored){phase_=Phase::failed;return false;}
    if(phase_!=Phase::complete&&phase_!=Phase::failed)phase_=Phase::cancelled;return true;
}
static_assert(std::is_nothrow_move_assignable_v<video::MovieServiceFailure>);
} // namespace mka::native_logon
