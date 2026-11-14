#include "iop/kernel.hpp"
#include "iop/spu2.hpp"
#include "ps2/memory.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

    // thbase v1.02 ordinal 42 is imported by the captured retail modules.
    // With no IOP thread selected, the native current-priority query returns
    // the same zero sentinel used by GetThreadId in this runtime state.
    ps2::iop::Cpu current_priority_call{};
    assert(ps2::iop::kernel::invoke_native_import("thbase", 0x0102, 42, current_priority_call));
    assert(current_priority_call.r[2] == 0);

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

    // The retail MSL initializer imports mwfile v1.03 ordinal 15. The
    // MWFILEFR host module provides that exact C++ export; it must not fall
    // through to the guest IOP interpreter in strict mode.
    ps2::iop::Cpu mwfile_init_call{};
    mwfile_init_call.r[4] = 0xA5A5A5A5u;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 15, mwfile_init_call));
    assert(mwfile_init_call.r[2] == 0);
    assert(mwfile_init_call.r[4] == 0xA5A5A5A5u);

    // Retail MSL reaches MWFILEFR v1.03 ordinal 13 after the filesystem open.
    // The selected IRX's MIPS32 leaf returns without assigning v0, so the
    // native export must preserve the incoming return register in strict mode.
    ps2::iop::Cpu mwfile_ordinal13_call{};
    mwfile_ordinal13_call.r[2] = 0x13579BDFu;
    mwfile_ordinal13_call.r[4] = 0x2468ACE0u;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 13, mwfile_ordinal13_call));
    assert(mwfile_ordinal13_call.r[2] == 0x13579BDFu);
    assert(mwfile_ordinal13_call.r[4] == 0x2468ACE0u);

    const auto temp_root = std::filesystem::temp_directory_path() /
        ("mwfilefr-native-import-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(temp_root);
    const auto temp_file = temp_root / "MWFILEFR_READ_TEST.BIN";
    std::vector<std::uint8_t> read_expected(64u);
    for (std::size_t i = 0; i < read_expected.size(); ++i)
        read_expected[i] = static_cast<std::uint8_t>(i * 29u + 7u);
    {
        std::ofstream out(temp_file, std::ios::binary);
        out.write(reinterpret_cast<const char *>(read_expected.data()),
                  static_cast<std::streamsize>(read_expected.size()));
        assert(out.good());
    }
    const char *old_vfs_root_value = std::getenv("PS2_NATIVE_VFS_ROOT");
    const bool had_old_vfs_root = old_vfs_root_value != nullptr;
    const std::string old_vfs_root = old_vfs_root_value ? old_vfs_root_value : "";
#ifdef _WIN32
    _putenv_s("PS2_NATIVE_VFS_ROOT", temp_root.string().c_str());
#else
    setenv("PS2_NATIVE_VFS_ROOT", temp_root.string().c_str(), 1);
#endif
    constexpr std::uint32_t read_path_address = 0x7000u;
    constexpr std::uint32_t read_stack_address = 0x8000u;
    constexpr std::uint32_t read_destination = 0x007F2D54u;
    constexpr char read_guest_path[] = "cdrom0:\\MWFILEFR_READ_TEST.BIN;1";
    std::memcpy(ps2::iop::ram_ptr(read_path_address), read_guest_path,
                sizeof(read_guest_path));
    ps2::iop::Cpu mwfile_open_read_call{};
    mwfile_open_read_call.r[4] = read_path_address;
    mwfile_open_read_call.r[5] = 1u;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 4,
                                                  mwfile_open_read_call));
    const std::uint32_t read_file_handle = mwfile_open_read_call.r[2];
    assert(read_file_handle != 0u);

    std::memset(ps2::iop::ram_ptr(read_stack_address), 0, 0x20u);
    ps2::iop::ram_ptr(read_stack_address + 0x10u)[0] = 1u;
    ps2::iop::Cpu mwfile_read_call{};
    mwfile_read_call.r[4] = read_file_handle;
    mwfile_read_call.r[5] = read_destination;
    mwfile_read_call.r[6] = 16u;
    mwfile_read_call.r[7] = 7u;
    mwfile_read_call.r[29] = read_stack_address;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 10,
                                                  mwfile_read_call));
    assert(mwfile_read_call.r[2] == 16u);
    assert(std::equal(read_expected.begin() + 7, read_expected.begin() + 23,
                      ps2::iop::ram_ptr(read_destination)));

    mwfile_read_call.r[6] = 16u;
    mwfile_read_call.r[7] = 60u;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 10,
                                                  mwfile_read_call));
    assert(mwfile_read_call.r[2] == 4u);
    assert(std::equal(read_expected.begin() + 60, read_expected.end(),
                      ps2::iop::ram_ptr(read_destination)));

    ps2::iop::Cpu mwfile_close_read_call{};
    mwfile_close_read_call.r[4] = read_file_handle;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 5,
                                                  mwfile_close_read_call));
    assert(mwfile_close_read_call.r[2] == 0u);
#ifdef _WIN32
    _putenv_s("PS2_NATIVE_VFS_ROOT", had_old_vfs_root ? old_vfs_root.c_str() : "");
#else
    if (had_old_vfs_root) setenv("PS2_NATIVE_VFS_ROOT", old_vfs_root.c_str(), 1);
    else unsetenv("PS2_NATIVE_VFS_ROOT");
#endif
    std::error_code remove_ec;
    std::filesystem::remove_all(temp_root, remove_ec);
    assert(!remove_ec);

    constexpr std::uint32_t missing_mwfile_path = 0x6000u;
    constexpr char missing_mwfile_path_text[] = "cdrom0:\\__native_mwfile_missing__;1";
    std::memcpy(ps2::iop::ram_ptr(missing_mwfile_path), missing_mwfile_path_text,
                sizeof(missing_mwfile_path_text));
    ps2::iop::Cpu mwfile_open_call{};
    mwfile_open_call.r[4] = missing_mwfile_path;
    mwfile_open_call.r[5] = 1;
    assert(ps2::iop::kernel::invoke_native_import("mwfile", 0x0103, 4, mwfile_open_call));
    assert(mwfile_open_call.r[2] == 0); // unresolved paths do not fall through to guest IOP code

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

    // DEV9 loads as a native offline module. Its service exports report the
    // same unavailable-device status as the PS2SDK driver when no hardware is
    // present; the guest DEV9 IRX is never loaded in strict mode.
    const auto dev9_irx = make_iopmod_elf("DEV9");
    const int dev9_id = ps2::iop::load_module_buffer(dev9_irx, "buffer", "");
    assert(dev9_id > 0);
    const auto dev9_module = ps2::iop::kernel::modules().find(dev9_id);
    assert(dev9_module != ps2::iop::kernel::modules().end());
    assert(dev9_module->second.name == "DEV9");
    ps2::iop::Cpu dev9_call{};
    dev9_call.r[4] = 0; // DEV9 DMA controller
    assert(ps2::iop::kernel::invoke_native_import("dev9", 0x0109, 5, dev9_call));
    assert(dev9_call.r[2] == 0xFFFFFFFFu);
    constexpr std::uint32_t ethernet_id = 0x1800u;
    ps2::iop::set_ram32(ethernet_id, 0xA5A5A5A5u);
    dev9_call = {};
    dev9_call.r[4] = ethernet_id;
    assert(ps2::iop::kernel::invoke_native_import("dev9", 0x0109, 9, dev9_call));
    assert(dev9_call.r[2] == 0xFFFFFFFFu);
    assert(ps2::iop::ram32(ethernet_id) == 0xA5A5A5A5u);

    // USBD v1.2 is a host-native offline service. Loading by the retail disc
    // path selects USBD without loading the guest USB_driver IRX body.
    const int usbd_id = ps2::iop::load_module_buffer(
        {}, "cdrom0:\\MODULES\\USBD.IRX;1", "");
    assert(usbd_id > 0);
    const auto usbd_module = ps2::iop::kernel::modules().find(usbd_id);
    assert(usbd_module != ps2::iop::kernel::modules().end());
    assert(usbd_module->second.name == "USBD");
    assert(usbd_module->second.base == 0);
    assert(usbd_module->second.entry == 0);

    // LGKBM's EE-facing SIF RPC service is host-owned. Its retail IRX imports
    // kernel libraries but exports no IOP library; strict path loading selects
    // the C++ module record and does not load the guest init/thread body.
    const int lgkbm_id = ps2::iop::load_module_buffer(
        {}, "cdrom0:\\MODULES\\LGKBM.IRX;1", "");
    assert(lgkbm_id > 0);
    const auto lgkbm_module = ps2::iop::kernel::modules().find(lgkbm_id);
    assert(lgkbm_module != ps2::iop::kernel::modules().end());
    assert(lgkbm_module->second.name == "LGKBM");
    assert(lgkbm_module->second.base == 0);
    assert(lgkbm_module->second.entry == 0);

    // LGAUD is an optional USB audio driver. With no attached USB audio
    // device, its host replacement has no IOP exports and does not execute the
    // guest IRX init/thread body.
    const int lgaud_id = ps2::iop::load_module_buffer(
        {}, "cdrom0:\\MODULES\\LGAUD.IRX;1", "");
    assert(lgaud_id > 0);
    const auto lgaud_module = ps2::iop::kernel::modules().find(lgaud_id);
    assert(lgaud_module != ps2::iop::kernel::modules().end());
    assert(lgaud_module->second.name == "LGAUD");
    assert(lgaud_module->second.base == 0);
    assert(lgaud_module->second.entry == 0);

    // Offline device queries match the SDK's no-device path and every v1.2
    // import ordinal is resolved to a C++ export rather than guest code.
    constexpr std::array<std::uint16_t, 15> usbd_ordinals = {
        4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    };
    for (const std::uint16_t ordinal : usbd_ordinals) {
        ps2::iop::Cpu call{};
        assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, ordinal, call));
    }

    ps2::iop::Cpu usbd_call{};
    usbd_call.r[4] = 0;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 13, usbd_call));
    assert(usbd_call.r[2] == 0x101u); // missing device

    usbd_call = {};
    usbd_call.r[4] = 0;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 9, usbd_call));
    assert(usbd_call.r[2] == 0xFFFFFFFFu); // OpenPipe returns -1 without a device

    usbd_call = {};
    usbd_call.r[4] = 0x1234u; // no endpoint exists
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 10, usbd_call));
    assert(usbd_call.r[2] == 0x102u); // bad pipe

    usbd_call = {};
    usbd_call.r[4] = 0;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 6, usbd_call));
    assert(usbd_call.r[2] == 0); // no static descriptor

    usbd_call = {};
    usbd_call.r[4] = 0;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 8, usbd_call));
    assert(usbd_call.r[2] == 0); // no device private data

    usbd_call = {};
    usbd_call.r[4] = 0;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 17, usbd_call));
    assert(usbd_call.r[2] == 0x101u); // report descriptor for absent device

    usbd_call = {};
    usbd_call.r[4] = 0x1234u;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 18, usbd_call));
    assert(usbd_call.r[2] == 0x102u); // no isochronous pipe

    // RegisterLdd mutates the real IOP driver structure, preserving the SDK's
    // next/prev/gp list contract while the empty device tree invokes no guest
    // probe/connect callback.
    constexpr std::uint32_t usbd_driver_a = 0x3000u;
    constexpr std::uint32_t usbd_driver_b = 0x3040u;
    ps2::iop::set_ram32(usbd_driver_a + 0x08u, 0x4000u); // name pointer
    ps2::iop::set_ram32(usbd_driver_b + 0x08u, 0x4010u);
    usbd_call = {};
    usbd_call.r[4] = usbd_driver_a;
    usbd_call.r[28] = 0x5000u;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 4, usbd_call));
    assert(usbd_call.r[2] == 0);
    assert(ps2::iop::ram32(usbd_driver_a + 0x2Cu) == 0x5000u);

    usbd_call = {};
    usbd_call.r[4] = usbd_driver_a;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 4, usbd_call));
    assert(usbd_call.r[2] == 0x121u); // already registered head

    usbd_call = {};
    usbd_call.r[4] = usbd_driver_b;
    usbd_call.r[28] = 0x5100u;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 4, usbd_call));
    assert(usbd_call.r[2] == 0);
    assert(ps2::iop::ram32(usbd_driver_a + 0x00u) == usbd_driver_b);
    assert(ps2::iop::ram32(usbd_driver_b + 0x04u) == usbd_driver_a);
    assert(ps2::iop::ram32(usbd_driver_b + 0x2Cu) == 0x5100u);

    usbd_call = {};
    usbd_call.r[4] = usbd_driver_a;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 5, usbd_call));
    assert(usbd_call.r[2] == 0);
    assert(ps2::iop::ram32(usbd_driver_b + 0x04u) == 0);

    usbd_call = {};
    usbd_call.r[4] = usbd_driver_a;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 5, usbd_call));
    assert(usbd_call.r[2] == 0x104u); // no longer registered

    constexpr std::uint32_t usbd_autoloader = 0x3080u;
    ps2::iop::set_ram32(usbd_autoloader + 0x08u, 0x4020u);
    usbd_call = {};
    usbd_call.r[4] = usbd_autoloader;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 14, usbd_call));
    assert(usbd_call.r[2] == 0);
    usbd_call = {};
    usbd_call.r[4] = usbd_autoloader;
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 14, usbd_call));
    assert(usbd_call.r[2] == 0x121u); // only one autoloader is supported
    usbd_call = {};
    assert(ps2::iop::kernel::invoke_native_import("usbd", 0x0102, 15, usbd_call));
    assert(usbd_call.r[2] == 0);

    // The retail LIBSD IOPMOD name is Sound_Device_Library. Its replacement
    // registers every libsd v1.5 ordinal imported by the selected SDRDRV.IRX.
    const auto libsd_irx = make_iopmod_elf("Sound_Device_Library");
    const int libsd_id = ps2::iop::load_module_buffer(libsd_irx, "buffer", "");
    assert(libsd_id > 0);
    const auto libsd_module = ps2::iop::kernel::modules().find(libsd_id);
    assert(libsd_module != ps2::iop::kernel::modules().end());
    assert(libsd_module->second.name == "LIBSD");
    ps2::iop::Cpu libsd_call{};
    libsd_call.r[4] = 0; // channel 0, voice 0 VOLL
    libsd_call.r[5] = 0x1234;
    assert(ps2::iop::kernel::invoke_native_import("libsd", 0x0105, 5, libsd_call));
    libsd_call = {};
    libsd_call.r[4] = 0;
    assert(ps2::iop::kernel::invoke_native_import("libsd", 0x0105, 6, libsd_call));
    assert(libsd_call.r[2] == 0x1234);

    // LIBSD's native batch service implements the PS2SDK EE-side DMA batch
    // commands without entering the guest SIF library or IOP interpreter.
    constexpr std::uint32_t libsd_batch = 0x5200u;
    constexpr std::uint32_t libsd_returns = 0x5300u;
    constexpr std::uint32_t ee_writeee_destination = 0x00100000u;
    constexpr std::uint32_t ee_eereturn_destination = 0x00100100u;
    ps2::iop::set_ram32(libsd_batch + 0u, 0x12340006u); // WRITEEE, entry=0x1234
    ps2::iop::set_ram32(libsd_batch + 4u, ee_writeee_destination);
    ps2::iop::set_ram32(libsd_batch + 8u, 0x00080007u); // EERETURN, 8-byte transfer
    ps2::iop::set_ram32(libsd_batch + 12u, ee_eereturn_destination);
    ps2::iop::set_ram32(libsd_returns + 0u, 0xAABBCCDDu);
    ps2::iop::set_ram32(libsd_returns + 4u, 0x55667788u);
    ps2::st32(ee_writeee_destination, 0);
    ps2::st32(ee_eereturn_destination, 0xFFFFFFFFu);
    ps2::st32(ee_eereturn_destination + 4u, 0xFFFFFFFFu);
    libsd_call = {};
    libsd_call.r[4] = libsd_batch;
    libsd_call.r[5] = libsd_returns;
    libsd_call.r[6] = 2;
    assert(ps2::iop::kernel::invoke_native_import("libsd", 0x0105, 15, libsd_call));
    assert(libsd_call.r[2] == 2);
    assert(ps2::ld32(ee_writeee_destination) == 0x1234u);
    // WRITEEE stores a zero result at rets[0] before EERETURN copies the
    // requested byte count from the start of the IOP return array.
    assert(ps2::ld32(ee_eereturn_destination) == 0u);
    assert(ps2::ld32(ee_eereturn_destination + 4u) == 0x55667788u);

    // Invalid EE ranges and IOP source ranges fail before touching memory.
    ps2::iop::set_ram32(libsd_batch, 0x12340006u);
    ps2::iop::set_ram32(libsd_batch + 4u, ps2::kRamSize - 2u);
    libsd_call = {};
    libsd_call.r[4] = libsd_batch;
    libsd_call.r[5] = libsd_returns;
    libsd_call.r[6] = 1;
    assert(ps2::iop::kernel::invoke_native_import("libsd", 0x0105, 15, libsd_call));
    assert(libsd_call.r[2] == static_cast<std::uint32_t>(-1));

    ps2::iop::set_ram32(libsd_batch, 0x00080007u);
    ps2::iop::set_ram32(libsd_batch + 4u, ee_eereturn_destination);
    libsd_call = {};
    libsd_call.r[4] = libsd_batch;
    libsd_call.r[5] = (8u << 20) - 4u; // outer return slot fits, 8-byte DMA source does not
    libsd_call.r[6] = 1;
    assert(ps2::iop::kernel::invoke_native_import("libsd", 0x0105, 15, libsd_call));
    assert(libsd_call.r[2] == static_cast<std::uint32_t>(-1));

    constexpr std::array<std::uint16_t, 26> libsd_ordinals = {
        4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 23, 24, 25, 26, 27, 30, 31, 32, 33,
    };
    for (const std::uint16_t ordinal : libsd_ordinals) {
        ps2::iop::Cpu call{};
        assert(ps2::iop::kernel::invoke_native_import("libsd", 0x0105, ordinal, call));
    }

    return 0;
}
