#include "iop/kernel.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

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

std::vector<std::uint8_t> make_native_module_elf(const std::string &name) {
    std::vector<std::uint8_t> data(256);
    data[0] = 0x7F;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 1;
    data[5] = 1;
    put16(data, 18, 8); // EM_MIPS
    put32(data, 28, 52);
    put16(data, 42, 32);
    put16(data, 44, 1);
    put32(data, 52, 0x70000080u); // PT_IOPMOD
    put32(data, 56, 128);
    std::memcpy(data.data() + 154, name.c_str(), name.size() + 1);
    return data;
}

std::vector<std::uint8_t> make_importer_elf(const std::string &module_name,
                                           const std::string &library_name,
                                           std::uint16_t version,
                                           std::uint16_t ordinal) {
    constexpr std::size_t load_offset = 0x100;
    constexpr std::size_t load_size = 0x100;
    constexpr std::size_t iopmod_offset = 0x80;
    constexpr std::size_t import_offset = 0x40;
    constexpr std::size_t stub_offset = import_offset + 20;
    std::vector<std::uint8_t> data(load_offset + load_size);
    data[0] = 0x7F;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 1;
    data[5] = 1;
    put16(data, 16, 2);  // ET_EXEC
    put16(data, 18, 8);  // EM_MIPS
    put32(data, 28, 52);
    put16(data, 42, 32);
    put16(data, 44, 2);

    const std::size_t load_ph = 52;
    put32(data, load_ph, 1); // PT_LOAD
    put32(data, load_ph + 4, static_cast<std::uint32_t>(load_offset));
    put32(data, load_ph + 16, static_cast<std::uint32_t>(load_size));
    put32(data, load_ph + 20, static_cast<std::uint32_t>(load_size));
    put32(data, load_ph + 24, 7);
    put32(data, load_ph + 28, 0x100);

    const std::size_t iopmod_ph = load_ph + 32;
    put32(data, iopmod_ph, 0x70000080u); // PT_IOPMOD
    put32(data, iopmod_ph + 4, static_cast<std::uint32_t>(iopmod_offset));
    put32(data, iopmod_offset + 12, static_cast<std::uint32_t>(load_size));
    std::memcpy(data.data() + iopmod_offset + 26, module_name.c_str(), module_name.size() + 1);

    const std::size_t import = load_offset + import_offset;
    put32(data, import, 0x41E00000u);
    put32(data, import + 8, version);
    std::memcpy(data.data() + import + 12, library_name.c_str(), library_name.size() + 1);
    put32(data, load_offset + stub_offset, 0x03E00008u); // jr ra
    put32(data, load_offset + stub_offset + 4, 0x24000000u | ordinal); // addiu v0, zero, ordinal
    return data;
}

} // namespace

int main() {
    int native_calls = 0;
    ps2::iop::kernel::NativeIopModule provider;
    provider.name = "SYNTHETIC_NATIVE_IMPORT_PROVIDER";
    ps2::iop::kernel::NativeIopLibrary exports;
    exports.name = "native1";
    exports.version = 0x0101;
    exports.exports.push_back({7, [&native_calls](ps2::iop::Cpu &c) {
                                  ++native_calls;
                                  c.r[2] = c.r[4] + c.r[5];
                              }});
    provider.libraries.push_back(std::move(exports));
    ps2::iop::kernel::register_native_module(std::move(provider));
    ps2::iop::reset();

    constexpr std::uint32_t kImportImageOffset = 0x40;
    constexpr std::uint32_t kImportStubOffset = kImportImageOffset + 20;
    const auto importer = make_importer_elf("SYNTHETIC_IMPORTER", "native1", 0x0101, 7);
    const int importer_id = ps2::iop::load_module_buffer(importer, "buffer", "");
    assert(importer_id > 0);

    const auto module = ps2::iop::kernel::modules().find(importer_id);
    assert(module != ps2::iop::kernel::modules().end());
    assert(module->second.base != 0);
    const std::uint32_t stub = module->second.base + kImportStubOffset;
    const std::uint32_t unresolved = ps2::iop::ram32(stub);
    assert((unresolved >> 26) == ps2::iop::kHleOpcode);
    assert(ps2::iop::ram32(stub + 4) == (0x24000000u | 7u));

    const auto provider_elf = make_native_module_elf("SYNTHETIC_NATIVE_IMPORT_PROVIDER");
    assert(ps2::iop::load_module_buffer(provider_elf, "buffer", "") > 0);
    const std::uint32_t linked = ps2::iop::ram32(stub);
    assert((linked >> 26) == ps2::iop::kHleOpcode);
    assert(linked != unresolved);

    // Enter the relinked import stub as a guest call. It dispatches to the
    // registered C++ export and returns without executing an imported guest body.
    assert(ps2::iop::kernel::call_guest(stub, 19, 23) == 42);
    assert(native_calls == 1);
    return 0;
}
