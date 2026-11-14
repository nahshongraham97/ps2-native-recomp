#pragma once

// Compatibility ABI for ahead-of-time IRX bodies emitted by the GoW reference
// compiler. These state and memory adapters belong to this runtime; generated
// bodies do not link the donor IOP emulator.

#include "iop/iop.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ps2x::iop::detail {

struct IopCpuState {
    std::array<std::uint32_t, 32> gpr{};
    std::uint32_t hi = 0;
    std::uint32_t lo = 0;
    std::uint32_t pc = 0;
    std::array<std::uint32_t, 32> cop0{};
    std::uint32_t pendingLoadReg = 0;
    std::uint32_t pendingLoadValue = 0;
    bool pendingLoad = false;
    bool branchPending = false;
    std::uint32_t branchTarget = 0;
    bool stopped = false;
    bool yielded = false;
    bool exception = false;
};

class IopMemory {
public:
    [[nodiscard]] std::uint8_t load8(std::uint32_t address) const {
        return ps2::iop::read8(address);
    }
    [[nodiscard]] std::uint16_t load16(std::uint32_t address) const {
        return ps2::iop::read16(address);
    }
    [[nodiscard]] std::uint32_t load32(std::uint32_t address) const {
        return ps2::iop::read32(address);
    }
    void store8(std::uint32_t address, std::uint8_t value) {
        ps2::iop::write8(address, value);
    }
    void store16(std::uint32_t address, std::uint16_t value) {
        ps2::iop::write16(address, value);
    }
    void store32(std::uint32_t address, std::uint32_t value) {
        ps2::iop::write32(address, value);
    }
};

struct IopNativeModule {
    const char *name;
    std::uint32_t base;
    std::uint32_t textSize;
    std::uint64_t hash;
    std::uint32_t (*run)(IopCpuState &, IopMemory &, std::uint32_t budget);
};

const IopNativeModule *iopNativeModules(std::size_t &count);

inline std::uint64_t iopNativeHash(const std::uint8_t *data, std::size_t size) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i)
        hash = (hash ^ data[i]) * 1099511628211ull;
    return hash;
}

} // namespace ps2x::iop::detail

