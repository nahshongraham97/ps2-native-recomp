#pragma once

#include "native/iop_native.h"

#include <cstdint>
#include <limits>
#include <optional>

namespace ps2x::iop::detail {

inline constexpr std::uint32_t kLwlMask[4] = {
    0x00FFFFFFu, 0x0000FFFFu, 0x000000FFu, 0x00000000u};
inline constexpr std::uint32_t kLwlShift[4] = {24u, 16u, 8u, 0u};
inline constexpr std::uint32_t kLwrMask[4] = {
    0x00000000u, 0xFF000000u, 0xFFFF0000u, 0xFFFFFF00u};
inline constexpr std::uint32_t kLwrShift[4] = {0u, 8u, 16u, 24u};

inline void iopRaiseException(IopCpuState &cpu, std::uint32_t code,
                              std::uint32_t faultPc, bool delaySlot,
                              std::optional<std::uint32_t> badAddress = std::nullopt) {
    std::uint32_t cause = cpu.cop0[13] & ~0x7Cu;
    cause |= (code & 0x1Fu) << 2u;
    if (delaySlot) {
        cause |= 0x80000000u;
        cpu.cop0[14] = faultPc - 4u;
    } else {
        cause &= ~0x80000000u;
        cpu.cop0[14] = faultPc;
    }
    cpu.cop0[13] = cause;
    if (badAddress) cpu.cop0[8] = *badAddress;
    const std::uint32_t status = cpu.cop0[12];
    cpu.cop0[12] = (status & ~0x3Fu) | ((status << 2u) & 0x3Fu);
    cpu.pc = (status & (1u << 22u)) ? 0xBFC00180u : 0x80000080u;
    cpu.branchPending = false;
    cpu.pendingLoad = false;
    cpu.exception = true;
}

inline void iopDiv(IopCpuState &cpu, std::uint32_t a, std::uint32_t b) {
    const std::int32_t lhs = static_cast<std::int32_t>(a);
    const std::int32_t rhs = static_cast<std::int32_t>(b);
    if (rhs == 0) {
        cpu.lo = lhs >= 0 ? 0xFFFFFFFFu : 1u;
        cpu.hi = static_cast<std::uint32_t>(lhs);
    } else if (lhs == std::numeric_limits<std::int32_t>::min() && rhs == -1) {
        cpu.lo = static_cast<std::uint32_t>(lhs);
        cpu.hi = 0u;
    } else {
        cpu.lo = static_cast<std::uint32_t>(lhs / rhs);
        cpu.hi = static_cast<std::uint32_t>(lhs % rhs);
    }
}

inline void iopDivu(IopCpuState &cpu, std::uint32_t a, std::uint32_t b) {
    if (b == 0u) {
        cpu.lo = 0xFFFFFFFFu;
        cpu.hi = a;
    } else {
        cpu.lo = a / b;
        cpu.hi = a % b;
    }
}

inline void iopSwl(IopMemory &memory, std::uint32_t address, std::uint32_t value) {
    const std::uint32_t aligned = address & ~3u;
    const std::uint32_t old = memory.load32(aligned);
    std::uint32_t result = old;
    switch (address & 3u) {
    case 0u: result = (old & 0xFFFFFF00u) | (value >> 24u); break;
    case 1u: result = (old & 0xFFFF0000u) | (value >> 16u); break;
    case 2u: result = (old & 0xFF000000u) | (value >> 8u); break;
    case 3u: result = value; break;
    }
    memory.store32(aligned, result);
}

inline void iopSwr(IopMemory &memory, std::uint32_t address, std::uint32_t value) {
    const std::uint32_t aligned = address & ~3u;
    const std::uint32_t old = memory.load32(aligned);
    std::uint32_t result = old;
    switch (address & 3u) {
    case 0u: result = value; break;
    case 1u: result = (old & 0x000000FFu) | (value << 8u); break;
    case 2u: result = (old & 0x0000FFFFu) | (value << 16u); break;
    case 3u: result = (old & 0x00FFFFFFu) | (value << 24u); break;
    }
    memory.store32(aligned, result);
}

} // namespace ps2x::iop::detail

