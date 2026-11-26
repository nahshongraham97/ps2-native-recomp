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

// ============================================================================
// Stage B: Typed Native Structures (Zero GuestMemory, Zero g_ram, Zero c.r)
// ============================================================================

struct NativeRwFrame {
    std::uint8_t private_flags = 0; // guest +3
    NativeRwFrame *root = nullptr;  // guest +160
    NativeRwFrame *dirty_next = nullptr; // guest +8
    NativeRwFrame **dirty_prev = nullptr; // guest +12
};

struct NativeRwDirtyList {
    NativeRwFrame *head = nullptr; // guest 0x0069F34C
    NativeRwFrame *sentinel = nullptr;
};

struct NativeRwV2d {
    std::uint32_t x_bits = 0;
    std::uint32_t y_bits = 0;
};

struct NativeRwCamera {
    NativeRwFrame *parent = nullptr; // guest +4
    NativeRwV2d view_window{};       // guest +104, +108
    NativeRwV2d inverse_window{};    // guest +112, +116
};

/// Stage B native reimplementation of 0x001A70F0 (RwFrameDirty):
/// returns (frame.root->private_flags & 3).
inline std::uint8_t frame_dirty_stage_b(const NativeRwFrame &frame) noexcept {
    return static_cast<std::uint8_t>(frame.root->private_flags & 3u);
}

/// Stage B native reimplementation of 0x001A7510 (RwFrameUpdateObjects).
inline NativeRwFrame *frame_update_objects_stage_b(
    NativeRwFrame &frame,
    NativeRwDirtyList &dirty_list) noexcept {
    NativeRwFrame *const root = frame.root;
    const std::uint8_t root_flags = root->private_flags;
    if ((root_flags & 3u) == 0u) {
        NativeRwFrame *const old_head = dirty_list.head;
        root->dirty_next = old_head;
        root->dirty_prev = &dirty_list.head;
        if (old_head != nullptr) {
            old_head->dirty_prev = &root->dirty_next;
        }
        dirty_list.head = root;
    }
    root->private_flags = static_cast<std::uint8_t>(root_flags | 3u);
    frame.private_flags = static_cast<std::uint8_t>(frame.private_flags | 12u);
    return &frame;
}

/// Stage B native reimplementation of 0x001A64A0 (RwCameraSetViewWindow).
inline NativeRwCamera *camera_set_view_window_stage_b(
    NativeRwCamera &camera,
    const NativeRwV2d &window,
    NativeRwDirtyList &dirty_list,
    std::uint32_t &fcr31) noexcept {
    camera.view_window = window;
    camera.inverse_window.x_bits = ps2::fp::ee::div(fcr31, 0x3F800000u, camera.view_window.x_bits);
    camera.inverse_window.y_bits = ps2::fp::ee::div(fcr31, 0x3F800000u, camera.view_window.y_bits);
    if (camera.parent != nullptr) {
        frame_update_objects_stage_b(*camera.parent, dirty_list);
    }
    return &camera;
}

} // namespace mka::phase7
