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
    // Retail PS2 disassembly at 0x003432D0:
    // Takes argument a0 (5 configuration words). Reads network manager pointer at -0x5318(gp).
    // When network manager is absent (NULL), line 00343350 executes beql a0, zero, 0x00343368
    // and returns -1 (0xFFFFFFFF), completely bypassing socket/network initialization at 0x0033ECF0.
    // In our native runtime, initialize_retail_online_stage handles the network-absent condition
    // cleanly by routing to offline execution with StartupError::none, allocating zero network resources.
    const std::array<std::uint32_t, 5> stage_args{0x1000u, 0x2000u, 0x3000u, 0x4000u, 0x5000u};
    StartupFailure failure{};
    const bool stage_ok = main_binding::initialize_retail_online_stage(dummy_main, stage_args, failure);
    check(stage_ok, "initialize_retail_online_stage routes cleanly to offline route");
    check(failure.kind == StartupError::none, "no failure reported on clean offline route");

    std::cout << "PASS_RETAIL_ONLINE_OFFLINE_ROUTE" << std::endl;
    return 0;
}
