#pragma once

#include <cstdint>

namespace mka::phase6_track_a {

// Disposable Stage B capability only. Original 0016AE10 reads the byte at
// this+20 and returns its bit 5; complete owner layout/lifetime remain OPEN.
// This minimal native value carries precisely that independently proved input.
struct NativeObj16AE10 {
    std::uint8_t flags = 0;
};

inline std::uint64_t fn_0016ae10_stage_b(const NativeObj16AE10 &obj) noexcept {
    return static_cast<std::uint64_t>((obj.flags >> 5) & 1u);
}

// Original 002E1710 is JR RA; NOP. It has no semantic inputs, return value or
// effects. Scheduling/return-address validation belongs to the test adapter.
inline void fn_002e1710_stage_b() noexcept {}

} // namespace mka::phase6_track_a
