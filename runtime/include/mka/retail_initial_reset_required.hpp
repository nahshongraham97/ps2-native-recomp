#pragma once

#include <optional>

namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;

// Required production effects of18D970. These have no default implementations.
// Their definitions must use the actual native owners and preserve source
// errors/lifetime. Missing resource producers prevent full-game linking.
namespace initial_reset_required {
bool reset_saved_profile_children_246070(NativeRetailMainState&, StartupFailure&);
bool reset_selected_resource_334700(NativeRetailMainState&, StartupFailure&);
bool request_mode_one_banks_12a4f0(NativeRetailMainState&, StartupFailure&);
std::optional<bool> banks_pending_12abd0(NativeRetailMainState&, StartupFailure&);
// Clear nonowning object/cookie then task/cookie. This does not destroy either
// object. Their real16F670/12DDA0 producers must supply the native reference owners.
bool clear_nonowning_references_18da30(NativeRetailMainState&, StartupFailure&);
bool configure_selected_resource_zero_16fc70(NativeRetailMainState&, StartupFailure&);
} // namespace initial_reset_required
} // namespace mka::retail
