#pragma once

#include "mka/stage_a_guest_memory.hpp"
#include "ps2/fpu.hpp"

#include <cstdint>

namespace mka::phase7 {

/// Native reimplementation of 0x001A7510 (RwFrameUpdateObjects) over Stage A GuestMemory.
/// Matches original body 0x001A7510..0x001A7577 (26 instructions), preserving exact
/// load/store ordering and dirty-list insertion semantics.
inline std::uint32_t frame_update_objects_001a7510(
    mka::stage_a::GuestMemory &mem,
    std::uint32_t frame_addr,
    std::uint32_t dirty_list_head_addr = 0x0069F34Cu) {
    const std::uint32_t root = mem.load<std::uint32_t>(frame_addr + 160u);
    const std::uint8_t root_flags = mem.load<std::uint8_t>(root + 3u);
    if ((root_flags & 3u) == 0u) {
        const std::uint32_t head = mem.load<std::uint32_t>(dirty_list_head_addr);
        mem.store<std::uint32_t>(root + 8u, head);
        mem.store<std::uint32_t>(root + 12u, dirty_list_head_addr);
        mem.store<std::uint32_t>(head + 4u, root + 8u);
        mem.store<std::uint32_t>(dirty_list_head_addr, root + 8u);
    }
    mem.store<std::uint8_t>(root + 3u, static_cast<std::uint8_t>(root_flags | 3u));
    const std::uint8_t self_flags = mem.load<std::uint8_t>(frame_addr + 3u);
    mem.store<std::uint8_t>(frame_addr + 3u, static_cast<std::uint8_t>(self_flags | 12u));
    return frame_addr;
}

/// Native reimplementation of 0x001A64A0 (RwCameraSetViewWindow) over Stage A GuestMemory.
/// Matches original body 0x001A64A0..0x001A650B (27 instructions), preserving exact
/// EE binary32 division / FCR31 semantics, store ordering, and nullable parent update.
inline std::uint32_t camera_set_view_window_001a64a0(
    mka::stage_a::GuestMemory &mem,
    std::uint32_t camera_addr,
    std::uint32_t window_addr,
    std::uint32_t &fcr31) {
    const std::uint32_t wx = mem.load<std::uint32_t>(window_addr);
    const std::uint32_t wy = mem.load<std::uint32_t>(window_addr + 4u);
    mem.store<std::uint32_t>(camera_addr + 104u, wx);
    mem.store<std::uint32_t>(camera_addr + 108u, wy);

    const std::uint32_t inv_x = ps2::fp::ee::div(
        fcr31, 0x3F800000u, mem.load<std::uint32_t>(camera_addr + 104u));
    mem.store<std::uint32_t>(camera_addr + 112u, inv_x);

    const std::uint32_t inv_y = ps2::fp::ee::div(
        fcr31, 0x3F800000u, mem.load<std::uint32_t>(camera_addr + 108u));
    mem.store<std::uint32_t>(camera_addr + 116u, inv_y);

    const std::uint32_t parent = mem.load<std::uint32_t>(camera_addr + 4u);
    if (parent != 0u) {
        frame_update_objects_001a7510(mem, parent);
    }
    return camera_addr;
}

} // namespace mka::phase7
