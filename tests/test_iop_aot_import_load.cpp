#include "iop/kernel.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ps2::iop { void execute(Cpu &, std::int64_t &); }

static void put16(std::vector<std::uint8_t> &b, std::size_t p, std::uint16_t v) {
    b[p] = static_cast<std::uint8_t>(v); b[p + 1] = static_cast<std::uint8_t>(v >> 8);
}
static void put32(std::vector<std::uint8_t> &b, std::size_t p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b[p + i] = static_cast<std::uint8_t>(v >> (i * 8));
}
int main() {
#ifdef _WIN32
    _putenv_s("PS2_IOP_NATIVE_STRICT", "0");
#else
    setenv("PS2_IOP_NATIVE_STRICT", "0", 1);
#endif
    using namespace ps2::iop;
    reset();
    kernel::NativeIopModule host;
    host.name = "FIXTUREHOST";
    host.start = [](const std::string &) { return true; };
    kernel::NativeIopLibrary lib;
    lib.name = "fixture"; lib.version = 0x0101;
    lib.exports.push_back({7, [](Cpu &c) { c.r[2] = 0x12345678u; }});
    host.libraries.push_back(std::move(lib));
    kernel::register_native_module(std::move(host));
    assert(load_module_buffer({}, "FIXTUREHOST.IRX", "") > 0);

    std::vector<std::uint8_t> elf(0x160, 0);
    std::memcpy(elf.data(), "\x7f" "ELF", 4); elf[4] = 1; elf[5] = 1;
    put16(elf, 16, 2); put16(elf, 18, 8); put32(elf, 28, 52);
    put16(elf, 42, 32); put16(elf, 44, 2);
    put32(elf, 52, 0x70000080u); put32(elf, 56, 0x100);
    put32(elf, 84, 1); put32(elf, 88, 0x130); put32(elf, 100, 0x30); put32(elf, 104, 0x30);
    put32(elf, 0x104, 32); put32(elf, 0x10c, 0x30);
    put32(elf, 0x130, 0x41e00000); put32(elf, 0x138, 0x0101);
    std::memcpy(elf.data() + 0x13c, "fixture", 7);
    put32(elf, 0x144, 0x03e00008); put32(elf, 0x148, 0x24000007);
    const int id = load_module_buffer(elf, "TESTFIX.IRX", "");
    assert(id > 0);
    const auto it = kernel::modules().find(id); assert(it != kernel::modules().end());
    Cpu c{}; c.pc = it->second.base + 20; c.npc = c.pc + 4; c.r[31] = 0x1234;
    const bool handled = kernel::invoke_compiled_import(c);
    const bool expected = std::getenv("MKA_EXPECT_AOT_IMPORT") != nullptr;
    assert(handled == expected);
    if (handled) {
        assert(c.pc == 0x1234 && c.npc == 0x1238 && c.r[2] == 0x12345678u);
        // A modified stub must not retain the load-time bridge selection.
        c.pc = it->second.base + 20; c.npc = c.pc + 4; c.r[2] = 0;
        const std::uint32_t linked_stub = ram32(c.pc);
        set_ram32(c.pc, 0xFFFFFFFFu);
        assert(!kernel::invoke_compiled_import(c));
        assert(c.pc == it->second.base + 20 && c.r[2] == 0);
        set_ram32(c.pc, linked_stub);
        std::int64_t budget = 1;
        execute(c, budget);
        assert(c.pc == 0x1234 && c.npc == 0x1238 && c.r[2] == 0x12345678u);
    }
}
