#include "mka/retail_mode_scene.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <new>
#include <utility>
namespace mka::retail {
NativeRetailModeSceneRoutes::NativeRetailModeSceneRoutes(
    std::weak_ptr<NativeRetailMainLifetime> lifetime,
    std::shared_ptr<NativeRetailRendererCore> core, std::vector<RetailModeScenePlan> plans)
    : lifetime_(std::move(lifetime)),core_(std::move(core)),plans_(std::move(plans)) {}
bool NativeRetailModeSceneRoutes::access(StartupFailure& f) const noexcept {
    auto lifetime=lifetime_.lock();
    if(std::this_thread::get_id()!=thread_||!lifetime||!lifetime->active_on_owner_thread()){
        f={StartupError::game_session,"mode scene requires live Main owner/thread"};return false;
    }
    return true;
}
bool NativeRetailModeSceneRoutes::install(NativeRetailMainState& main,
    std::shared_ptr<NativeRetailRendererCore> core,std::vector<RetailModeScenePlan> plans,
    StartupFailure& f) noexcept {
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()||main.mode_scene_routes||!core||
       !core->same_session(main.startup.game_session)||plans.empty()){
        f={StartupError::game_session,"explicit mode scene registration requires unbound live Main/Core"};return false;
    }
    for(std::size_t i=0;i<plans.size();++i){
        const auto& plan=plans[i];
        if(plan.child!=RetailInitialTransitionBody::source_245930 && plan.child!=RetailInitialTransitionBody::menu_18db60){
            f={StartupError::content,"unproved native transition child"};return false;
        }
        if(plan.dispatch_stage<0||plan.dispatch_stage>50){
            f={StartupError::content,"mode scene dispatch stage outside proved 27B1D0 table"};return false;
        }
        if(!plan.scene.display||!plan.scene.gpu_retirement){
            f={StartupError::content,"mode scene requires explicit display and host GPU retirement owner"};return false;
        }
        if(plan.scene.display!=plans.front().scene.display ||
           plan.scene.display.owner_before(plans.front().scene.display) ||
           plans.front().scene.display.owner_before(plan.scene.display)){
            f={StartupError::content,"cross-display mode transition requires an unbound host context-switch service"};return false;
        }
        for(std::size_t j=0;j<i;++j)
            if(plans[j].transition_mode==plan.transition_mode&&plans[j].child==plan.child){
                f={StartupError::content,"ambiguous native mode scene registration"};return false;
            }
    }
    try {
        main.mode_scene_routes=std::shared_ptr<NativeRetailModeSceneRoutes>(
            new NativeRetailModeSceneRoutes(main.lifetime,std::move(core),std::move(plans)));
        return true;
    } catch(...) {f={StartupError::allocation,"mode scene registration allocation"};return false;}
}
bool NativeRetailModeSceneRoutes::request(std::uint32_t mode,RetailInitialTransitionBody child) noexcept {
    StartupFailure f;
    if(!access(f))return false;
    if(busy_){request_failure_="mode scene transition reentry refused";return false;}
    for(std::size_t i=0;i<plans_.size();++i)
        if(plans_[i].transition_mode==mode&&plans_[i].child==child){
            pending_=i;request_failure_=nullptr;return true;
        }
    request_failure_="mode scene transition has no configured asset/presentation binding";
    return false;
}
bool NativeRetailModeSceneRoutes::dispatch(NativeRetailMainState& main,StartupFailure& f) {
    f={};if(!access(f))return false;
    if(main.mode_scene_routes.get()!=this||busy_){
        f={StartupError::game_session,"foreign/reentrant native mode scene dispatch"};return false;
    }
    if(request_failure_){f={StartupError::content,request_failure_};return false;}
    if(!pending_)return true;
    struct Guard{bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(busy_);
    const auto selected=*pending_;
    const auto& plan=plans_[selected];
    if(!plan.scene.display->is_context_current()){
        f={StartupError::memory_owners,"mode scene requires its actual display context before transition"};return false;
    }
    if(main.scene&&!scene_bootstrap_required::continue_scene_reset_at_125d60(main,f))return false;
    active_.reset();
    try {
        auto candidate=std::make_shared<NativeRetailSceneOwners>();
        candidate->config=plan.scene;
        main.scene=std::move(candidate);
        if(scene_bootstrap_required::continue_first_scene_at_225d40(main,core_,f)!=RetailStartupGate::ready){
            StartupFailure cleanup;
            (void)scene_bootstrap_required::continue_scene_reset_at_125d60(main,cleanup);
            return false;
        }
        if(!main.scene||!main.scene->scene_loaded){
            StartupFailure cleanup;
            (void)scene_bootstrap_required::continue_scene_reset_at_125d60(main,cleanup);
            f={StartupError::content,"registered mode scene assets missing; diagnostic headless path is not production readiness"};return false;
        }
        active_=selected;pending_.reset();++transitions_;
        return true;
    } catch(const detail::NativeMainReentry&) {
        // Preserve the existing native Main transfer; never turn it into an
        // ordinary return or an allocation error from this presentation seam.
        throw;
    } catch(...) {
        StartupFailure cleanup;
        (void)scene_bootstrap_required::continue_scene_reset_at_125d60(main,cleanup);
        f={StartupError::allocation,"native mode scene preparation allocation"};return false;
    }
}
std::optional<std::uint32_t> NativeRetailModeSceneRoutes::active_mode() const noexcept {
    StartupFailure f;if(!access(f)||!active_)return {};
    return plans_[*active_].transition_mode;
}
std::optional<std::int32_t> NativeRetailModeSceneRoutes::active_stage() const noexcept {
    StartupFailure f;if(!access(f)||!active_)return {};
    return plans_[*active_].dispatch_stage;
}
} // namespace mka::retail

