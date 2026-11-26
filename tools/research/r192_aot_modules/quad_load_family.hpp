#pragma once
// Primitive family emitted by original 002EF5F0. No runtime word decoder.
// This is an R192 compatibility kernel, not native-port implementation credit.
#include "ps2/generated.hpp"
#include <cstdint>
#include <stdexcept>
#include <span>

namespace mka::emitted {
enum class QuadDestination : std::uint32_t { vector = 0, gpr = 2, gpr_alias3 = 3, gpr_alias4 = 4 };
struct QuadLoadDescriptor {
    QuadDestination destination_kind;
    std::uint32_t destination_register;
    std::uint32_t base_register;
    std::uint16_t raw_offset;
};

inline std::uint32_t original_quad_load_word(const QuadLoadDescriptor& d) {
    const auto type = static_cast<std::uint32_t>(d.destination_kind);
    if (type != 0 && type != 2 && type != 3 && type != 4)
        throw std::runtime_error("quad_load: unsupported original destination type");
    if (d.destination_register >= 32 || d.base_register >= 32)
        throw std::runtime_error("quad_load: unproved register domain");
    return (type == 0 ? 0xD8000000u : 0x78000000u) |
           (d.base_register << 21) | (d.destination_register << 16) | d.raw_offset;
}

inline bool append_quad_load(std::span<std::uint32_t> output, std::uint32_t& cursor,
                             const QuadLoadDescriptor& d) {
    const auto word = original_quad_load_word(d);
    if (cursor >= output.size()) return false; // Original current-count/capacity guard.
    output[cursor++] = word;
    return true;
}

// Build-time specialization: all instruction fields are template constants.
// Runtime use requires a separately authenticated construction/family binding.
// It performs ONE original emitted load only; no PC/budget/return is invented.
template<QuadDestination Kind, unsigned Destination, unsigned Base, std::uint16_t Offset>
inline void quad_load(ps2::Cpu& c) {
    static_assert(Destination < 32 && Base < 32);
    static_assert(Kind == QuadDestination::vector || Kind == QuadDestination::gpr ||
                  Kind == QuadDestination::gpr_alias3 || Kind == QuadDestination::gpr_alias4);
    const std::uint32_t base = Base ? c.r[Base].w[0] : 0u;
    constexpr std::int32_t displacement = Offset < 0x8000u ? Offset : static_cast<std::int32_t>(Offset) - 0x10000;
    const std::uint32_t address = (base + static_cast<std::uint32_t>(displacement)) & ~15u;
    // Current proof covers RAM, including original alignment-down semantics.
    // Hardware/scratchpad aliases require their own proved boundary.
    if (address > ps2::kRamSize - 16)
        throw std::runtime_error("quad_load: memory domain not admitted");
    ps2::Reg128 value;
    ps2::ld128(address, value);
    if constexpr (Destination != 0) {
        if constexpr (Kind == QuadDestination::vector) c.vu0.vf[Destination] = value;
        else c.r[Destination] = value;
    }
}
}
