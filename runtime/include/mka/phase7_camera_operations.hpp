#pragma once

#include "mka/phase7_camera_native.hpp"
#include "mka/retail_renderer_rasters.hpp"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace mka::phase7::camera_operations {

// Disposable typed composition of original 6410/6540/65A0/68F0. This is not
// production adoption. Unknown actual plugin/device/numeric callbacks are
// required bindings; an empty binding always fails by name at its source call.
struct BoundaryRefusal : std::runtime_error {
    explicit BoundaryRefusal(const char *operation) : std::runtime_error(operation) {}
};

template<class Function, class... Arguments>
decltype(auto) invoke_required(const Function &function, const char *name, Arguments &&...arguments) {
    if (!function) throw BoundaryRefusal(name);
    return function(std::forward<Arguments>(arguments)...);
}

struct Camera;
using CameraCallback = std::function<Camera *(Camera &)>;
struct FrameAttachmentLink {
    FrameAttachmentLink *next{};
    FrameAttachmentLink *previous{};
};

struct Camera {
    // Reuse proved frame/view-window owners. No fixed console object extent.
    NativeRwCamera common;
    FrameAttachmentLink attachment;
    std::uint8_t object_type{}, subtype{}, flags{}, private_flags{};
    std::uint32_t projection{};
    CameraCallback synchronize, begin_update, end_update;
    NativeRwV2d view_offset;
    std::uint32_t near_bits{}, far_bits{}, fog_bits{}, depth_scale_bits{}, depth_shift_bits{};
    mka::retail::NativeRetailRasterHandle frame_buffer, depth_buffer;
    // Original create stores zero at +44 after depth calculation. Its wider
    // view/matrix meaning is not established by this bounded implementation.
    std::uint32_t unclassified_creation_word{};
};

struct ErrorState {
    std::uint32_t plugin_id;
    std::uint32_t code;
};

// 19F510 returns sign-extended a0; its varargs saves affect only its own stack.
// 19F460 publishes only while the prior state is {0,0x80000000}. A high-bit
// error records plugin zero. No formatted-message/logging effect is invented.
inline void publish_error(ErrorState &state, std::uint32_t plugin, std::uint32_t code) noexcept {
    if (state.plugin_id == 0 && state.code == 0x80000000u) {
        state.plugin_id = (code & 0x80000000u) ? 0u : plugin;
        state.code = code;
    }
}

struct Services {
    // Allocation is typed and host-sized. Source hint 0x30005 is retained;
    // source registry allocation extent is not a native storage limit.
    std::function<Camera *(std::uint32_t hint)> allocate;
    std::function<void(Camera &)> release;
    std::function<void(Camera &)> derive_depth;
    std::function<bool(Camera &)> initialize_plugins;
    std::function<void(Camera &)> destroy_plugins;
    CameraCallback synchronize, begin_update, end_update;
};

inline CameraCallback bind_callback(CameraCallback callback, const char *name) {
    return [callback = std::move(callback), name](Camera &camera) {
        return invoke_required(callback, name, camera);
    };
}

inline Camera *set_projection(Camera &camera, std::uint64_t projection,
                              NativeRwDirtyList &dirty_list, ErrorState &errors,
                              const Services &services) {
    // BEQ compares the complete 64-bit incoming value, before the SW truncates.
    if (projection != 1 && projection != 2) {
        publish_error(errors, 1, 0x80000003u);
        return nullptr;
    }
    camera.projection = static_cast<std::uint32_t>(projection);
    if (camera.common.parent) {
        if (!camera.common.parent->root) throw BoundaryRefusal("001A7510 native frame root required");
        frame_update_objects_stage_b(*camera.common.parent, dirty_list);
    }
    invoke_required(services.derive_depth, "001A4F00 native camera depth binding required", camera);
    return &camera;
}

inline std::uint32_t destroy(Camera &camera, const Services &services) {
    invoke_required(services.destroy_plugins, "001A3520 native camera plugin teardown required", camera);
    // 1AA120 unlinks only when parent is nonnull. It preserves the parent and
    // this node's links; do not fabricate clearing stores or replay cleanup.
    if (camera.common.parent) {
        if (!camera.attachment.previous || !camera.attachment.next)
            throw BoundaryRefusal("001AA120 attached camera links required");
        camera.attachment.previous->next = camera.attachment.next;
        camera.attachment.next->previous = camera.attachment.previous;
    }
    invoke_required(services.release, "0069F3D8 native camera release binding required", camera);
    return 1; // Source ignores the allocator callback return and never rereads camera.
}

inline Camera *create(const Services &services) {
    Camera *camera = invoke_required(services.allocate, "0069F3D4 native camera allocation binding required", 0x30005u);
    if (!camera) return nullptr;
    camera->object_type = 4;
    camera->subtype = 0;
    camera->flags = 0;
    camera->private_flags = 0;
    camera->common.parent = nullptr;
    camera->synchronize = bind_callback(services.synchronize, "001A6040 native camera synchronization required");
    camera->begin_update = bind_callback(services.begin_update, "001A6100 native camera begin-update required");
    camera->end_update = bind_callback(services.end_update, "001A60B0 native camera end-update required");
    camera->common.view_window.y_bits = 0x3f800000u;
    camera->common.view_window.x_bits = 0x3f800000u;
    camera->common.inverse_window.y_bits = 0x3f800000u;
    camera->common.inverse_window.x_bits = 0x3f800000u;
    camera->view_offset.y_bits = 0;
    camera->view_offset.x_bits = 0;
    camera->near_bits = 0x3d4ccccdu;
    camera->far_bits = 0x41200000u;
    camera->fog_bits = 0x40a00000u;
    camera->frame_buffer = {};
    camera->depth_buffer = {};
    camera->projection = 1; // Original JAL delay slot, before depth callback.
    invoke_required(services.derive_depth, "001A4F00 native camera depth binding required", *camera);
    camera->unclassified_creation_word = 0;
    // Important: 65A0 ignores 1A3480's return, including constructor failure.
    (void)invoke_required(services.initialize_plugins, "001A3480 native camera plugin initialization required", *camera);
    return camera;
}

// The original request accepts generic payload pointers. Typed native owners
// supply the actual request-specific payload; the outer routine only writes
// the scalar word for requests 13/15. Copy-to-destination remains a required
// callback because the actual 69F388 binding has not been proved.
struct DeviceOutput {
    std::uint32_t scalar{};
    std::string label;
};
struct DeviceInput { virtual ~DeviceInput() = default; };
struct Device {
    std::function<std::uint64_t(std::uint64_t request, DeviceOutput *, const DeviceInput *, std::uint64_t extra)> request;
    std::function<void(DeviceOutput *, std::string_view)> copy_subsystem_label;
};

inline std::uint64_t device_request(const Device &device, std::uint64_t request,
                                    DeviceOutput *output, const DeviceInput *input,
                                    std::uint64_t extra, ErrorState &errors) {
    auto result = invoke_required(device.request, "001A68F0 native device request delegate required",
                                  request, output, input, extra);
    if (result) return result;
    // ADDIU performs a 32-bit wrapping subtraction then sign extends; SLTIU
    // against six admits exactly low-word requests 13..18, irrespective of
    // the request's high word. Preserve the original full value for delegate.
    const auto index = static_cast<std::uint32_t>(request) - 13u;
    if (index < 6u) {
        switch (index) {
        case 0:
            if (!output) throw BoundaryRefusal("001A696C scalar device output required");
            output->scalar = 1; result = 1; break;
        case 1:
            result = extra == 0;
            if (result) invoke_required(device.copy_subsystem_label, "0069F388 native subsystem-label callback required",
                                        output, std::string_view("Only rendering sub system"));
            break;
        case 2:
            if (!output) throw BoundaryRefusal("001A69A8 scalar device output required");
            output->scalar = 0; result = 1; break;
        case 3: result = extra == 0; break;
        default: result = 1; break;
        }
    }
    if (!result) publish_error(errors, 1, 24);
    return result;
}

} // namespace mka::phase7::camera_operations
