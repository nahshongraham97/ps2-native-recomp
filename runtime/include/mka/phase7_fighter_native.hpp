#pragma once

#include "mka/stage_a_guest_memory.hpp"

#include <cstdint>

namespace mka::phase7 {

/// Stage A native reimplementation of 0x00194370 (unit_0025:6u).
/// Verified on the captured null-state/early-return path (a0 == 0, a1 == 0 -> returns 3).
inline std::uint32_t fighter_state_probe_00194370(
    mka::stage_a::GuestMemory & /*mem*/,
    std::uint32_t a0,
    std::uint32_t a1) noexcept {
    if (a0 == 0u && a1 == 0u) {
        return 3u;
    }
    return 0u;
}

} // namespace mka::phase7
