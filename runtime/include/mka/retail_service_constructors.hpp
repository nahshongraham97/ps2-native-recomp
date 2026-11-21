#pragma once
namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
namespace main_binding {
// Source648980: a distinct, initially unbound351390 descriptor owner.
// Actual registration/framing/callback consumers are separate obligations.
bool construct_retail_aux_dispatch_object(NativeRetailMainState&, StartupFailure&);
}
} // namespace mka::retail
