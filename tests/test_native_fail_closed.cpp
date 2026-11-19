#include "ps2/generated.hpp"
#include "kernel.hpp"
#include "iop/kernel.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr std::uint32_t kEntry = 0x00120000u;
constexpr std::uint32_t kCaller = 0x002E6180u; // Former synthetic particle return.
unsigned ee_calls = 0;
unsigned native_starts = 0;

void check(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "[fixture-check-failed] %s\n", message);
        std::exit(2);
    }
}
void environment(const char* name, const char* value) {
#ifdef _WIN32
    check(_putenv_s(name, value) == 0, "set isolated child environment");
#else
    check(setenv(name, value, 1) == 0, "set isolated child environment");
#endif
}
void configure_child() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    // Diagnostic switches must never resurrect an instruction interpreter or
    // synthetic success. Each invocation is an isolated test process.
    for (const char* name : {"PS2_IOP_NATIVE_STRICT", "PS2_IOP_INTERPRETER",
                             "PS2_EE_LIVE", "PS2_EE_INTERPRETER",
                             "PS2_EE_NATIVE_STRICT", "PS2_GUEST_FALLBACK"})
        environment(name, "0");
}
void compiled_unit(ps2::Cpu& cpu, std::uint32_t entry, std::uint32_t depth) {
    check(entry == 1 && depth == 0, "registered EE body receives exact entry/depth");
    ++ee_calls;
    cpu.r[2].d[0] = 0xA11CEu;
    cpu.pc = cpu.r[31].w[0];
}
void register_ee() {
    static const std::uint16_t entries[] = {1};
    static const ps2::UnitInfo unit{kEntry, 4, compiled_unit, entries};
    ps2::register_units(&unit, 1);
}
void register_native() {
    ps2::iop::kernel::NativeIopModule module;
    module.name = "fixture_native_ok";
    module.start = [](const std::string& args) {
        ++native_starts;
        return args == "synthetic-ok";
    };
    ps2::iop::kernel::NativeIopLibrary library;
    library.name = "fixture_exports";
    library.version = 0x1234;
    library.exports.push_back({7, [](ps2::iop::Cpu& cpu) {
        cpu.r[2] = cpu.r[4] ^ cpu.r[5];
    }});
    module.libraries.push_back(std::move(library));
    ps2::iop::kernel::register_native_module(std::move(module));
    const int id = ps2::iop::load_module_buffer({}, "fixture_native_ok.irx", "synthetic-ok");
    check(id > 0 && native_starts == 1, "activate explicit synthetic host module");
}
void positive() {
    register_ee();
    ps2::Cpu ee{};
    ee.pc = kEntry;
    ee.r[31].w[0] = kCaller;
    ps2::dispatch(ee, 0, 0);
    check(ee_calls == 1 && ee.r[2].d[0] == 0xA11CEu && ee.pc == kCaller,
          "former particle caller executes registered body, not fabricated zero");
    ps2::call(ee, kEntry, 0);
    check(ee_calls == 2 && ee.pc == kCaller, "aligned raw call executes registered code");
    ee.pc = kEntry | ps2::kUnwind;
    ps2::dispatch(ee, 0, 0);
    check(ee_calls == 2 && ee.pc == (kEntry | ps2::kUnwind),
          "internal unwind marker stays distinct from raw targets");
    ps2::call(ee, kEntry, ps2::kMaxDepth);
    check(ee_calls == 2 && ee.pc == kEntry, "aligned deferred call preserves its target");
    ps2::dispatch(ee, 0, 0);
    check(ee_calls == 3 && ee.pc == kCaller, "deferred call executes on outer dispatch");

    register_native();
    check(ps2::iop::kernel::native_strict(), "strict enforcement ignores old off switch");
    ps2::iop::Cpu cpu{};
    cpu.r[4] = 0x11223344u;
    cpu.r[5] = 0x55667788u;
    cpu.r[31] = kCaller;
    check(ps2::iop::kernel::invoke_native_import("fixture_exports", 0x1234, 7, cpu),
          "exact native library/version/ordinal dispatch succeeds");
    check(cpu.r[2] == 0x444444CCu && cpu.r[4] == 0x11223344u &&
              cpu.r[5] == 0x55667788u && cpu.r[31] == kCaller,
          "native export publishes only its exact result");

    ps2::iop::kernel::NativeIopModule denied;
    denied.name = "fixture_native_denied";
    denied.start = [](const std::string&) { return false; };
    ps2::iop::kernel::NativeIopLibrary uninstalled;
    uninstalled.name = "fixture_denied_exports";
    uninstalled.version = 0x1234;
    uninstalled.exports.push_back({7, [](ps2::iop::Cpu& c) { c.r[2] = 0xBADu; }});
    denied.libraries.push_back(std::move(uninstalled));
    ps2::iop::kernel::register_native_module(std::move(denied));
    const auto before = ps2::iop::kernel::modules().size();
    int callback_count = 0, callback_id = 99, callback_result = 99;
    const int failed = ps2::iop::load_module_buffer({}, "fixture_native_denied.irx", "synthetic-fail",
        [&](int id, int result) { ++callback_count; callback_id = id; callback_result = result; });
    check(failed == -1 && callback_count == 1 && callback_id == -1 && callback_result == -1 &&
              ps2::iop::kernel::modules().size() == before,
          "false start reports failure and never publishes a module");
    cpu.r[2] = 0;
    check(ps2::iop::kernel::invoke_native_import("fixture_exports", 0x1234, 7, cpu) &&
              cpu.r[2] == 0x444444CCu,
          "failed activation cannot replace the previous working export");
    std::puts("[fixture-positive] registered EE/native IOP dispatch and start failure passed");
}
void put16(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint16_t value) {
    bytes[at] = static_cast<std::uint8_t>(value);
    bytes[at + 1] = static_cast<std::uint8_t>(value >> 8);
}
void put32(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::vector<std::uint8_t> malformed_elf(const std::string& mode) {
    // Synthetic data-only ELF32 layout. No code entry is invoked: malformed
    // source/relocation ranges must stop before AOT body selection/execution.
    std::vector<std::uint8_t> bytes(352);
    bytes[0] = 0x7F; bytes[1] = 'E'; bytes[2] = 'L'; bytes[3] = 'F';
    bytes[4] = 1; bytes[5] = 1;
    put16(bytes, 16, 0xFF80);
    put32(bytes, 28, 52); put16(bytes, 42, 32); put16(bytes, 44, 2);
    put32(bytes, 52, 0x70000080u); put32(bytes, 56, 128);
    put32(bytes, 84, 1); put32(bytes, 88, 192);
    put32(bytes, 100, 16); put32(bytes, 104, 16);
    put32(bytes, 140, 16); // IOPMOD text16/data0/bss0; total16.
    put32(bytes, 32, 224); put16(bytes, 46, 40); put16(bytes, 48, 1);
    put32(bytes, 228, 9); put32(bytes, 240, 288); put32(bytes, 244, 8);
    put32(bytes, 288, 0); put32(bytes, 292, 2); // R_MIPS_32, overridden below.
    if (mode == "elf-table") put32(bytes, 28, 0xFFFFFFF0u);
    else if (mode == "elf-source") put32(bytes, 88, 0xFFFFFFF0u);
    else if (mode == "elf-allocation") put32(bytes, 140, 0xFFFFFFFFu);
    else if (mode == "elf-segment") put32(bytes, 100, 32);
    else if (mode == "elf-reloc-target") put32(bytes, 288, 16);
    else if (mode == "elf-reloc-table") put32(bytes, 244, 7);
    else if (mode == "elf-reloc-source") put32(bytes, 240, 0xFFFFFFF0u);
    else if (mode == "elf-reloc-type") put32(bytes, 292, 0x7Fu);
    else check(false, "known malformed ELF fixture mode");
    return bytes;
}
int negative(const std::string& mode) {
    ps2::Cpu ee{};
    ee.pc = kEntry;
    ee.r[31].w[0] = kCaller;
    if (mode.rfind("ee-", 0) == 0) {
        register_ee();
        if (mode == "ee-null") ee.pc = 0;
        else if (mode == "ee-missing") ee.pc = kEntry + 0x1000;
        else if (mode == "ee-unaligned") ee.pc = kEntry + 2;
        else if (mode == "ee-call-unaligned") ps2::call(ee, kEntry + 1, 0);
        else if (mode == "ee-call-deferred-unaligned") ps2::call(ee, kEntry + 1, ps2::kMaxDepth);
        else if (mode == "ee-main-unaligned") ps2::kernel::start_main_thread(kEntry + 1, 0, 0, 0);
        else if (mode == "ee-stop") ps2::stop(ee, ee.pc, "synthetic explicit stop");
        else if (mode == "ee-bad-entry") ps2::bad_entry(ee, kEntry);
        else if (mode == "ee-break") ps2::breakpoint(ee, ee.pc);
        else if (mode == "ee-trap") ps2::trap(ee, ee.pc);
        else check(false, "known EE negative fixture mode");
        ps2::dispatch(ee, 0, 0);
    } else if (mode == "iop-pc") {
        ps2::iop::kernel::call_guest(0x00123450u);
    } else if (mode == "iop-bad-hle") {
        ps2::iop::Cpu cpu{};
        ps2::iop::call_hle(cpu, 0xFFFFFFFFu);
    } else if (mode == "iop-syscall") {
        ps2::iop::Cpu cpu{}; cpu.r[2] = 0xDEADu;
        ps2::iop::kernel::syscall(cpu, 0x00123450u);
    } else if (mode.rfind("iop-import-", 0) == 0) {
        register_native();
        ps2::iop::Cpu cpu{}; cpu.r[31] = kCaller; cpu.r[2] = 0xDEADBEEFu;
        if (mode == "iop-import-library")
            ps2::iop::kernel::invoke_native_import("fixture_missing", 0x1234, 7, cpu);
        else if (mode == "iop-import-version")
            ps2::iop::kernel::invoke_native_import("fixture_exports", 0x1235, 7, cpu);
        else if (mode == "iop-import-ordinal")
            ps2::iop::kernel::invoke_native_import("fixture_exports", 0x1234, 8, cpu);
        else check(false, "known import negative fixture mode");
    } else if (mode == "elf-magic") {
        ps2::iop::load_module_buffer({1, 2, 3, 4}, "fixture_bad_elf.irx", "");
    } else if (mode.rfind("elf-", 0) == 0) {
        ps2::iop::load_module_buffer(malformed_elf(mode), "fixture_bad_elf.irx", "");
    } else {
        std::fprintf(stderr, "[fixture-usage] unknown mode: %s\n", mode.c_str());
        return 2;
    }
    // A nonzero exit alone is insufficient: the parent runner must require
    // the mode-specific runtime marker, never this fixture-return guard.
    std::fprintf(stderr, "[fixture-unexpected-return] fatal mode %s returned\n", mode.c_str());
    return 2;
}
} // namespace

int main(int argc, char** argv) {
    configure_child();
    if (argc != 2) { std::fputs("[fixture-usage] exactly one mode is required\n", stderr); return 2; }
    try {
        const std::string mode(argv[1]);
        if (mode == "positive") { positive(); return 0; }
        return negative(mode);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[fixture-exception] %s\n", error.what());
        return 2;
    }
}
