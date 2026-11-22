#pragma once
namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
namespace scene_bootstrap_required {
// Required remainder of1861B0, starting at181A30. Must implement live actor
// graph unlinking then the complete remaining reset/bootstrap in source order.
// No production implementation exists. A test fixture cannot close this gate.
bool continue_scene_reset_at_181a30(NativeRetailMainState&, StartupFailure&);
}
}
