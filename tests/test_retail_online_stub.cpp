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

    // 1. Verify online stubbing contract against PS2 oracle (network absent):
    // When no network adapter is attached on PS2 hardware, read_retail_online_boot_word returns 0.
    // Memory-safe aligned buffer provides valid reference without invoking unrelated subsystem constructors.
    alignas(NativeRetailMainState) char main_buf[sizeof(NativeRetailMainState)];
    auto& dummy_main = *reinterpret_cast<NativeRetailMainState*>(main_buf);

    const std::uint32_t boot_word = main_binding::read_retail_online_boot_word(dummy_main);
    check(boot_word == 0u,
          "PS2 oracle offline equivalence: boot word is 0 (offline path, no network boot override)");

    // 2. Verify initialize_retail_online_stage routes cleanly to the offline route:
    // PS2 function 003432D0 reads 5 configuration words from a0. When network hardware is absent,
    // it bypasses driver initialization and returns cleanly to offline execution without allocating sockets.
    const std::array<std::uint32_t, 5> stage_args{0x1000u, 0x2000u, 0x3000u, 0x4000u, 0x5000u};
    StartupFailure failure{};
    const bool stage_ok = main_binding::initialize_retail_online_stage(dummy_main, stage_args, failure);
    check(stage_ok, "initialize_retail_online_stage routes cleanly to offline route");
    check(failure.kind == StartupError::none, "no failure reported on clean offline route");

    std::cout << "PASS_RETAIL_ONLINE_OFFLINE_ROUTE" << std::endl;
    return 0;
}
