#include "padman.hpp"

#include "iop.hpp"
#include "kernel.hpp"
#include "sif.hpp"
#include "sio2.hpp"
#include "ps2/memory.hpp"
#include "ps2/state.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ps2::iop::padman {

namespace {

constexpr std::uint32_t kPadRpcSid0 = 0x80000100u;
constexpr std::uint32_t kPadRpcSid1 = 0x80000101u;
constexpr std::uint32_t kPadRpcFunction = 1u;
constexpr std::uint32_t kPadAreaBytes = 256u;
constexpr std::uint32_t kPadRecordBytes = 128u;
constexpr std::uint8_t kPadModePressure = 0x79u;
constexpr std::uint8_t kPadStateDisconnected = 0u;
constexpr std::uint8_t kPadStateExecCommand = 5u;
constexpr std::uint8_t kPadStateStable = 6u;
constexpr std::uint8_t kPadRequestComplete = 0u;
constexpr std::uint8_t kPadRequestBusy = 2u;

// This is the 128-byte ee_paddata_t layout used by PS2SDK's newer libpad.
struct EePadData {
    std::uint8_t data[32];
    std::uint32_t act_dir_data[2];
    std::uint32_t act_align_data[2];
    std::uint32_t act_data[4];
    std::uint32_t comb_data[4];
    std::uint32_t mode_table[2];
    std::uint32_t frame;
    std::uint32_t find_pad_retries;
    std::uint32_t length;
    std::uint8_t mode_config;
    std::uint8_t mode_cur_id;
    std::uint8_t model;
    std::uint8_t button_data_ready;
    std::uint8_t num_modes;
    std::uint8_t mode_cur_offset;
    std::uint8_t num_actuators;
    std::uint8_t num_actuator_combinations;
    std::uint8_t val_c6;
    std::uint8_t mode;
    std::uint8_t lock;
    std::uint8_t act_dir_size;
    std::uint8_t state;
    std::uint8_t request_state;
    std::uint8_t current_task;
    std::uint8_t run_task;
    std::uint8_t stat70_bit;
    std::uint8_t padding[11];
};
static_assert(sizeof(EePadData) == kPadRecordBytes);
static_assert(offsetof(EePadData, frame) == 88u);
static_assert(offsetof(EePadData, state) == 112u);

struct OpenPad {
    std::uint32_t ee_area{};
    std::uint32_t frame{};
    bool open{};
};

bool g_active = false;
bool g_initialized = false;
std::array<std::array<OpenPad, 4>, 2> g_open_pads{};

std::uint32_t ee_physical(std::uint32_t address) { return address & 0x1FFFFFFFu; }

bool valid_ee_range(std::uint32_t address, std::uint32_t size) {
    const std::uint32_t physical = ee_physical(address);
    return physical < ps2::kRamSize && size <= ps2::kRamSize - physical;
}

[[noreturn]] void strict_stop(const char *reason, std::uint32_t method, std::uint32_t command) {
    std::fprintf(stderr, "[iop-native-stop] PADMAN RPC method %u command 0x%08X: %s\n", method, command, reason);
    std::fflush(stderr);
    std::abort();
}

void initialize_pad_area(std::uint32_t address) {
    EePadData initial{};
    std::memset(initial.data, 0xFF, sizeof initial.data);
    initial.state = kPadStateExecCommand;
    initial.request_state = kPadRequestBusy;
    initial.current_task = 0;
    initial.button_data_ready = 0;
    std::uint8_t *area = ps2::fast<std::uint8_t>(ee_physical(address));
    if (!area) return;
    std::memcpy(area, &initial, sizeof initial);
    std::memcpy(area + kPadRecordBytes, &initial, sizeof initial);
}

void fill_pad_record(EePadData &record, const sio2::ControllerState &controller, std::uint32_t frame) {
    std::memset(&record, 0, sizeof record);
    record.frame = frame;
    record.mode_config = 2u; // PADMAN MODE_CONFIG_READY
    record.mode_cur_id = kPadModePressure;
    record.model = 3u; // PAD_MODEL_DUALSHOCK2
    record.button_data_ready = controller.connected ? 1u : 0u;
    record.num_modes = 1u;
    record.mode_table[0] = kPadModePressure >> 4;
    record.num_actuators = 2u;
    record.state = controller.connected ? kPadStateStable : kPadStateDisconnected;
    record.request_state = kPadRequestComplete;
    record.current_task = 1u; // PADMAN TASK_UPDATE_PAD
    if (!controller.connected) {
        record.data[0] = 0xFFu;
        return;
    }

    // PADMAN's EE pad data begins with ok, mode, then the active-low button word.
    const std::uint16_t raw_buttons = static_cast<std::uint16_t>(~controller.buttons);
    record.data[0] = 0u;
    record.data[1] = kPadModePressure;
    record.data[2] = static_cast<std::uint8_t>(raw_buttons);
    record.data[3] = static_cast<std::uint8_t>(raw_buttons >> 8);
    std::copy(controller.axes.begin(), controller.axes.end(), record.data + 4);
    // PS2 response pressure order: right, left, up, down, triangle, circle,
    // cross, square, L1, R1, L2, R2.
    constexpr std::array<std::uint8_t, 12> kPressureButtons{{5, 7, 4, 6, 12, 13, 14, 15, 10, 11, 8, 9}};
    for (std::size_t i = 0; i < kPressureButtons.size(); ++i) {
        const std::uint8_t button = kPressureButtons[i];
        record.data[8 + i] = (controller.buttons & (1u << button)) ? controller.pressure[button] : 0u;
    }
    record.data[31] = 0x5Au;
    record.length = 32u;
}

bool pad_rpc(std::uint32_t method, std::uint32_t args, std::uint32_t args_size,
             std::uint32_t ret, std::uint32_t ret_size) {
    if (!valid_ee_range(args, args_size) || !valid_ee_range(ret, ret_size) ||
        args_size < 4u || ret_size < 24u) {
        if (kernel::native_strict()) strict_stop("invalid EE RPC buffer", method, 0xFFFFFFFFu);
        return true;
    }

    std::array<std::uint8_t, 128> request{};
    const std::uint32_t copy_size = std::min<std::uint32_t>(args_size, static_cast<std::uint32_t>(request.size()));
    const std::uint32_t args_physical = ee_physical(args);
    const std::uint32_t ret_physical = ee_physical(ret);
    std::memcpy(request.data(), ps2::fast<std::uint8_t>(args_physical), copy_size);
    const std::uint32_t command = ps2::load<std::uint32_t>(request.data());
    std::uint8_t *reply = ps2::fast<std::uint8_t>(ret_physical);
    if (!reply) {
        if (kernel::native_strict()) strict_stop("reply buffer is outside EE RAM", method, command);
        return true;
    }
    std::memcpy(reply, request.data(), std::min<std::uint32_t>(ret_size, static_cast<std::uint32_t>(request.size())));

    const auto set_result = [&](std::uint32_t value) {
        if (ret_size >= 16u) ps2::store<std::uint32_t>(reply + 12u, value);
    };
    if (method != kPadRpcFunction) {
        if (kernel::native_strict()) strict_stop("unsupported RPC function", method, command);
        set_result(0u);
        return true;
    }

    switch (command) {
    case 0x12u: // padGetModVersion; observed in the selected disc's PADMAN trace.
        set_result(0x422u);
        return true;
    case 0x10u: // padInit; observed in the selected disc's PADMAN trace.
        g_initialized = true;
        set_result(1u);
        return true;
    case 0x01u: { // padPortOpen
        if (!g_initialized) {
            if (kernel::native_strict()) strict_stop("padPortOpen before padInit", method, command);
            set_result(0u);
            return true;
        }
        const std::uint32_t port = ps2::load<std::uint32_t>(request.data() + 4u);
        const std::uint32_t slot = ps2::load<std::uint32_t>(request.data() + 8u);
        const std::uint32_t area = ps2::load<std::uint32_t>(request.data() + 16u);
        if (port >= 2u || slot >= 4u || (area & 0x3Fu) != 0u || !valid_ee_range(area, kPadAreaBytes)) {
            if (kernel::native_strict()) strict_stop("invalid pad port, slot, or 256-byte status area", method, command);
            set_result(0u);
            return true;
        }
        initialize_pad_area(area);
        g_open_pads[port][slot] = OpenPad{area, 0u, true};
        set_result(1u);
        // New libpad reads padArea in EE RAM. The legacy result is an IOP
        // pointer, so report null rather than inventing a guest allocation.
        if (ret_size >= 24u) ps2::store<std::uint32_t>(reply + 20u, 0u);
        return true;
    }
    default:
        if (kernel::native_strict()) strict_stop("unimplemented PADMAN RPC command", method, command);
        set_result(0u);
        return true;
    }
}

} // namespace

void reset() {
    g_active = false;
    g_initialized = false;
    g_open_pads = {};
}

void activate() {
    g_active = true;
    g_initialized = false;
    g_open_pads = {};
    sif::register_hle_override(kPadRpcSid0, pad_rpc);
    sif::register_hle_override(kPadRpcSid1, pad_rpc);
    std::fprintf(stderr, "[iop-native] PADMAN host service active (SIF 0x%08X / 0x%08X)\n", kPadRpcSid0, kPadRpcSid1);
}

void frame_tick() {
    if (!g_active || !g_initialized) return;
    for (std::uint32_t port = 0; port < g_open_pads.size(); ++port) {
        sio2::ControllerState controller;
        if (!sio2::controller_state(static_cast<int>(port), controller)) continue;
        for (std::uint32_t slot = 0; slot < g_open_pads[port].size(); ++slot) {
            OpenPad &open = g_open_pads[port][slot];
            if (!open.open || !valid_ee_range(open.ee_area, kPadAreaBytes)) continue;
            EePadData record{};
            ++open.frame;
            fill_pad_record(record, controller, open.frame);
            const std::uint32_t destination = ee_physical(open.ee_area) + ((open.frame & 1u) * kPadRecordBytes);
            std::uint8_t *memory = ps2::fast<std::uint8_t>(destination);
            if (memory) std::memcpy(memory, &record, sizeof record);
        }
    }
}

void state(ps2::state::Stream &s) {
    if (s.version() < 12u) {
        if (s.loading()) reset();
        return;
    }
    s.tag("padman");
    s.pod(g_active);
    s.pod(g_initialized);
    s.pod(g_open_pads);
}

void restore_sif_overrides() {
    if (g_active) {
        sif::register_hle_override(kPadRpcSid0, pad_rpc);
        sif::register_hle_override(kPadRpcSid1, pad_rpc);
    } else {
        sif::unregister_hle_override(kPadRpcSid0);
        sif::unregister_hle_override(kPadRpcSid1);
    }
}

} // namespace ps2::iop::padman
