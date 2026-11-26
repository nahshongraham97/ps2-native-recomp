#pragma once

#include "mka/stage_a_guest_memory.hpp"

#include <cstdint>

namespace mka::phase7 {

struct NativePadSlot {
    std::uint8_t flags = 0;         // guest (0x00660080 + slot*32) + 460
    std::uint32_t active_ptr = 0;   // guest + 468
    std::uint32_t entries_ptr = 0;  // guest + 464
    std::uint32_t button_mask = 0;  // guest + 480
};

/// Stage A native reimplementation of 0x001637D0 (PAD Scanner, unit_0018:328u).
/// Scans the 3 PAD slots at 0x00660080 (stride 32). When a slot is active
/// ((flags & 2) != 0 && (flags & 1) == 0 && active_ptr != 0), scans up to 16
/// button entries (stride 12) and invokes the event callback for matching masks.
template <typename OnMatch>
inline std::uint32_t pad_scan_001637d0(
    mka::stage_a::GuestMemory &mem,
    OnMatch &&on_match,
    std::uint32_t slot_base = 0x00660080u) {
    std::uint32_t matched = 0;
    for (std::uint32_t slot = 0; slot < 3u; ++slot) {
        const std::uint32_t base = slot_base + slot * 32u;
        const std::uint8_t flags = mem.load<std::uint8_t>(base + 460u);
        if ((flags & 2u) == 0u || (flags & 1u) != 0u) {
            continue;
        }
        const std::uint32_t active_ptr = mem.load<std::uint32_t>(base + 468u);
        if (active_ptr == 0u) {
            continue;
        }
        const std::uint32_t entries_ptr = mem.load<std::uint32_t>(base + 464u);
        const std::uint32_t mask = mem.load<std::uint32_t>(base + 480u);
        for (std::uint32_t i = 0; i < 16u; ++i) {
            const std::uint32_t entry_addr = entries_ptr + i * 12u;
            const std::uint32_t entry_mask = mem.load<std::uint32_t>(entry_addr);
            if ((mask & entry_mask) != 0u) {
                on_match(slot, i, entry_addr, active_ptr);
                ++matched;
            }
        }
    }
    return matched;
}

} // namespace mka::phase7
