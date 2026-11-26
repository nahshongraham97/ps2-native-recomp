// Standalone startup-loader test. Uses exact v8 headers, never boots a game.
// Host callbacks are refusal stubs: any attempt to execute guest code fails.
// Compile with loader.cpp, -lbcrypt and the package's two compiled seals.
// Compile this file alone -shared -DR192_AOT_TEST_DLL to make a synthetic
// metadata control; optional R192_AOT_TEST_BAD_ENTRY_POINTER plants the entry
// array outside the DLL. It is never a retail implementation or runtime input.
#include "ps2/generated.hpp"
#include "ps2/ee_live_abi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

#ifdef R192_AOT_TEST_DLL
#ifndef R192_AOT_EE_STAMP
#error "A test DLL requires an explicit fixture stamp"
#endif
namespace {
void no_guest_execution(ps2::Cpu&, std::uint32_t, std::uint32_t) {
    std::abort();
}
#ifdef R192_AOT_TEST_PADDED_UNIT
const std::uint16_t entries[4] = {
#ifdef R192_AOT_TEST_ACTIVE_PADDING
    1,
#else
    0,
#endif
    1, 0, 0};
#else
const std::uint16_t entries[4] = {1, 0, 0, 0};
#endif
const std::uint32_t checks[2] = {0, 0x03E00008};
const ps2::UnitInfo units[] = {
    {
#ifdef R192_AOT_TEST_PADDED_UNIT
     0xFFFFFFFCu,
#else
     0,
#endif
     16, &no_guest_execution,
#ifdef R192_AOT_TEST_BAD_ENTRY_POINTER
     reinterpret_cast<const std::uint16_t*>(1)
#else
     entries
#endif
    }
};
const ps2::ModuleInfo modules[] = {{"synthetic_loader_metadata_control",
#ifdef R192_AOT_TEST_PADDED_UNIT
                                   0x00B00004, 12,
#else
                                   0x00B00000, 16,
#endif
                                   checks, 1, units, 1}};
}
extern "C" __declspec(dllexport) std::uint64_t ps2_ee_live_stamp() {
    return static_cast<std::uint64_t>(R192_AOT_EE_STAMP);
}
extern "C" __declspec(dllexport) const ps2::ModuleInfo* ps2_ee_live_init(
    const ps2::EeLiveHost* host, std::uint32_t* count) {
    const std::uint64_t layout = static_cast<std::uint64_t>(sizeof(ps2::Cpu)) |
                                (static_cast<std::uint64_t>(sizeof(ps2::EeLiveHost)) << 32);
    if (!host || host->layout != layout || !count) return nullptr;
    *count = 1;
    return modules;
}
#else
#include "../tools/research/r192_aot_modules/loader.hpp"
namespace {
std::size_t registry_calls = 0, registered_modules = 0, guest_callback_calls = 0;
[[noreturn]] void forbidden_guest_execution() {
    ++guest_callback_calls;
    throw std::runtime_error("test harness refuses guest callback execution");
}
}

// These symbols have the exact v8 ABI expected by the sealed module host table.
// They do not implement instructions, memory access or any game service.
namespace ps2 {
alignas(64) std::uint8_t g_ram[kRamSize + 64];
alignas(64) std::uint8_t g_spr[kScratchpadSize];
bool g_watch_on = false;
std::uint32_t g_watch_base = 0, g_watch_end = 0;
void dispatch(Cpu&, std::uint32_t, std::uint32_t) { forbidden_guest_execution(); }
#define X(R, name, params, args) R name params { forbidden_guest_execution(); }
PS2_EE_LIVE_FUNCTIONS(X)
#undef X
void add_modules_ranked(const ModuleInfo* modules, std::uint32_t count, int rank) {
    ++registry_calls;
    registered_modules += count;
    if (!modules || !count || (rank != 0 && rank != 1))
        throw std::runtime_error("invalid registry mutation");
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--print-layout") {
        std::printf("{\"cpu_size\":%zu,\"ee_host_size\":%zu}\n", sizeof(ps2::Cpu), sizeof(ps2::EeLiveHost));
        return 0;
    }
    if (argc != 4) {
        std::fprintf(stderr, "usage: loader_test <package-dir> --expect-pass <module-count>\n"
                             "       loader_test <package-dir> --expect-reject <named-error-substring>\n");
        return 2;
    }
    const bool want_pass = std::string(argv[2]) == "--expect-pass";
    const bool want_reject = std::string(argv[2]) == "--expect-reject";
    if (!want_pass && !want_reject) return 2;
    bool admitted = false;
    std::string error;
    try {
        mka::aot::initialize_precompiled_aot_modules(argv[1]);
        admitted = true;
    } catch (const std::exception& exc) { error = exc.what(); }
    const bool pass = guest_callback_calls == 0 &&
        (want_pass ? admitted && registry_calls > 0 && registered_modules == std::stoull(argv[3])
                     : !admitted && registry_calls == 0 && registered_modules == 0 &&
                       error.starts_with("[sealed-ee-aot]") && error.find(argv[3]) != std::string::npos);
    std::fprintf(stderr, "loader-test %s admitted=%d registry_calls=%zu modules=%zu guest_callbacks=%zu error=%s\n",
                 pass ? "PASS" : "FAIL", admitted, registry_calls, registered_modules,
                 guest_callback_calls, error.c_str());
    return pass ? 0 : 1;
}
#endif
