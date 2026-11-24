#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool ok, const char* msg) {
    if (!ok) {
        std::cerr << "Assertion failed: " << msg << std::endl;
        std::abort();
    }
}
}

int main() {
    using namespace mka::retail;

    // 1. Verify offline boot configuration contract:
    // When no network boot override is requested, read_retail_online_boot_word returns 0.
    // Memory-safe aligned buffer provides valid reference without invoking unrelated subsystem constructors.
    alignas(NativeRetailMainState) char main_buf[sizeof(NativeRetailMainState)];
    auto& dummy_main = *reinterpret_cast<NativeRetailMainState*>(main_buf);

    const std::uint32_t boot_word = main_binding::read_retail_online_boot_word(dummy_main);
    check(boot_word == 0u,
          "offline boot word is 0 (offline path, no network boot override)");

    // 2. Verify initialize_retail_online_stage routes cleanly to the offline route:
    // Retail PS2 disassembly from SLUS_214.10 (caller 0x00122900 -> 0x003432D0):
    // 0x00122900 sets up stack config buffer at sp+32 and calls 0x003432D0.
    // 0x003432D0 calls 0x0034F380 (allocates 84-byte "Online" object and stores it at -0x5318(gp)),
    // copies 5 config words, and calls 0x0033ECF0 (cOnline::InitializeOnline).
    // In 0x0033ECF0, 0x00115B68 attempts to load dev9.irx; on a console without network hardware,
    // this fails with "TRC Violation*** -> Unable to load dev9.irx\n", calls ShutdownPartial (0x00343E80),
    // and returns -1 (0xFFFFFFFF). 0x003432D0 returns -1, which caller 0x00122900 completely discards
    // (storing to 48(sp) and continuing to offline options at 0x00164130).
    // Omission of the 84-byte dynamic heap allocation in native C++ has zero impact on subsequent host subsystems.
    // Under named deviation DEVIATION_ONLINE_STAGE_RETURN_BOOL, initialize_retail_online_stage handles
    // the network-absent condition cleanly by returning true with StartupError::none to indicate
    // non-failing offline progression, allocating zero network resources.
    const std::array<std::uint32_t, 5> stage_args{0x1000u, 0x2000u, 0x3000u, 0x4000u, 0x5000u};
    StartupFailure failure{};
    const bool stage_ok = main_binding::initialize_retail_online_stage(dummy_main, stage_args, failure);
    check(stage_ok, "initialize_retail_online_stage routes cleanly to offline route (DEVIATION_ONLINE_STAGE_RETURN_BOOL)");
    check(failure.kind == StartupError::none, "no failure reported on clean offline route");

    // 3. Verify native unwired component modal helper (DEVIATION_NATIVE_GO_ONLINE_MODAL_MESSAGE):
    // On retail hardware without a network adapter, 0x0034A110 immediately presents 0x00639C80
    // ("No network adaptor detected"). The string at 0x005E6320 is the authentication-failure fallback
    // reached if adapter hardware is present but DNAS authentication fails.
    // This test exercises the unwired component helper returning the authentic 0x005E6320 string.
    main_binding::RetailOnlineModalState modal_state{};
    const bool modal_ok = main_binding::trigger_retail_go_online_modal(modal_state);
    check(modal_ok, "trigger_retail_go_online_modal succeeds");
    check(modal_state.active, "modal state marked active upon selecting Go Online");
    check(std::string(modal_state.message) == "Could not authenticate user ID and Password. Please check your network configuration and try again.",
          "modal message matches retail SLUS_214.10 string at 0x005E6320");
    check(modal_state.return_to_main_menu, "modal dismisses with return to main menu");

    std::cout << "PASS_RETAIL_ONLINE_OFFLINE_ROUTE" << std::endl;
    std::cout << "PASS_RETAIL_ONLINE_MODAL_DISMISSED" << std::endl;
    return 0;
}
