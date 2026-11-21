#include "mka/retail_main.hpp"
#include "mka/retail_input_roles.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
namespace {
bool require_role_owner(const NativeRetailMainState& main, std::uint32_t role,
    StartupFailure& failure) {
    failure = {};
    if (!main.input_roles) {
        failure = {StartupError::native_exception, "retail input role owner missing before observation"};
        return false;
    }
    if (role > static_cast<std::uint32_t>(RetailInputRole::role3)) {
        failure = {StartupError::native_exception, "retail input role identity out of range"};
        return false;
    }
    return true;
}
void observe_failure(RetailInputRoleFailure native, StartupFailure& failure) {
    failure.kind = StartupError::native_exception;
    failure.operation = native.kind == RetailInputRoleError::wrong_thread ?
        "retail input role observation wrong thread" :
        native.kind == RetailInputRoleError::invalid_action_row ?
        "retail input action row out of range" : "retail input role observation failed";
}
}
std::optional<RetailFrameRole> observe_retail_frame_role(const NativeRetailMainState& main,
    std::uint32_t role, StartupFailure& failure) {
    if (!require_role_owner(main, role, failure)) return {};
    RetailInputRoleFailure native;
    const auto value = main.input_roles->observe(static_cast<RetailInputRole>(role), native);
    if (!value) { observe_failure(native, failure); return {}; }
    return RetailFrameRole{value->initialized, value->dispatch_blocked, bool(value->participant)};
}
std::optional<RetailActionRowObservation> observe_retail_action_row(const NativeRetailMainState& main,
    std::uint32_t role, std::uint32_t row, StartupFailure& failure) {
    if (!require_role_owner(main, role, failure)) return {};
    RetailInputRoleFailure native;
    const auto value = main.input_roles->observe_action_row(static_cast<RetailInputRole>(role), row, native);
    if (!value) { observe_failure(native, failure); return {}; }
    return value;
}
} // namespace mka::retail::main_binding
