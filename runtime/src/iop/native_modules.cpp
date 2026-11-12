#include "iop.hpp"
#include "kernel.hpp"
#include "mwfilefr.hpp"
#include "sio2.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace ps2::iop::kernel {

namespace {

constexpr std::uint32_t kSio2Base = 0x1F808200u;
constexpr std::uint32_t kTransferDataSize = 0x94u;
constexpr std::uint32_t kPortCtrl1Offset = 0x04u;
constexpr std::uint32_t kPortCtrl2Offset = 0x14u;
constexpr std::uint32_t kStat70Offset = 0x24u;
constexpr std::uint32_t kRegDataOffset = 0x28u;
constexpr std::uint32_t kStat74Offset = 0x68u;
constexpr std::uint32_t kInSizeOffset = 0x6Cu;
constexpr std::uint32_t kOutSizeOffset = 0x70u;
constexpr std::uint32_t kInPtrOffset = 0x74u;
constexpr std::uint32_t kOutPtrOffset = 0x78u;
constexpr std::uint32_t kInDmaOffset = 0x7Cu;
constexpr std::uint32_t kOutDmaOffset = 0x88u;

bool g_sio2man_started = false;
bool g_native_modules_registered = false;

bool valid_iop_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = phys(address);
    return physical < kRamSize && size <= static_cast<std::uint64_t>(kRamSize - physical);
}

bool fail_transfer(Cpu &c, const char *reason) {
    std::fprintf(stderr, "[iop-native-stop] SIO2MAN native service rejected call: %s\n", reason);
    if (native_strict()) {
        std::fflush(stderr);
        std::abort();
    }
    c.r[2] = 0;
    return false;
}

std::uint32_t field32(std::uint32_t base, std::uint32_t offset) {
    return read32(base + offset);
}

void store32(std::uint32_t base, std::uint32_t offset, std::uint32_t value) {
    write32(base + offset, value);
}

void native_sio2_transfer(Cpu &c) {
    if (!g_sio2man_started) {
        fail_transfer(c, "module service used before SIO2MAN load");
        return;
    }

    const std::uint32_t td = c.r[4];
    if (td == 0 || !valid_iop_range(td, kTransferDataSize)) {
        fail_transfer(c, "transfer-data record is outside IOP RAM");
        return;
    }

    const std::uint32_t in_size = field32(td, kInSizeOffset);
    const std::uint32_t out_size = field32(td, kOutSizeOffset);
    const std::uint32_t in_ptr = field32(td, kInPtrOffset);
    const std::uint32_t out_ptr = field32(td, kOutPtrOffset);
    const std::uint32_t in_dma_addr = field32(td, kInDmaOffset);
    const std::uint32_t in_dma_words = field32(td, kInDmaOffset + 4u);
    const std::uint32_t in_dma_blocks = field32(td, kInDmaOffset + 8u);
    const std::uint32_t out_dma_addr = field32(td, kOutDmaOffset);
    const std::uint32_t out_dma_words = field32(td, kOutDmaOffset + 4u);
    const std::uint32_t out_dma_blocks = field32(td, kOutDmaOffset + 8u);

    constexpr std::uint64_t kMaximumTransferBytes = kRamSize;
    const auto dma_byte_count = [](std::uint32_t words, std::uint32_t blocks) {
        const std::uint64_t total_words = static_cast<std::uint64_t>(words) * blocks;
        return total_words > kMaximumTransferBytes / 4u ? kMaximumTransferBytes + 1u : total_words * 4u;
    };
    const std::uint64_t in_dma_bytes = dma_byte_count(in_dma_words, in_dma_blocks);
    const std::uint64_t out_dma_bytes = dma_byte_count(out_dma_words, out_dma_blocks);
    if (in_size > kMaximumTransferBytes || out_size > kMaximumTransferBytes ||
        (in_size && (!in_ptr || !valid_iop_range(in_ptr, in_size))) ||
        (out_size && (!out_ptr || !valid_iop_range(out_ptr, out_size))) ||
        (in_dma_addr && (in_dma_bytes > kMaximumTransferBytes || !valid_iop_range(in_dma_addr, in_dma_bytes))) ||
        (out_dma_addr && (out_dma_bytes > kMaximumTransferBytes || !valid_iop_range(out_dma_addr, out_dma_bytes)))) {
        fail_transfer(c, "buffer or DMA range is invalid");
        return;
    }

    // This is the SIO2MAN send_td / transfer / recv_td contract, served by the
    // existing host controller and card backend. No IOP module instructions
    // or HLE import stubs are executed here.
    sio2::write(kSio2Base + 0x68u, 0xCu);
    for (std::uint32_t port = 0; port < 4; ++port) {
        sio2::write(kSio2Base + 0x40u + port * 8u, field32(td, kPortCtrl1Offset + port * 4u));
        sio2::write(kSio2Base + 0x44u + port * 8u, field32(td, kPortCtrl2Offset + port * 4u));
    }
    for (std::uint32_t index = 0; index < 16; ++index)
        sio2::write(kSio2Base + index * 4u, field32(td, kRegDataOffset + index * 4u));

    for (std::uint32_t index = 0; index < in_size; ++index)
        sio2::write(kSio2Base + 0x60u, read8(in_ptr + index));

    if (in_dma_addr) sio2::arm_dma(11, in_dma_addr, in_dma_words, in_dma_blocks);
    if (out_dma_addr) sio2::arm_dma(12, out_dma_addr, out_dma_words, out_dma_blocks);

    // The current host SIO2 backend completes the transfer synchronously when
    // the start bit is written, so the guest semaphore/interrupt wait is not
    // part of this native service path.
    sio2::write(kSio2Base + 0x68u, 1u);

    store32(td, 0x00u, sio2::read(kSio2Base + 0x6Cu));
    store32(td, kStat70Offset, sio2::read(kSio2Base + 0x70u));
    store32(td, kStat74Offset, sio2::read(kSio2Base + 0x74u));
    for (std::uint32_t index = 0; index < out_size; ++index)
        write8(out_ptr + index, static_cast<std::uint8_t>(sio2::read(kSio2Base + 0x64u)));

    c.r[2] = 1;
}

void native_sio2_transfer_reset(Cpu &c) { c.r[2] = 0; }

void native_sio2_transfer_init(Cpu &c) { c.r[2] = 0; }

void native_sio2_wait_for_intr(Cpu &c) {
    // SIO2 transfers are completed before returning from native_sio2_transfer.
    c.r[2] = 0;
}

void native_sio2_stat70_get(Cpu &c) { c.r[2] = sio2::read(kSio2Base + 0x70u); }

void native_sio2_mtap_change_slot(Cpu &c) {
    const std::uint32_t slots = c.r[4];
    if (!valid_iop_range(slots, 8u * sizeof(std::uint32_t))) {
        fail_transfer(c, "multitap slot array is outside IOP RAM");
        return;
    }
    std::uint32_t available = 0;
    for (std::uint32_t port = 0; port < 4; ++port) {
        const std::int32_t current = static_cast<std::int32_t>(read32(slots + port * 4u));
        const bool next_slot_exists = static_cast<std::int64_t>(current) + 1 < 2;
        write32(slots + (port + 4u) * 4u, next_slot_exists ? 1u : 0u);
        available += next_slot_exists ? 1u : 0u;
    }
    c.r[2] = available == 4 ? 1u : 0u;
}

void native_sio2_mtap_get_slot_max(Cpu &c) { c.r[2] = 1; }

void native_sio2_mtap_update_slots(Cpu &c) { c.r[2] = 0; }

NativeIopLibrary sio2man_library(std::uint16_t version, bool memory_card_imports) {
    NativeIopLibrary library;
    library.name = "sio2man";
    library.version = version;
    library.exports = {
        {11, native_sio2_stat70_get},
        {46, native_sio2_transfer_init},
        {51, native_sio2_transfer},
        {52, native_sio2_transfer_reset},
        {57, native_sio2_mtap_change_slot},
        {58, native_sio2_mtap_get_slot_max},
        {60, native_sio2_mtap_update_slots},
    };
    if (memory_card_imports) {
        library.exports.push_back({24, native_sio2_transfer_init});
        library.exports.push_back({25, native_sio2_transfer});
        library.exports.push_back({26, native_sio2_transfer_reset});
        library.exports.push_back({59, native_sio2_mtap_get_slot_max});
        library.exports.push_back({67, native_sio2_wait_for_intr});
    }
    return library;
}

} // namespace

void install_native_modules() {
    g_sio2man_started = false;
    register_native_mwfilefr_job();
    if (g_native_modules_registered) return;

    NativeIopModule sio2man;
    sio2man.name = "SIO2MAN";
    sio2man.start = [](const std::string &) {
        g_sio2man_started = true;
        return true;
    };
    // The selected disc's PADMAN imports SIO2MAN v2.4 ordinals 11, 46, 51,
    // 52, 57, 58 and 60; MCMAN imports v2.7 ordinals 24, 25, 26, 57, 59
    // and 67. These are explicit per-disc import contracts, not a general
    // library-version compatibility rule.
    sio2man.libraries.push_back(sio2man_library(0x0204u, false));
    sio2man.libraries.push_back(sio2man_library(0x0207u, true));
    register_native_module(std::move(sio2man));

    NativeIopModule mwfilefr;
    mwfilefr.name = "MWFILEFR";
    // module_name_of() strips the extension from disc paths, so MWFILEFR.IRX
    // already resolves through the canonical MWFILEFR key. Only the distinct
    // IOPMOD-header spelling needs an explicit alias.
    mwfilefr.aliases = {"mwFile_module"};
    mwfilefr.start = [](const std::string &) {
        install_native_mwfilefr_service();
        return true;
    };
    register_native_module(std::move(mwfilefr));
    g_native_modules_registered = true;
}

} // namespace ps2::iop::kernel
