#include "adx_native.hpp"

#include "compliance_census.hpp"
#include "kernel.hpp"
#include "sif.hpp"
#include "ps2/memory.hpp"
#include "ps2/system.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ps2::iop::adx_native {

namespace {

constexpr std::uint32_t kAdxSid = 0x90000200u;

struct AdxInstance {
    bool active = false;
    std::uint32_t handle = 0;
    std::uint32_t frame_count = 0;
};

struct AdxBuffer {
    bool active = false;
    std::uint32_t handle = 0;
};

struct AdxChunk {
    bool active = false;
    std::uint32_t handle = 0;
};

std::array<AdxInstance, 4> s_instances{};
std::array<AdxBuffer, 8> s_buffers{};
std::array<AdxChunk, 16> s_chunks{};

bool valid_ee_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = address & 0x1FFFFFFFu;
    return physical < ps2::kRamSize && size <= static_cast<std::uint64_t>(ps2::kRamSize - physical);
}

bool adx_rpc(std::uint32_t method, std::uint32_t args,
             std::uint32_t args_size, std::uint32_t ret,
             std::uint32_t ret_size) {
    std::uint32_t reply_value = 0;
    bool has_reply = false;

    // The CRI ADXI driver registers SID 0x90000200 with 12 RPC functions.
    // Captured execution trace from retail MKA intro movie playback:
    // fn 2, 3, 1024, 1025, 1027, 1032, 1033, 1034, 1036, 1037, 1058, 1059.
    switch (method) {
    case 2: {
        // Open / Initialize stream player instance
        std::uint32_t instance_id = 0;
        if (args_size >= 4 && valid_ee_range(args, 4)) {
            const std::uint8_t *p = ps2::fast<std::uint8_t>(args & 0x1FFFFFFFu);
            if (p) std::memcpy(&instance_id, p, sizeof(instance_id));
        }
        std::uint32_t slot = instance_id < s_instances.size() ? instance_id : 0;
        s_instances[slot].active = true;
        s_instances[slot].handle = 0x000CAD40u + slot * 0x44u;
        s_instances[slot].frame_count = 0;
        reply_value = s_instances[slot].handle;
        has_reply = true;
        std::fprintf(stderr, "[adx-native] fn 2 (Init) instance=%u -> handle 0x%08X\n", slot, reply_value);
        break;
    }

    case 3: {
        // Close / Stop stream player instance
        std::uint32_t handle = 0;
        if (args_size >= 4 && valid_ee_range(args, 4)) {
            const std::uint8_t *p = ps2::fast<std::uint8_t>(args & 0x1FFFFFFFu);
            if (p) std::memcpy(&handle, p, sizeof(handle));
        }
        for (auto &inst : s_instances) {
            if (inst.handle == handle) {
                inst.active = false;
                break;
            }
        }
        std::fprintf(stderr, "[adx-native] fn 3 (Close) handle=0x%08X\n", handle);
        break;
    }

    case 1024: {
        // Decode / Process chunk: updates stream presentation
        std::uint32_t slot = 0;
        for (std::size_t i = 0; i < s_chunks.size(); ++i) {
            if (!s_chunks[i].active) {
                slot = static_cast<std::uint32_t>(i);
                break;
            }
        }
        s_chunks[slot].active = true;
        s_chunks[slot].handle = 0x000D1840u + slot * 0x14u;
        reply_value = s_chunks[slot].handle;
        has_reply = true;
        // Mark that an ADX movie chunk was processed
        for (auto &inst : s_instances) {
            if (inst.active) ++inst.frame_count;
        }
        break;
    }

    case 1025: {
        // Release chunk handle
        std::uint32_t handle = 0;
        if (args_size >= 4 && valid_ee_range(args, 4)) {
            const std::uint8_t *p = ps2::fast<std::uint8_t>(args & 0x1FFFFFFFu);
            if (p) std::memcpy(&handle, p, sizeof(handle));
        }
        for (auto &c : s_chunks) {
            if (c.handle == handle) {
                c.active = false;
                break;
            }
        }
        break;
    }

    case 1027: {
        // Reset reference / previous picture buffer
        std::fprintf(stderr, "[adx-native] fn 1027 (ResetRef)\n");
        break;
    }

    case 1032: {
        // Poll playback status event
        reply_value = 0x000CB220u;
        has_reply = true;
        break;
    }

    case 1033: {
        // Acknowledge playback status event
        break;
    }

    case 1034: {
        // Configure buffer layout / stream format
        std::fprintf(stderr, "[adx-native] fn 1034 (ConfigFormat)\n");
        break;
    }

    case 1036: {
        // Start playback / unpause audio
        std::fprintf(stderr, "[adx-native] fn 1036 (StartPlayback)\n");
        break;
    }

    case 1037: {
        // Stop playback / pause audio
        std::fprintf(stderr, "[adx-native] fn 1037 (StopPlayback)\n");
        break;
    }

    case 1058: {
        // Allocate packet buffer
        std::uint32_t slot = 0;
        for (std::size_t i = 0; i < s_buffers.size(); ++i) {
            if (!s_buffers[i].active) {
                slot = static_cast<std::uint32_t>(i);
                break;
            }
        }
        s_buffers[slot].active = true;
        s_buffers[slot].handle = 0x000D0D00u + slot * 0x30u;
        reply_value = s_buffers[slot].handle;
        has_reply = true;
        break;
    }

    case 1059: {
        // Free packet buffer
        std::uint32_t handle = 0;
        if (args_size >= 4 && valid_ee_range(args, 4)) {
            const std::uint8_t *p = ps2::fast<std::uint8_t>(args & 0x1FFFFFFFu);
            if (p) std::memcpy(&handle, p, sizeof(handle));
        }
        for (auto &b : s_buffers) {
            if (b.handle == handle) {
                b.active = false;
                break;
            }
        }
        break;
    }

    default:
        std::fprintf(stderr, "[adx-native] unhandled method %u (args_size=%u ret_size=%u)\n", method, args_size, ret_size);
        break;
    }

    if (has_reply && ret && ret_size >= sizeof(std::uint32_t) && valid_ee_range(ret, sizeof(std::uint32_t))) {
        std::uint8_t *reply_bytes = ps2::fast<std::uint8_t>(ret & 0x1FFFFFFFu);
        if (reply_bytes) {
            std::memcpy(reply_bytes, &reply_value, sizeof(reply_value));
        }
    }

    return true;
}

} // namespace

void reset() {
    for (auto &inst : s_instances) inst = AdxInstance{};
    for (auto &b : s_buffers) b = AdxBuffer{};
    for (auto &c : s_chunks) c = AdxChunk{};
}

void install() {
    reset();
    sif::register_hle_override(kAdxSid, adx_rpc);
    std::fprintf(stderr, "[adx-native] host SIF service registered for SID 0x%08X (12 RPC functions)\n", kAdxSid);
}

kernel::NativeIopModule make_module() {
    kernel::NativeIopModule module;
    module.name = "CRI_ADXI";
    module.aliases = {"CRI_ADX_Driver"};
    module.start = [](const std::string &) {
        install();
        std::fprintf(stderr, "[iop-native] CRI_ADXI host replacement active; guest IRX is not loaded\n");
        return true;
    };
    return module;
}

} // namespace ps2::iop::adx_native
