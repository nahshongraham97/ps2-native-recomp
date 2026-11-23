#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {

// Original 3432D0 online initialization is formally out of scope per Owner Decision
// (research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json).
//
// DEVIATION_ONLINE_STAGE_RETURN_BOOL:
// In the retail PS2 binary, function 0x003432D0 takes stack config pointer a0, copies
// 5 words into the network manager if present, and checks the network context pointer
// at -0x5318(gp). When the network manager is NULL (offline/no network hardware),
// 0x003432D0 returns -1 (0xFFFFFFFF). However, the single retail caller at 0x00122900
// completely discards the return value (v0) and immediately calls 0x00164130 (game options).
// The native stub returns bool true with StartupError::none to indicate non-failing offline
// progression to the native orchestrator.
std::uint32_t read_retail_online_boot_word(const NativeRetailMainState&) {
    // Zero indicates offline execution; no network boot override.
    return 0u;
}

bool initialize_retail_online_stage(NativeRetailMainState&,
                                    const std::array<std::uint32_t, 5>&,
                                    StartupFailure&) {
    // Adopts the retail offline route: network adapter absent, continue with local offline startup.
    // Under DEVIATION_ONLINE_STAGE_RETURN_BOOL, returns true (non-failing) matching caller 00122900's
    // discarding of retail -1. Never fabricates synthetic network success or silent no-ops for live network requests.
    return true;
}

} // namespace mka::retail::main_binding
