#include "libsd_native.hpp"

#include "iop.hpp"
#include "spu2.hpp"
#include "libsd_effect_data.hpp"
#include "ps2/memory.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

namespace ps2::iop::libsd_native {

namespace {

constexpr std::uint32_t kSpuBase = 0x1F900000u;
constexpr std::uint32_t kSpuRamBytes = 2u * 1024u * 1024u;
constexpr std::uint32_t kSpuRamMask = kSpuRamBytes - 1u;
constexpr std::uint32_t kDmaWrite = 0;
constexpr std::uint32_t kDmaRead = 1;
constexpr std::uint32_t kDmaStop = 2;
constexpr std::uint32_t kDmaWriteFrom = 3;
constexpr std::uint32_t kDmaLoop = 0x10;
constexpr std::uint32_t kDmaIoMode = 8;

// Offsets correspond to the PS2SDK libsd ParamRegList order. The register
// table is used only as data: calls are served by the host SPU2 core.
constexpr std::array<std::uint16_t, 41> kParamRegisterOffsets = {
    0x000, 0x002, 0x004, 0x006, 0x008, 0x00A, 0x00C, 0x00E,
    0x198,
    0x760, 0x762, 0x764, 0x766, 0x768, 0x76A, 0x76C, 0x76E, 0x770, 0x772,
    0x180, 0x184, 0x1A0, 0x1A4, 0x340, 0x188, 0x18C, 0x190, 0x194,
    0x2E0, 0x33C, 0x1A8, 0x19C, 0x1C0, 0x1C4, 0x1C8, 0x19A, 0x1A8,
    0x1AC, 0x1AE, 0x1B0, 0x334,
};

struct EffectAttr {
    std::uint32_t core = 0;
    std::uint32_t mode = 0;
    std::int16_t depth_l = 0;
    std::int16_t depth_r = 0;
    std::int32_t delay = 0;
    std::int32_t feedback = 0;
};

std::array<EffectAttr, 2> g_effect_attr{};
std::array<std::uint32_t, 2> g_effect_address{};
std::array<std::uint32_t, 2> g_voice_transfer_io_mode{};
std::array<std::uint32_t, 2> g_voice_transfer_status{1, 1};
std::array<bool, 2> g_voice_transfer_complete{};
std::array<std::uint32_t, 2> g_block_transfer_buffer{};
std::array<std::uint32_t, 2> g_block_transfer_address{};
std::array<std::uint32_t, 2> g_block_transfer_size{};
std::array<bool, 2> g_block_transfer_active{};
std::array<std::uint32_t, 2> g_transfer_intr_handler{};
std::array<std::uint32_t, 2> g_transfer_intr_argument{};
std::array<std::uint32_t, 2> g_transfer_callback{};
std::uint32_t g_spu2_intr_handler = 0;
std::uint32_t g_spu2_intr_argument = 0;
std::uint32_t g_irq_callback = 0;
std::uint32_t g_spdif_settings = 0;

std::uint32_t arg(const Cpu &cpu, unsigned index) {
    if (index < 4) return cpu.r[4 + index];
    const std::uint32_t address = cpu.r[29] + 16u + (index - 4u) * 4u;
    const std::uint32_t physical = phys(address);
    if (physical > kRamSize - 4u) return 0;
    return read32(address);
}

bool valid_iop_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = phys(address);
    return physical < kRamSize && size <= static_cast<std::uint64_t>(kRamSize - physical);
}

std::uint32_t ee_physical(std::uint32_t address) {
    return address & (ps2::kRamSize - 1u);
}

bool valid_ee_range(std::uint32_t address, std::uint32_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = ee_physical(address);
    return physical < ps2::kRamSize && size <= ps2::kRamSize - physical;
}

bool copy_to_ee(std::uint32_t destination, const void *source, std::uint32_t size) {
    if ((size != 0 && !source) || !valid_ee_range(destination, size)) return false;
    if (size != 0) std::memcpy(ps2::g_ram + ee_physical(destination), source, size);
    return true;
}

void set_result(Cpu &cpu, std::int32_t result) { cpu.r[2] = static_cast<std::uint32_t>(result); }
void set_u32_result(Cpu &cpu, std::uint32_t result) { cpu.r[2] = result; }

std::uint32_t param_address(std::uint32_t entry) {
    const std::uint32_t index = (entry >> 8) & 0xFFu;
    if (index >= kParamRegisterOffsets.size()) return 0xFFFFFFFFu;
    const std::uint32_t core = entry & 1u;
    const std::uint32_t core_stride = (entry & 0x80u) ? 0x28u : 0x400u;
    const std::uint32_t voice_offset = (entry & 0x3Eu) << 3;
    return kParamRegisterOffsets[index] + core * core_stride + voice_offset;
}

bool set_param(std::uint32_t entry, std::uint16_t value) {
    const std::uint32_t address = param_address(entry);
    if (address == 0xFFFFFFFFu) return false;
    spu2::write(kSpuBase + address, value, 2);
    return true;
}

bool get_param(std::uint32_t entry, std::uint16_t &value) {
    const std::uint32_t address = param_address(entry);
    if (address == 0xFFFFFFFFu) return false;
    value = static_cast<std::uint16_t>(spu2::read(kSpuBase + address, 2));
    return true;
}

bool set_switch(std::uint32_t entry, std::uint32_t value) {
    const std::uint32_t index = (entry >> 8) & 0xFFu;
    if (index >= kParamRegisterOffsets.size()) return false;
    const std::uint32_t address = kParamRegisterOffsets[index] + ((entry & 1u) << 10);
    spu2::write(kSpuBase + address, value & 0xFFFFu, 2);
    spu2::write(kSpuBase + address + 2u, (value >> 16) & 0xFFu, 2);
    return true;
}

bool get_switch(std::uint32_t entry, std::uint32_t &value) {
    const std::uint32_t index = (entry >> 8) & 0xFFu;
    if (index >= kParamRegisterOffsets.size()) return false;
    const std::uint32_t address = kParamRegisterOffsets[index] + ((entry & 1u) << 10);
    value = (spu2::read(kSpuBase + address, 2) & 0xFFFFu) |
            ((spu2::read(kSpuBase + address + 2u, 2) & 0xFFFFu) << 16);
    return true;
}

bool set_addr(std::uint32_t entry, std::uint32_t value) {
    const std::uint32_t index = (entry >> 8) & 0xFFu;
    if (index >= kParamRegisterOffsets.size()) return false;
    const std::uint32_t address = kParamRegisterOffsets[index] + ((entry & 1u) << 10) + ((entry & 0x3Eu) * 6u);
    spu2::write(kSpuBase + address, (value >> 17) & 0xFFFFu, 2);
    if (index != 29u) spu2::write(kSpuBase + address + 2u, (value >> 1) & 0xFFF8u, 2);
    return true;
}

bool get_addr(std::uint32_t entry, std::uint32_t &value) {
    const std::uint32_t index = (entry >> 8) & 0xFFu;
    if (index >= kParamRegisterOffsets.size()) return false;
    const std::uint32_t address = kParamRegisterOffsets[index] + ((entry & 1u) << 10) + ((entry & 0x3Eu) * 6u);
    const std::uint32_t high = (spu2::read(kSpuBase + address, 2) & 0xFFFFu) << 17;
    if (index == 29u) {
        value = high | 0x1FFFFu;
    } else {
        value = high | ((spu2::read(kSpuBase + address + 2u, 2) & 0xFFFFu) << 1) | 1u;
    }
    return true;
}

void set_spdif_mode(std::uint16_t value) {
    std::uint16_t out = static_cast<std::uint16_t>(spu2::read(kSpuBase + 0x7C0u, 2));
    std::uint16_t mode = static_cast<std::uint16_t>(spu2::read(kSpuBase + 0x7C6u, 2));
    switch (value & 0xFu) {
    case 0: mode &= 0xFFFDu; out = static_cast<std::uint16_t>((value & 0xFEF7u) | 0x20u); break;
    case 1: mode |= 2u; out = static_cast<std::uint16_t>((out & 0xFFD7u) | 0x100u); break;
    case 2: out &= 0xFED7u; break;
    case 0xF: out = static_cast<std::uint16_t>((out & 0xFEDFu) | 8u); break;
    default: return;
    }
    if (value & 0x80u) mode |= 0x8000u;
    else mode &= 0x7FFFu;
    switch (value & 0xF00u) {
    case 0x800: spu2::write(kSpuBase + 0x7C8u, 0x200u, 2); mode |= 0x1800u; break;
    case 0x400: spu2::write(kSpuBase + 0x7C8u, 0, 2); mode &= 0xE7FFu; break;
    default:
        spu2::write(kSpuBase + 0x7C8u, 0x200u, 2);
        mode = static_cast<std::uint16_t>((mode & 0xE7FFu) | 0x800u);
        break;
    }
    spu2::write(kSpuBase + 0x7C0u, out, 2);
    spu2::write(kSpuBase + 0x7C6u, mode, 2);
    g_spdif_settings = value;
}

bool set_core_attr(std::uint32_t entry, std::uint16_t value) {
    const std::uint32_t core = entry & 1u;
    const std::uint32_t address = 0x19Au + core * 0x400u;
    const std::uint32_t selector = entry & ~1u;
    std::uint16_t current = static_cast<std::uint16_t>(spu2::read(kSpuBase + address, 2));
    if (selector == 0x08u) {
        current = static_cast<std::uint16_t>((current - 0x3F01u) | ((value & 0x3Fu) << 8));
        spu2::write(kSpuBase + address, current, 2);
        return true;
    }
    if (selector == 0x0Au) {
        set_spdif_mode(value);
        return true;
    }
    constexpr std::array<unsigned, 3> shifts = {7, 6, 14};
    if (selector < 0x02u || selector > 0x06u || (selector & 1u)) return false;
    const unsigned shift = shifts[(selector >> 1) - 1u];
    current = static_cast<std::uint16_t>((current & ~(1u << shift)) | ((value & 1u) << shift));
    spu2::write(kSpuBase + address, current, 2);
    return true;
}

bool get_core_attr(std::uint32_t entry, std::uint16_t &value) {
    const std::uint32_t core = entry & 1u;
    const std::uint32_t address = 0x19Au + core * 0x400u;
    const std::uint32_t selector = entry & ~1u;
    const std::uint16_t current = static_cast<std::uint16_t>(spu2::read(kSpuBase + address, 2));
    if (selector == 0x08u) { value = static_cast<std::uint16_t>((current >> 8) & 0x3Fu); return true; }
    if (selector == 0x0Au) { value = static_cast<std::uint16_t>(g_spdif_settings); return true; }
    constexpr std::array<unsigned, 3> shifts = {7, 6, 14};
    if (selector < 0x02u || selector > 0x06u || (selector & 1u)) return false;
    value = static_cast<std::uint16_t>((current >> shifts[(selector >> 1) - 1u]) & 1u);
    return true;
}

void set_tsa(std::uint32_t core, std::uint32_t address) {
    const std::uint32_t base = kSpuBase + 0x1A8u + (core & 1u) * 0x400u;
    spu2::write(base, (address >> 17) & 0xFFFFu, 2);
    spu2::write(base + 2u, (address >> 1) & 0xFFFFu, 2);
}

std::int32_t dma_transfer(std::uint32_t core, std::uint32_t iop_address, std::uint32_t spu_address,
                          std::uint32_t bytes, bool to_spu) {
    if (bytes == 0) return 0;
    if (bytes > kSpuRamBytes || (bytes & 3u) || !valid_iop_range(iop_address, bytes)) return -1;
    set_tsa(core, spu_address & kSpuRamMask);
    spu2::dma(static_cast<int>(core & 1u), iop_address, bytes / 4u, to_spu, (core & 1u) ? 7 : 4);
    return static_cast<std::int32_t>(bytes);
}

std::int32_t voice_transfer(std::uint32_t channel, std::uint32_t mode, std::uint32_t iop_address,
                            std::uint32_t spu_address, std::uint32_t bytes) {
    const std::uint32_t core = channel & 1u;
    const std::uint32_t direction = mode & 3u;
    if (direction != kDmaWrite && direction != kDmaRead) return -1;
    g_voice_transfer_io_mode[core] = (mode & kDmaIoMode) != 0;
    std::int32_t result = -1;
    if (g_voice_transfer_io_mode[core]) {
        if (direction == kDmaWrite && !(bytes & 1u) && bytes <= kSpuRamBytes && valid_iop_range(iop_address, bytes)) {
            set_tsa(core, spu_address & kSpuRamMask);
            const std::uint32_t std_address = kSpuBase + 0x1ACu + core * 0x400u;
            for (std::uint32_t offset = 0; offset < bytes; offset += 2u)
                spu2::write(std_address, read16(iop_address + offset), 2);
            result = static_cast<std::int32_t>(bytes);
        }
    } else {
        result = dma_transfer(core, iop_address, spu_address, bytes, direction == kDmaWrite);
    }
    g_voice_transfer_status[core] = 0;
    g_voice_transfer_complete[core] = result >= 0;
    return result;
}

std::uint32_t voice_transfer_status(std::uint32_t channel, std::uint32_t wait) {
    const std::uint32_t core = channel & 1u;
    if (g_voice_transfer_io_mode[core] || g_voice_transfer_status[core]) return 1;
    if (!g_voice_transfer_complete[core]) return 0;
    g_voice_transfer_complete[core] = false;
    if (wait & 1u) {
        g_voice_transfer_status[core] = 1;
        return 1;
    }
    g_voice_transfer_status[core] = 1;
    return 0;
}

std::uint32_t stop_transfer(std::uint32_t channel) {
    const std::uint32_t core = channel & 1u;
    const std::uint32_t address = g_block_transfer_active[core] ? g_block_transfer_address[core] : 0u;
    spu2::dma_stop(static_cast<int>(core));
    g_block_transfer_active[core] = false;
    g_voice_transfer_complete[core] = true;
    return (g_block_transfer_buffer[core] << 24) | (address & 0x00FFFFFFu);
}

std::int32_t block_transfer(std::uint32_t channel, std::uint32_t mode, std::uint32_t iop_address,
                            std::uint32_t bytes, std::uint32_t start_address) {
    const std::uint32_t core = channel & 1u;
    const std::uint32_t direction = mode & 3u;
    if (direction == kDmaStop) {
        return -1; // handled by the ABI wrapper, which also returns the prior DMA address
    }
    if (direction != kDmaWrite && direction != kDmaRead && direction != kDmaWriteFrom) return -1;
    std::uint32_t transfer_address = iop_address;
    std::uint32_t transfer_size = bytes;
    if (direction == kDmaWriteFrom) {
        if (start_address == 0) start_address = iop_address;
        std::uint32_t offset = start_address - iop_address;
        if (offset > bytes) {
            if (mode & kDmaLoop) {
                offset -= bytes;
                g_block_transfer_buffer[core] = 1;
            } else return -1;
        } else {
            g_block_transfer_buffer[core] = 0;
        }
        if (offset & 1023u) offset += 1024u;
        transfer_address = iop_address + (transfer_size * g_block_transfer_buffer[core]) + offset;
    } else {
        g_block_transfer_buffer[core] = 0;
    }
    if (mode & kDmaLoop) transfer_size /= 2u;
    if (!valid_iop_range(transfer_address, transfer_size)) return -1;

    std::int32_t result = -1;
    if (direction == kDmaRead) {
        const std::uint32_t spu_address = ((mode & 0xF00u) << 2) + 0x800u;
        result = dma_transfer(core, transfer_address, spu_address, transfer_size, false);
    } else {
        result = dma_transfer(core, transfer_address, 0, transfer_size, true);
        if (result >= 0 && (mode & kDmaLoop)) {
            const std::uint32_t next = transfer_address + transfer_size;
            if (valid_iop_range(next, transfer_size)) {
                const std::int32_t second = dma_transfer(core, next, 0, transfer_size, true);
                if (second < 0) result = second;
            } else {
                result = -1;
            }
        }
    }
    g_block_transfer_address[core] = transfer_address;
    g_block_transfer_size[core] = transfer_size;
    g_block_transfer_active[core] = false; // the SPU2 core consumes the copied data synchronously
    return result < 0 ? result : 0;
}

bool read_effect_attr(std::uint32_t address, EffectAttr &attr) {
    if (address == 0 || !valid_iop_range(address, 20)) return false;
    attr.core = read32(address);
    attr.mode = read32(address + 4u);
    attr.depth_l = static_cast<std::int16_t>(read16(address + 8u));
    attr.depth_r = static_cast<std::int16_t>(read16(address + 10u));
    attr.delay = static_cast<std::int32_t>(read32(address + 12u));
    attr.feedback = static_cast<std::int32_t>(read32(address + 16u));
    return true;
}

void write_effect_attr(std::uint32_t address, const EffectAttr &attr) {
    write32(address, attr.core);
    write32(address + 4u, attr.mode);
    write16(address + 8u, static_cast<std::uint16_t>(attr.depth_l));
    write16(address + 10u, static_cast<std::uint16_t>(attr.depth_r));
    write32(address + 12u, static_cast<std::uint32_t>(attr.delay));
    write32(address + 16u, static_cast<std::uint32_t>(attr.feedback));
}

std::uint32_t get_eea(std::uint32_t core) {
    const std::uint32_t high = spu2::read(kSpuBase + 0x33Cu + (core & 1u) * 0x400u, 2);
    return (high << 17) | 0x1FFFFu;
}

void set_esa(std::uint32_t core, std::uint32_t address) {
    const std::uint32_t base = kSpuBase + 0x2E0u + (core & 1u) * 0x400u;
    spu2::write(base, address >> 17, 2);
    spu2::write(base + 2u, address >> 1, 2);
    g_effect_address[core & 1u] = address;
}

void clear_effect_work_area(std::uint32_t core, std::uint32_t channel, std::uint32_t mode) {
    if (mode > 9u) return;
    if (mode == 0) return;
    const std::uint32_t bytes = static_cast<std::uint32_t>(detail::kEffectSizes[mode]) << 4;
    const std::uint32_t end = get_eea(core);
    const std::uint32_t start = (end - (bytes - 2u)) & kSpuRamMask;
    std::uint16_t *ram = spu2::ram();
    const std::uint32_t first = (start >> 1) & (kSpuRamBytes / 2u - 1u);
    const std::uint32_t words = bytes / 2u;
    for (std::uint32_t i = 0; i < words; ++i) ram[(first + i) & (kSpuRamBytes / 2u - 1u)] = 0;
    (void)channel;
}

void write_effect_register(std::uint32_t address, std::uint32_t value) {
    spu2::write(kSpuBase + address, value >> 14, 2);
    spu2::write(kSpuBase + address + 2u, value << 2, 2);
}

void apply_effect_parameters(std::uint32_t core, std::uint32_t mode,
                             const std::array<std::uint16_t, 32> &data) {
    const std::uint32_t base = (core & 1u) * 0x400u;
    write_effect_register(0x2E4u + base, data[0]);
    write_effect_register(0x2E8u + base, data[1]);
    for (std::uint32_t i = 0; i < 8; ++i) spu2::write(kSpuBase + 0x2ECu + base + i * 2u, data[2u + i], 2);
    for (std::uint32_t i = 0; i < 20; ++i) write_effect_register(0x2FCu + base + i * 4u, data[10u + i]);
    const std::uint32_t master = kSpuBase + 0x760u + base;
    spu2::write(master + 0x24u, data[30], 2);
    spu2::write(master + 0x26u, data[31], 2);
    (void)mode;
}

std::int32_t apply_effect_attr(std::uint32_t core, const EffectAttr &input) {
    core &= 1u;
    std::uint32_t mode = input.mode;
    const bool clear = (mode & 0x100u) != 0;
    mode &= ~0x100u;
    if (mode > 9u) return -1;
    const std::uint32_t end = get_eea(core);
    const std::uint32_t bytes = static_cast<std::uint32_t>(detail::kEffectSizes[mode]) << 4;
    const std::uint32_t effect_address = (end - (bytes - 2u)) & kSpuRamMask;

    std::array<std::uint16_t, 32> data{};
    for (std::uint32_t i = 0; i < 16; ++i) {
        const std::uint32_t word = detail::kEffectParamWords[mode * 16u + i];
        data[i * 2u] = static_cast<std::uint16_t>(word);
        data[i * 2u + 1u] = static_cast<std::uint16_t>(word >> 16);
    }
    EffectAttr attr = input;
    attr.core = core;
    attr.mode = mode;
    if (mode == 7u) { attr.feedback = 0x80; attr.delay = 0x80; }
    else if (mode == 8u) { attr.feedback = 0; attr.delay = 0x80; }
    else { attr.feedback = 0; attr.delay = 0; }
    if (mode == 7u || mode == 8u) {
        const std::uint32_t delay = (static_cast<std::uint32_t>(input.delay) + 1u) << 5;
        const std::uint32_t feedback = ((static_cast<std::uint32_t>(input.feedback) << 7) + static_cast<std::uint32_t>(input.feedback)) << 1;
        data[7] = static_cast<std::uint16_t>(feedback);
        data[10] = static_cast<std::uint16_t>((delay << 1) - data[0]);
        data[11] = static_cast<std::uint16_t>(delay - data[1]);
        data[16] = static_cast<std::uint16_t>(data[17] + delay);
        data[12] = static_cast<std::uint16_t>(data[13] + delay);
        data[27] = static_cast<std::uint16_t>(data[29] + delay);
        data[26] = static_cast<std::uint16_t>(data[28] + delay);
    }
    const std::uint32_t core_attr_address = kSpuBase + 0x19Au + core * 0x400u;
    const bool effects_enabled = (spu2::read(core_attr_address, 2) & 0x80u) != 0;
    if (effects_enabled) spu2::write(core_attr_address, spu2::read(core_attr_address, 2) & ~0x80u, 2);
    if (clear && effects_enabled) clear_effect_work_area(core, 0, g_effect_attr[core].mode);
    g_effect_attr[core] = attr;
    spu2::write(kSpuBase + 0x764u + core * 0x28u, static_cast<std::uint16_t>(attr.depth_l), 2);
    spu2::write(kSpuBase + 0x766u + core * 0x28u, static_cast<std::uint16_t>(attr.depth_r), 2);
    apply_effect_parameters(core, mode, data);
    set_esa(core, effect_address);
    if (clear) clear_effect_work_area(core, 0, mode);
    if (effects_enabled) spu2::write(core_attr_address, spu2::read(core_attr_address, 2) | 0x80u, 2);
    return 0;
}

std::int32_t process_batch(std::uint32_t batch, std::uint32_t returns, std::uint32_t count, std::uint32_t voice_mask, bool extended) {
    if (count > kRamSize / 8u || !valid_iop_range(batch, static_cast<std::uint64_t>(count) * 8u) ||
        (returns && !valid_iop_range(returns, static_cast<std::uint64_t>(count) * 4u))) return -1;
    std::uint32_t return_index = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t record = batch + i * 8u;
        const std::uint32_t function = read16(record);
        const std::uint32_t entry = read16(record + 2u);
        const std::uint32_t value = read32(record + 4u);
        std::uint32_t result = 0;
        bool ok = true;
        auto perform = [&](std::uint32_t actual_entry) {
            std::uint16_t half = 0;
            std::uint32_t word = 0;
            switch (function) {
            case 0x01: return set_param(actual_entry, static_cast<std::uint16_t>(value));
            case 0x11: ok = get_param(actual_entry, half); result = half; return ok;
            case 0x02: return set_switch(actual_entry, value);
            case 0x12: ok = get_switch(actual_entry, word); result = word; return ok;
            case 0x03: return set_addr(actual_entry, value);
            case 0x13: ok = get_addr(actual_entry, word); result = word; return ok;
            case 0x04: return set_core_attr(actual_entry, static_cast<std::uint16_t>(value));
            case 0x14: ok = get_core_attr(actual_entry, half); result = half; return ok;
            case 0x05:
                if (!valid_iop_range(value, 4)) return false;
                write32(value, entry);
                return true;
            case 0x06: { // SD_BATCH_WRITEEE: copy the 32-bit entry value to EE RAM.
                return copy_to_ee(value, &entry, sizeof(entry));
            }
            case 0x07: { // SD_BATCH_EERETURN: copy the IOP return buffer to EE RAM.
                if (!valid_iop_range(returns, entry)) return false;
                return copy_to_ee(value, ps2::iop::ram_ptr(returns), entry);
            }
            default: return false;
            }
        };
        const bool voice_command = extended && (function == 0x01 || function == 0x11 || function == 0x03 || function == 0x13) &&
                                   ((entry & 0x3Eu) == 0x3Eu);
        if (voice_command) {
            for (std::uint32_t voice = 0; voice < 24; ++voice) {
                if (!(voice_mask & (1u << voice))) continue;
                const std::uint32_t voice_entry = (entry & 0xFFC1u) | (1u << (voice + 1u));
                if (!perform(voice_entry)) return -1 - static_cast<std::int32_t>(return_index);
                if (returns && (function == 0x11 || function == 0x13)) write32(returns + return_index * 4u, result);
                ++return_index;
            }
            if (function == 0x01 || function == 0x03) --return_index;
            continue;
        }
        if (!perform(entry)) return -1 - static_cast<std::int32_t>(return_index);
        if (returns && function >= 0x11 && function <= 0x14) write32(returns + return_index * 4u, result);
        else if (returns) write32(returns + return_index * 4u, 0);
        ++return_index;
    }
    return static_cast<std::int32_t>(return_index);
}

std::uint16_t note_to_pitch(std::uint32_t center_note, std::uint32_t center_fine,
                            std::uint32_t note, std::int32_t fine) {
    const double cents = (static_cast<double>(static_cast<std::int32_t>(note) - static_cast<std::int32_t>(center_note)) * 128.0 +
                          static_cast<double>(fine) + static_cast<double>(center_fine)) / 128.0;
    const double value = 4096.0 * std::pow(2.0, cents / 12.0);
    return static_cast<std::uint16_t>(std::clamp<long>(std::lround(value), 0, 0x3FFF));
}

std::uint16_t pitch_to_note(std::uint32_t center_note, std::uint32_t center_fine, std::uint32_t pitch) {
    pitch = std::min(pitch, 0x3FFFu);
    if (!pitch) return 0;
    const double semitones = 12.0 * std::log2(static_cast<double>(pitch) / 4096.0);
    const int whole = static_cast<int>(std::floor(semitones));
    const int fine = static_cast<int>(std::lround((semitones - whole) * 128.0 + center_fine));
    const int note = static_cast<int>(center_note) + whole + fine / 128;
    return static_cast<std::uint16_t>((std::clamp(note, 0, 255) << 8) | (std::clamp(fine & 0x7E, 0, 0x7E)));
}

void export_init(Cpu &cpu) {
    const std::uint32_t flag = arg(cpu, 0) & 1u;
    spu2::reset();
    g_voice_transfer_status = {1, 1};
    g_voice_transfer_complete = {};
    g_voice_transfer_io_mode = {};
    g_transfer_callback = {};
    g_transfer_intr_handler = {};
    g_transfer_intr_argument = {};
    g_spu2_intr_handler = g_spu2_intr_argument = g_irq_callback = 0;
    g_spdif_settings = 0;
    for (std::uint32_t core = 0; core < 2; ++core) {
        const std::uint32_t base = core * 0x400u;
        spu2::write(kSpuBase + 0x7C0u, 0, 2);
        spu2::write(kSpuBase + 0x7C0u, 0x8000u, 2);
        spu2::write(kSpuBase + 0x19Au + base, 0, 2);
        spu2::write(kSpuBase + 0x19Au + base, 0x8000u, 2);
        spu2::write(kSpuBase + 0x1A4u + base, 0xFFFFu, 2);
        spu2::write(kSpuBase + 0x1A6u + base, 0x00FFu, 2);
        for (std::uint32_t voice = 0; voice < 24; ++voice) {
            const std::uint32_t vp = base + voice * 16u;
            spu2::write(kSpuBase + vp, 0, 2);
            spu2::write(kSpuBase + vp + 2u, 0, 2);
            spu2::write(kSpuBase + vp + 4u, 0x3FFFu, 2);
            spu2::write(kSpuBase + vp + 6u, 0, 2);
            spu2::write(kSpuBase + vp + 8u, 0, 2);
            const std::uint32_t va = base + 0x1C0u + voice * 12u;
            spu2::write(kSpuBase + va, 0, 2);
            spu2::write(kSpuBase + va + 2u, 0x2800u, 2);
        }
        const std::uint32_t mix = 0x188u + base;
        for (const std::uint32_t offset : {0x00u, 0x04u, 0x08u, 0x0Cu}) {
            spu2::write(kSpuBase + mix + offset, 0xFFFFu, 2);
            spu2::write(kSpuBase + mix + offset + 2u, 0x00FFu, 2);
        }
        spu2::write(kSpuBase + 0x198u + base, 0x0FF0u + (core ? 0x0Cu : 0), 2);
        spu2::write(kSpuBase + 0x19Au + base, static_cast<std::uint16_t>(0xC000u | (core ? 1u : 0u)), 2);
        spu2::write(kSpuBase + 0x340u + base, 0, 2);
        spu2::write(kSpuBase + 0x342u + base, 0, 2);
        if (!flag) {
            for (const std::uint32_t offset : {0x760u, 0x762u, 0x764u, 0x766u})
                spu2::write(kSpuBase + offset + core * 0x28u, 0, 2);
            spu2::write(kSpuBase + 0x33Cu + base, core ? 0xFu : 0xEu, 2);
        }
        g_effect_attr[core] = EffectAttr{core, 0, 0, 0, 0, 0};
        set_esa(core, get_eea(core) - (static_cast<std::uint32_t>(detail::kEffectSizes[0]) << 4) + 2u);
    }
    spu2::write(kSpuBase + 0x7C6u, 0x900u, 2);
    spu2::write(kSpuBase + 0x7C8u, 0x200u, 2);
    spu2::write(kSpuBase + 0x7CAu, 8u, 2);
    static constexpr std::array<std::uint16_t, 16> voice_data = {
        0x707, 0x707, 0x707, 0x707, 0x707, 0x707, 0x707, 0x707,
        0, 0, 0, 0, 0, 0, 0, 0,
    };
    std::copy(voice_data.begin(), voice_data.end(), spu2::ram() + (0x5000u >> 1));
    spu2::write(kSpuBase + 0x1A8u, 0, 2);
    spu2::write(kSpuBase + 0x1AAu, 0x2800u, 2);
    set_result(cpu, 0);
}

void export_set_param(Cpu &cpu) { set_param(arg(cpu, 0), static_cast<std::uint16_t>(arg(cpu, 1))); }
void export_get_param(Cpu &cpu) { std::uint16_t v = 0; set_u32_result(cpu, get_param(arg(cpu, 0), v) ? v : 0); }
void export_set_switch(Cpu &cpu) { set_switch(arg(cpu, 0), arg(cpu, 1)); }
void export_get_switch(Cpu &cpu) { std::uint32_t v = 0; set_u32_result(cpu, get_switch(arg(cpu, 0), v) ? v : 0); }
void export_set_addr(Cpu &cpu) { set_addr(arg(cpu, 0), arg(cpu, 1)); }
void export_get_addr(Cpu &cpu) { std::uint32_t v = 0; set_u32_result(cpu, get_addr(arg(cpu, 0), v) ? v : 0); }
void export_set_core_attr(Cpu &cpu) { set_core_attr(arg(cpu, 0), static_cast<std::uint16_t>(arg(cpu, 1))); }
void export_get_core_attr(Cpu &cpu) { std::uint16_t v = 0; set_u32_result(cpu, get_core_attr(arg(cpu, 0), v) ? v : 0); }
void export_note_to_pitch(Cpu &cpu) { set_u32_result(cpu, note_to_pitch(arg(cpu, 0), arg(cpu, 1), arg(cpu, 2), static_cast<std::int16_t>(arg(cpu, 3)))); }
void export_pitch_to_note(Cpu &cpu) { set_u32_result(cpu, pitch_to_note(arg(cpu, 0), arg(cpu, 1), arg(cpu, 2))); }
void export_proc_batch(Cpu &cpu) { set_result(cpu, process_batch(arg(cpu, 0), arg(cpu, 1), arg(cpu, 2), 0, false)); }
void export_proc_batch_ex(Cpu &cpu) { set_result(cpu, process_batch(arg(cpu, 0), arg(cpu, 1), arg(cpu, 2), arg(cpu, 3), true)); }
void export_voice_trans(Cpu &cpu) { set_result(cpu, voice_transfer(arg(cpu, 0), arg(cpu, 1), arg(cpu, 2), arg(cpu, 3), arg(cpu, 4))); }
void export_block_trans(Cpu &cpu) {
    const std::uint32_t mode = arg(cpu, 1);
    if ((mode & 3u) == kDmaStop) { set_u32_result(cpu, stop_transfer(arg(cpu, 0))); return; }
    set_result(cpu, block_transfer(arg(cpu, 0), mode, arg(cpu, 2), arg(cpu, 3), arg(cpu, 4)));
}
void export_voice_trans_status(Cpu &cpu) { set_u32_result(cpu, voice_transfer_status(arg(cpu, 0), arg(cpu, 1))); }
void export_block_trans_status(Cpu &cpu) {
    const std::uint32_t core = arg(cpu, 0) & 1u;
    const std::uint32_t address = g_block_transfer_active[core] ? g_block_transfer_address[core] : 0u;
    set_u32_result(cpu, (g_block_transfer_buffer[core] << 24) | (address & 0x00FFFFFFu));
}
void export_set_trans_callback(Cpu &cpu) {
    const std::uint32_t core = arg(cpu, 0) & 1u;
    const std::uint32_t previous = g_transfer_callback[core];
    g_transfer_callback[core] = arg(cpu, 1);
    set_u32_result(cpu, previous);
}
void export_set_irq_callback(Cpu &cpu) {
    const std::uint32_t previous = g_irq_callback;
    g_irq_callback = arg(cpu, 0);
    set_u32_result(cpu, previous);
}
void export_set_effect_attr(Cpu &cpu) {
    EffectAttr attr;
    if (!read_effect_attr(arg(cpu, 1), attr)) { set_result(cpu, -1); return; }
    set_result(cpu, apply_effect_attr(arg(cpu, 0), attr));
}
void export_get_effect_attr(Cpu &cpu) {
    const std::uint32_t address = arg(cpu, 1);
    const std::uint32_t core = arg(cpu, 0) & 1u;
    if (address && valid_iop_range(address, 20)) write_effect_attr(address, g_effect_attr[core]);
}
void export_clear_effect(Cpu &cpu) {
    const std::uint32_t mode = arg(cpu, 2);
    if (mode > 9u) { set_result(cpu, -1); return; }
    clear_effect_work_area(arg(cpu, 0), arg(cpu, 1), mode);
    set_result(cpu, 0);
}
void export_set_trans_intr_handler(Cpu &cpu) {
    const std::uint32_t core = arg(cpu, 0) & 1u;
    const std::uint32_t previous = g_transfer_intr_handler[core];
    g_transfer_intr_handler[core] = arg(cpu, 1);
    g_transfer_intr_argument[core] = arg(cpu, 2);
    set_u32_result(cpu, previous);
}
void export_set_spu2_intr_handler(Cpu &cpu) {
    const std::uint32_t previous = g_spu2_intr_handler;
    g_spu2_intr_handler = arg(cpu, 0);
    g_spu2_intr_argument = arg(cpu, 1);
    set_u32_result(cpu, previous);
}
void export_stop_trans(Cpu &cpu) { set_u32_result(cpu, stop_transfer(arg(cpu, 0))); }
void export_clean_effect(Cpu &cpu) {
    const std::uint32_t mode = arg(cpu, 2);
    if (mode > 9u) { set_result(cpu, -1); return; }
    clear_effect_work_area(arg(cpu, 0), arg(cpu, 1), mode);
    set_result(cpu, 0);
}
void export_set_effect_mode(Cpu &cpu) { export_set_effect_attr(cpu); }
void export_set_effect_mode_params(Cpu &cpu) { export_set_effect_attr(cpu); }

} // namespace

void reset() {
    g_effect_attr = {};
    g_effect_address = {};
    g_voice_transfer_io_mode = {};
    g_voice_transfer_status = {1, 1};
    g_voice_transfer_complete = {};
    g_block_transfer_buffer = {};
    g_block_transfer_address = {};
    g_block_transfer_size = {};
    g_block_transfer_active = {};
    g_transfer_intr_handler = {};
    g_transfer_intr_argument = {};
    g_transfer_callback = {};
    g_spu2_intr_handler = g_spu2_intr_argument = g_irq_callback = g_spdif_settings = 0;
}

kernel::NativeIopModule make_module() {
    kernel::NativeIopModule module;
    module.name = "LIBSD";
    module.aliases = {"Sound_Device_Library"};
    module.start = [](const std::string &) {
        reset();
        std::fprintf(stderr, "[iop-native] LIBSD v1.5 C++ SPU2 bridge active; guest LIBSD IRX is not loaded\n");
        return true;
    };
    kernel::NativeIopLibrary library;
    library.name = "libsd";
    library.version = 0x0105u;
    library.exports = {
        {4, export_init}, {5, export_set_param}, {6, export_get_param}, {7, export_set_switch},
        {8, export_get_switch}, {9, export_set_addr}, {10, export_get_addr}, {11, export_set_core_attr},
        {12, export_get_core_attr}, {13, export_note_to_pitch}, {14, export_pitch_to_note},
        {15, export_proc_batch}, {16, export_proc_batch_ex}, {17, export_voice_trans},
        {18, export_block_trans}, {19, export_voice_trans_status}, {20, export_block_trans_status},
        {21, export_set_trans_callback}, {22, export_set_irq_callback}, {23, export_set_effect_attr},
        {24, export_get_effect_attr}, {25, export_clear_effect}, {26, export_set_trans_intr_handler},
        {27, export_set_spu2_intr_handler}, {30, export_stop_trans}, {31, export_clean_effect},
        {32, export_set_effect_mode}, {33, export_set_effect_mode_params},
    };
    module.libraries.push_back(std::move(library));
    return module;
}

} // namespace ps2::iop::libsd_native
