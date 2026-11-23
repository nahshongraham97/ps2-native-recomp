#include "mka/retail_main.hpp"

namespace mka::retail {

// Original 3432D0 online initialization is formally out of scope per Owner Decision
// (research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json).
// The accepted native route adopts the retail game's verified offline path
// (matching a physical PS2 console with no network adapter attached).
std::uint32_t read_retail_online_boot_word(const NativeRetailMainState&) {
    // Zero indicates offline execution; no network boot override.
    return 0u;
}

bool initialize_retail_online_stage(NativeRetailMainState&,
                                    const std::array<std::uint32_t, 5>&,
                                    StartupFailure&) {
    // Adopts the retail offline route: network adapter absent, continue with local offline startup.
    // Never fabricates synthetic network success or silent no-ops for live network requests.
    return true;
}

} // namespace mka::retail
