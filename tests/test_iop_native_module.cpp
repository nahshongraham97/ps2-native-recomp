#include "iop/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cstdlib>
#endif

namespace {

void put16(std::vector<std::uint8_t> &data, std::size_t at, std::uint16_t value) {
    data[at] = static_cast<std::uint8_t>(value);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t> &data, std::size_t at, std::uint32_t value) {
    data[at] = static_cast<std::uint8_t>(value);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
    data[at + 2] = static_cast<std::uint8_t>(value >> 16);
    data[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

std::vector<std::uint8_t> make_iopmod_elf(const std::string &module_name) {
    std::vector<std::uint8_t> data(256);
    data[0] = 0x7F;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 1; // ELFCLASS32
    data[5] = 1; // ELFDATA2LSB
    put32(data, 28, 52); // e_phoff
    put16(data, 42, 32); // e_phentsize
    put16(data, 44, 1);  // e_phnum
    put32(data, 52, 0x70000080u); // PT_IOPMOD
    put32(data, 56, 128);         // IOPMOD record file offset
    for (std::size_t i = 0; i < module_name.size(); ++i) data[154 + i] = static_cast<std::uint8_t>(module_name[i]);
    return data;
}

void set_strict_mode() {
#ifdef _WIN32
    _putenv_s("PS2_IOP_NATIVE_STRICT", "1");
#else
    setenv("PS2_IOP_NATIVE_STRICT", "1", 1);
#endif
}

} // namespace

int main() {
    set_strict_mode();
    assert(ps2::iop::kernel::native_strict());

    const auto irx = make_iopmod_elf("synthetic_native");
    std::string name;
    assert(ps2::iop::kernel::resolve_native_module_name(irx, "buffer", name));
    assert(name == "SYNTHETIC_NATIVE");
    assert(ps2::iop::kernel::resolve_native_module_name({}, "cdvdman.irx", name));
    assert(name == "CDVDMAN");
    assert(!ps2::iop::kernel::resolve_native_module_name({0x7F, 'E', 'L', 'F'}, "buffer", name));
    assert(name.empty());

    int starts = 0;
    int callback_id = 0;
    int callback_result = -1;
    // Production installs compiled modules before the guest kernel boots.
    // Their host registrations must survive the kernel's runtime reset.
    ps2::iop::kernel::register_hle_module("Synthetic_Native", [&] { ++starts; });
    ps2::iop::reset();
    const int id = ps2::iop::load_module_buffer(irx, "buffer", {}, [&](int module, int result) {
        callback_id = module;
        callback_result = result;
    });
    assert(id > 0);
    assert(starts == 1);
    assert(callback_id == id);
    assert(callback_result == 0);
    const auto module = ps2::iop::kernel::modules().find(id);
    assert(module != ps2::iop::kernel::modules().end());
    assert(module->second.name == "SYNTHETIC_NATIVE");
    assert(module->second.base == 0);
    assert(module->second.entry == 0);

    return 0;
}
