#pragma once

#include "mka/stage_a_guest_memory.hpp"

#include <cstdint>
#include <cstdlib>

namespace mka::phase7 {

/// Null-path probe ONLY for 0x002E60E0 (unit_0079:187u).
/// Observed on captured null-emitter states (a0 == 0 -> no guest stores, v0 == 3).
/// Not a full implementation: aborts fail-closed on active non-null emitters.
inline std::uint32_t particle_stub_null_probe_002e60e0(
    mka::stage_a::GuestMemory & /*mem*/,
    std::uint32_t a0) noexcept {
    if (a0 == 0u) {
        return 3u;
    }
    std::abort();
}

} // namespace mka::phase7
