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

    // Reset boots the runtime and installs its production native-module table.
    // Register synthetic fixtures afterward so they do not collide with the
    // built-in MWFILEFR replacement or its filename/header alias.
    ps2::iop::reset();

    const auto irx = make_iopmod_elf("synthetic_native");
    std::string name;
    assert(ps2::iop::kernel::resolve_native_module_name(irx, "buffer", name));
    assert(name == "SYNTHETIC_NATIVE");
    assert(ps2::iop::kernel::resolve_native_module_name({}, "cdvdman.irx", name));
    assert(name == "CDVDMAN");
    assert(!ps2::iop::kernel::resolve_native_module_name({0x7F, 'E', 'L', 'F'}, "buffer", name));
    assert(name.empty());

    int starts = 0;
    int legacy_starts = 0;
    int callback_id = 0;
    int callback_result = -1;
    // This is a synthetic ABI fixture only. It verifies direct C++ imports;
    // it does not claim that a retail IRX has been compiled or replaced.
    ps2::iop::kernel::NativeIopModule native;
    native.name = "Synthetic_Native";
    native.start = [&](const std::string &args) {
        ++starts;
        return args == "native-fixture";
    };
    ps2::iop::kernel::NativeIopLibrary library;
    library.name = "synthetic_exports";
    library.version = 0x0101;
    library.exports.push_back({7, [](ps2::iop::Cpu &c) { c.r[2] = c.r[4] + c.r[5]; }});
    native.libraries.push_back(std::move(library));
    ps2::iop::kernel::register_native_module(std::move(native));

    // A legacy callback with the same name must not be accepted in strict mode.
    ps2::iop::kernel::register_hle_module("Synthetic_Native", [&] { ++legacy_starts; });
    const int id = ps2::iop::load_module_buffer(irx, "buffer", "native-fixture", [&](int module, int result) {
        callback_id = module;
        callback_result = result;
    });
    assert(id > 0);
    assert(starts == 1);
    assert(legacy_starts == 0);
    assert(callback_id == id);
    assert(callback_result == 0);
    const auto module = ps2::iop::kernel::modules().find(id);
    assert(module != ps2::iop::kernel::modules().end());
    assert(module->second.name == "SYNTHETIC_NATIVE");
    assert(module->second.base == 0);
    assert(module->second.entry == 0);

    // MWFILEFR is requested by its disc filename but identifies itself as
    // "mwFile_module" in the IRX IOPMOD header. Both names must select the
    // same host-native replacement, and the loaded module keeps one identity.
    const int mwfilefr_path_id = ps2::iop::load_module_buffer(
        {}, "cdrom0:\\MODULES\\MWFILEFR.IRX;1", "");
    assert(mwfilefr_path_id > 0);
    const auto mwfilefr_path_module = ps2::iop::kernel::modules().find(mwfilefr_path_id);
    assert(mwfilefr_path_module != ps2::iop::kernel::modules().end());
    assert(mwfilefr_path_module->second.name == "MWFILEFR");
    assert(mwfilefr_path_module->second.base == 0);

    const auto mwfilefr_header = make_iopmod_elf("mwFile_module");
    const int mwfilefr_header_id = ps2::iop::load_module_buffer(mwfilefr_header, "buffer", "");
    assert(mwfilefr_header_id > 0);
    const auto mwfilefr_header_module = ps2::iop::kernel::modules().find(mwfilefr_header_id);
    assert(mwfilefr_header_module != ps2::iop::kernel::modules().end());
    assert(mwfilefr_header_module->second.name == "MWFILEFR");
    assert(mwfilefr_header_module->second.base == 0);

    ps2::iop::Cpu caller{};
    caller.r[4] = 19;
    caller.r[5] = 23;
    assert(ps2::iop::kernel::invoke_native_import("synthetic_exports", 0x0101, 7, caller));
    assert(caller.r[2] == 42);

    // Production registration installs the selected-disc SIO2MAN host service.
    // This synthetic IOPMOD header exercises module selection and one real
    // exported service without loading or executing guest IOP instructions.
    const auto sio2man_irx = make_iopmod_elf("sio2man");
    const int sio2man_id = ps2::iop::load_module_buffer(sio2man_irx, "buffer", "");
    assert(sio2man_id > 0);
    const auto sio2man_module = ps2::iop::kernel::modules().find(sio2man_id);
    assert(sio2man_module != ps2::iop::kernel::modules().end());
    assert(sio2man_module->second.name == "SIO2MAN");
    assert(sio2man_module->second.base == 0);
    assert(sio2man_module->second.entry == 0);

    constexpr std::uint32_t transfer_data = 0x1000u;
    ps2::iop::set_ram32(transfer_data + 0x6Cu, 0); // in_size
    ps2::iop::set_ram32(transfer_data + 0x70u, 0); // out_size
    ps2::iop::set_ram32(transfer_data + 0x74u, 0); // in
    ps2::iop::set_ram32(transfer_data + 0x78u, 0); // out
    ps2::iop::Cpu sio2_call{};
    sio2_call.r[4] = transfer_data;
    assert(ps2::iop::kernel::invoke_native_import("sio2man", 0x0207, 25, sio2_call));
    assert(sio2_call.r[2] == 1);
    assert(ps2::iop::ram32(transfer_data + 0x24u) == 0x0Fu); // stat70

    // PADMAN's observed v2.4 imports use an explicitly registered compatibility
    // view of the selected v2.7 service, not a general version wildcard.
    sio2_call = {};
    sio2_call.r[4] = transfer_data;
    assert(ps2::iop::kernel::invoke_native_import("sio2man", 0x0204, 51, sio2_call));
    assert(sio2_call.r[2] == 1);

    return 0;
}
