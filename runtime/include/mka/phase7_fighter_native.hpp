#pragma once

#include "mka/stage_a_guest_memory.hpp"

#include <cstdint>
#include <cstdlib>

namespace mka::phase7 {

/// Null-path probe ONLY for 0x00194370 (unit_0025:6u).
/// Observed in 30 captures across 6 states where a0 == 0 && a1 == 0 (returns 3).
/// Not a full implementation: aborts fail-closed on any non-null path.
inline std::uint32_t fighter_state_null_probe_00194370(
    mka::stage_a::GuestMemory & /*mem*/,
    std::uint32_t a0,
    std::uint32_t a1) noexcept {
    if (a0 == 0u && a1 == 0u) {
        return 3u;
    }
    std::abort();
}

} // namespace mka::phase7
