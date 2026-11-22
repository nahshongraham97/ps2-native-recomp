#include "mka/retail_input_admission.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <new>
namespace mka::retail {
namespace {
std::optional<bool> fail(RetailInputAdmissionFailure& f,RetailInputAdmissionError e,const char* op) {
    f.kind=e;f.operation=op;return {};
}
//280BC0, also inlined twice in280C50. These are game contexts, not devices.
bool context_blocks(std::int32_t context,std::int32_t mode) noexcept {
    if(context==0||context==2||context==3||context==8)return false;
    if(context==11)return mode==8||mode==9||mode==17;
    return true;
}
}
class NativeRetailInputAdmission final {
public:
    std::weak_ptr<NativeRetailMainLifetime> lifetime;
    std::shared_ptr<NativeRetailGameSession> session;
    std::shared_ptr<NativeRetailInputRoles> roles;
    std::shared_ptr<NativeRetailParticipants> participants;
    std::shared_ptr<NativeRetailInputAssignments> assignments;
    std::unique_ptr<NativeRetailInputSelection> selection;
    bool active{};
};
std::optional<bool> admit_retail_physical_role_from_280c50(
    NativeRetailMainState& main,RetailInputRole role,RetailInputAdmissionFailure& f) {
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()||
       !main.startup.game_session||!main.input_roles||!main.participants||
       !main.input_assignments||!main.mode_history)
        return fail(f,RetailInputAdmissionError::owner,"280C50 actual Main input owners");
    if(role!=RetailInputRole::role0&&role!=RetailInputRole::role1)
        return fail(f,RetailInputAdmissionError::role,"2214D0 physical role0/1 admission");
    if(!main.input_assignments->validate_domain(*main.input_roles,*main.participants,f.assignment))
        return fail(f,RetailInputAdmissionError::assignment,"280C50 Main assignment domain mismatch");
    if(!main.input_admission) {
        try {
            auto owner=std::make_shared<NativeRetailInputAdmission>();
            owner->lifetime=main.lifetime;owner->session=main.startup.game_session;
            owner->roles=main.input_roles;owner->participants=main.participants;
            owner->assignments=main.input_assignments;
            owner->selection=NativeRetailInputSelection::create(owner->session,owner->roles,owner->participants,f.selection);
            if(!owner->selection)return fail(f,RetailInputAdmissionError::selection,"280C50 actual281930 domain");
            main.input_admission=std::move(owner);
        }catch(const std::bad_alloc&){return fail(f,RetailInputAdmissionError::allocation,"280C50 native admission owner");}
    }
    // Retain through effectful registry cleanup and nonlocal Main reentry.
    const auto owner=main.input_admission;
    auto valid=[&] {
        const auto lifetime=owner->lifetime.lock();
        return lifetime&&lifetime->active_on_owner_thread()&&lifetime==main.lifetime&&
            main.startup.game_session==owner->session&&main.input_roles==owner->roles&&
            main.participants==owner->participants&&main.input_assignments==owner->assignments&&
            main.input_admission==owner&&main.mode_history.has_value();
    };
    if(!valid())return fail(f,RetailInputAdmissionError::owner,"280C50 retained Main identity");
    if(owner->active)return fail(f,RetailInputAdmissionError::reentrant,"280C50 recursive admission");
    struct Guard {bool& active;explicit Guard(bool& a):active(a){a=true;}~Guard(){active=false;}} guard(owner->active);
    auto state=owner->roles->observe(role,f.role);
    if(!state)return fail(f,RetailInputAdmissionError::role,"280C50 source role");
    if(!state->initialized)return false; //280C88, before any task lookup.
    auto blocked=[&]() -> std::optional<bool> {
        const auto mode=owner->session->scalar_mode(f.session);
        if(!mode)return {};
        return context_blocks(main.mode_history->top(),*mode);
    };
    if(state->participant) {
        const auto b=blocked();if(!b)return fail(f,RetailInputAdmissionError::session,"280C50 live scalar mode");
        if(*b)return false;
    }
    auto* tasks=owner->session->task_domain(f.session);
    if(!tasks)return fail(f,RetailInputAdmissionError::session,"280C50 actual task domain");
    auto* registry=tasks->registry(f.tasks);
    if(!registry)return fail(f,RetailInputAdmissionError::tasks,"280C50 source task roots not ready");
    for(const auto kind:{0x2065u,0x2066u}) {
        std::optional<std::shared_ptr<NativeRetailTaskObject>> task;
        try {task=registry->find_first_live(kind,f.registry);}
        catch(const detail::NativeMainReentry&){throw;}
        catch(const std::bad_alloc&){return fail(f,RetailInputAdmissionError::allocation,"280C50 task cleanup allocation");}
        catch(...){return fail(f,RetailInputAdmissionError::native_exception,"280C50 task cleanup exception");}
        if(!task)return fail(f,RetailInputAdmissionError::tasks,"280C50 effectful12ED10 query");
        if(!valid())return fail(f,RetailInputAdmissionError::owner,"280C50 Main changed during task cleanup");
        if(*task)return false;
    }
    const auto selected=owner->selection->select_physical(role,f.selection);
    if(!selected)return fail(f,RetailInputAdmissionError::selection,"280C50 actual281930 selection");
    if(!*selected)return false;
    const auto participant=*selected;
    auto p=participant->observe(f.participant);
    if(!p)return fail(f,RetailInputAdmissionError::participant,"280C50 selected participant");
    if(p->assigned_role>=0) {
        const auto b=blocked();if(!b)return fail(f,RetailInputAdmissionError::session,"280C50 selected live mode");
        if(*b)return false;
    }
    if(p->game_state==3)return false; //280E30..40, after selection/diagnostic.
    // Existing281060 has a required fighter+390 effect. Do not claim a
    // successful admission when that genuine producer is still unimplemented.
    if(p->has_fighter)return fail(f,RetailInputAdmissionError::fighter,"280C50 required fighter role propagation");
    if(p->assigned_role!=-1&&!owner->assignments->detach(participant,f.assignment))
        return fail(f,RetailInputAdmissionError::assignment,"280C50 old role detach before count");
    if(!owner->assignments->increment_count_before_280c50_bind(f.assignment)||
       !owner->assignments->bind_direct(participant,role,f.assignment))
        return fail(f,RetailInputAdmissionError::assignment,"280C50 count then281060 bind");
    return true;
}
} // namespace mka::retail
