#pragma once
namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
enum class RetailStartupGate;
namespace scene_bootstrap_required {
// Native181A30 camera/world-link detach, then the entire remaining1861B0
// suffix is required at125D60. A test fixture cannot close either remainder.
bool continue_scene_reset_at_181a30(NativeRetailMainState&, StartupFailure&);
// Entire remaining127260 after its count1 store; starts at220900.
RetailStartupGate continue_first_scene_at_220900(NativeRetailMainState&, StartupFailure&);
// Entire remaining1861B0 after completed181A30; starts at125D60.
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&);
}
}
