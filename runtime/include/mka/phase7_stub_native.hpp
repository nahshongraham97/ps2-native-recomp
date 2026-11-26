#pragma once

#include "mka/stage_a_guest_memory.hpp"

#include <cstdint>

namespace mka::phase7 {

/// Stage A native reimplementation of 0x002E60E0 (unit_0079:187u, 0x002E6180 particle/stub path entry).
/// Verified on captured null-emitter states (a0 == 0 -> no guest stores, v0 == 3).
inline std::uint32_t particle_stub_entry_002e60e0(
    mka::stage_a::GuestMemory & /*mem*/,
    std::uint32_t a0) noexcept {
    if (a0 == 0u) {
        return 3u;
    }
    return 0u;
}

} // namespace mka::phase7
