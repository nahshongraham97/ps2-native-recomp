#include "mka/retail_pad_control.hpp"

namespace mka::retail {
namespace {
bool fail(RetailGameRecordResetFailure& f,RetailGameRecordResetError kind,
          const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
std::shared_ptr<NativeRetailGameControls> controls_for(
    NativeRetailGameSession& session,NativeRetailTaskDomain& domain,
    RetailGameRecordResetFailure& f) noexcept {
    f={};
    auto* actual=session.task_domain(f.session);
    if(!actual) {
        fail(f,f.session.kind==RetailGameSessionError::wrong_thread ?
             RetailGameRecordResetError::wrong_thread : RetailGameRecordResetError::missing_owner,
             "native pad control Session owner");return {};
    }
    if(actual!=&domain) {
        fail(f,RetailGameRecordResetError::foreign_domain,"native pad control foreign task domain");return {};
    }
    auto controls=session.controls(f.session);
    if(!controls) fail(f,RetailGameRecordResetError::missing_owner,"native pad control live game flags");
    return controls;
}
}

bool synchronize_retail_input_task_arguments(NativeRetailTaskDomain& domain,
    NativeRetailGameControls& controls,RetailGameRecordResetFailure& f) {
    auto* registry=domain.prior_registry_if_born(f.task_domain);
    if(!registry) {
        if(f.task_domain.kind!=RetailTaskDomainError::none)
            return fail(f,RetailGameRecordResetError::task_domain,"native pad prior task registry");
        return true; // Actual source BSS root before the first12F0D0 birth.
    }
    for(const auto kind:{0x2065u,0x2066u,0x208bu}) {
        const auto task=registry->find_first_live(kind,f.registry);
        if(!task) return fail(f,RetailGameRecordResetError::registry,"280810/280760 effectful task lookup");
        if(!*task) continue;
        const auto argument=registry->input_argument_from_12e930(*task,f.registry);
        if(!argument) return fail(f,RetailGameRecordResetError::registry,f.registry.operation);
        if(!*argument) continue; // Real source null getter skips the store.
        // Original280850/280790 reload AFTER lookup/getter effects. Reentry
        // or a callback can change this live value between individual writes.
        const auto inhibited=controls.analog_input_inhibited(f.controls);
        if(!inhibited) return fail(f,RetailGameRecordResetError::controls,"native pad live bit6 read after getter");
        if(!(*argument)->store_restore_analog_inhibited_from_source(*inhibited,f.registry.argument))
            return fail(f,RetailGameRecordResetError::registry,"native pad actual argument flag store");
    }
    return true;
}

bool suspend_retail_input_from_163ba0(NativeRetailGameSession& session,
    NativeRetailTaskDomain& domain,NativeRetailInputRoles& roles,
    RetailGameRecordResetFailure& f) {
    auto controls=controls_for(session,domain,f);if(!controls) return false;
    const auto prior=controls->analog_input_inhibited(f.controls);
    if(!prior) return fail(f,RetailGameRecordResetError::controls,"163BA0 current input flag");
    if(*prior) return true;
    if(!roles.clear_digital_first_three_from_163ba0(f.roles))
        return fail(f,RetailGameRecordResetError::roles,"163BA0 digital triplet reset");
    if(!controls->store_analog_input_inhibited_from_source(true,f.controls))
        return fail(f,RetailGameRecordResetError::controls,"163BA0 bit6 call-delay-slot publication");
    return synchronize_retail_input_task_arguments(domain,*controls,f);
}

bool resume_retail_input_from_163a50(NativeRetailGameSession& session,
    NativeRetailTaskDomain& domain,NativeRetailInputRoles& roles,
    RetailGameRecordResetFailure& f) {
    auto controls=controls_for(session,domain,f);if(!controls) return false;
    if(!controls->store_analog_input_inhibited_from_source(false,f.controls))
        return fail(f,RetailGameRecordResetError::controls,"163A50 clear byte81 bit6");
    if(!roles.clear_dispatch_blocked_first_three_from_163a50(f.roles))
        return fail(f,RetailGameRecordResetError::roles,"163A50 linked participant block-bit clear");
    return synchronize_retail_input_task_arguments(domain,*controls,f);
}
} // namespace mka::retail
