#pragma once
namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
enum class RetailStartupGate;
namespace scene_bootstrap_required {
// Required remainder of1861B0, starting at181A30. Must implement live actor
// graph unlinking then the complete remaining reset/bootstrap in source order.
// Production181A30 now delegates its entire remaining suffix at125D60.
// A test fixture cannot close either required remainder.
bool continue_scene_reset_at_181a30(NativeRetailMainState&, StartupFailure&);
// Entire remaining127260 after its count1 store; starts at220900.
RetailStartupGate continue_first_scene_at_220900(NativeRetailMainState&, StartupFailure&);
// Entire remaining1861B0 after completed181A30; starts at125D60.
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&);
}
}
