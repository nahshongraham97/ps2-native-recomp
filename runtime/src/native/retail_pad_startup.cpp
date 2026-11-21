#include "mka/retail_main.hpp"
#include "mka/retail_pad_control.hpp"
#include "mka/retail_platform.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
RetailStartupGate initialize_retail_pad_service(NativeRetailMainState& main,
    unsigned source_wait_mode,StartupFailure& f) {
    f={};
    auto session=main.startup.game_session;
    if(!session || !main.input_roles) {
        f.kind=StartupError::game_record_reset;f.operation="221C90 native pad game owners missing";
        return RetailStartupGate::native_failure;
    }
    auto* domain=session->task_domain(f.game);
    if(!domain) {
        f.kind=StartupError::game_session;f.operation="221C90 actual native task domain";
        return RetailStartupGate::native_failure;
    }
    if(!suspend_retail_input_from_163ba0(*session,*domain,*main.input_roles,f.game_record_reset)) {
        f.kind=StartupError::game_record_reset;f.operation=f.game_record_reset.operation;
        return RetailStartupGate::native_failure;
    }
    // SIF/module/port negotiation and its optional waiting callback disappear
    // at this boundary. The real native event/controller/keyboard lifetime
    // was established before Main. No source port or mode is a host cap.
    (void)source_wait_mode;
    if(!main.startup.platform ||
       !main.startup.platform->validate_input_owner(main.controller_samples,f.platform)) {
        f.kind=StartupError::platform;f.operation="221C90 actual native input service readiness";
        return RetailStartupGate::native_failure;
    }
    auto controls=session->controls(f.game);
    if(!controls) {
        f.kind=StartupError::game_session;f.operation="221C90 retained native input flag owner";
        return RetailStartupGate::native_failure;
    }
    const auto inhibited=controls->analog_input_inhibited(f.game_record_reset.controls);
    if(!inhibited) {
        f.kind=StartupError::game_record_reset;f.operation="221C90 final live input flag read";
        return RetailStartupGate::native_failure;
    }
    if(!*inhibited && !resume_retail_input_from_163a50(
           *session,*domain,*main.input_roles,f.game_record_reset)) {
        f.kind=StartupError::game_record_reset;f.operation=f.game_record_reset.operation;
        return RetailStartupGate::native_failure;
    }
    return RetailStartupGate::ready;
}
} // namespace mka::retail::main_binding
