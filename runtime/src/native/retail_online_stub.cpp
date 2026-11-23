#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {

// Original 3432D0 online initialization is formally out of scope per Owner Decision
// (research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json).
//
// DEVIATION_ONLINE_STAGE_RETURN_BOOL:
// Retail MIPS execution analysis from SLUS_214.10:
// 1. Caller 0x00122900 sets up a 20-byte stack buffer at sp+32 (populated with 4 words from -21680(gp))
//    and calls 0x003432D0 with a0 = sp+32 (a non-NULL stack pointer).
// 2. In 0x003432D0, the pointer is checked against NULL. Because a0 is a valid stack buffer pointer,
//    the branch is taken and 0x003432D0 calls 0x0034F380.
// 3. 0x0034F380 invokes operator new(84, "Online") (0x00311780) and constructor 0x00344000 / 0x0033EFD0.
//    Base constructor 0x0033EFD0 stores the newly allocated object pointer into -21272(gp) (-0x5318(gp))
//    in the delay slot of its final jr ra, and installs class vtables (0x0064A200, 0x0064A2A0, 0x0064A650).
// 4. Back in 0x003432D0, the 5 config words from the stack buffer are copied into object offsets +12..+28.
//    0x003432D0 then calls 0x0033ECF0(object, -21196(gp)) (cOnline::InitializeOnline).
// 5. In 0x0033ECF0, 0x00115B68 is invoked to load/initialize the PS2 network adapter driver (dev9.irx).
//    On a standard console without network hardware, this call fails (v0 < 0), printing:
//    "TRC Violation*** -> Unable to load dev9.irx\n".
//    0x0033ECF0 then calls vtable method 72 (0x00343E80) to clean up partial online state,
//    and returns -1 (0xFFFFFFFF).
// 6. 0x003432D0 propagates this -1 return value back to caller 0x00122900, which stores it to 48(sp)
//    and completely discards it, proceeding immediately with offline initialization (0x00164130, 0x0012D7E0).
// 7. Heap impact: Omission of the 84-byte dynamic "Online" heap allocation in the native port has zero
//    effect on subsequent systems because native components use typed host memory ownership rather than
//    fixed guest heap addresses.
//
// Under named deviation DEVIATION_ONLINE_STAGE_RETURN_BOOL, initialize_retail_online_stage returns
// bool true with StartupError::none to indicate non-failing offline progression to the native orchestrator,
// directly matching retail caller 0x00122900's uninterrupted continuation past 0x003432D0.
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
