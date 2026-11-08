#pragma once

#include "../../../../include/armageddon/retail_pad_event.hpp"
#include "../../../../include/armageddon/retail_health.hpp"
#include "ps2/native_hook.hpp"
#include "ps2/generated.hpp"
#include "ps2/system.hpp"
#include "ps2/memory.hpp"
#include "ps2/cpu.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace mka::combat {

// Portable C++17 bit reinterpretation primitives
inline float from_bits(std::uint32_t bits) {
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

inline std::uint32_t to_bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

// Guest PC Constants: Cross-Unit Boundaries
constexpr std::uint32_t kPadScannerGuestPc   = 0x001637D0u; // From unit_0008 (entry 328u in unit_0018)
constexpr std::uint32_t kHealthScalarGuestPc = 0x00197BC0u; // From unit_0041 (Hit Dispatcher, RA 0x00207728)

// RDRAM Layout
constexpr std::uint32_t kRingBufferAddr       = 0x0065ed40u;
constexpr std::uint32_t kRecentRingAddr       = 0x0065e600u;
constexpr std::uint32_t kWriteIndexAddr       = 0x0064c3acu;
constexpr std::uint32_t kRecentIndexAddr      = 0x0064e0ecu;
constexpr std::uint32_t kLastTickAddr         = 0x0064e0e4u;

constexpr std::uint32_t kSubZeroHealthAddr   = 0x00660180u;
constexpr std::uint32_t kModeWordAddr        = 0x0064dea4u;
constexpr std::uint32_t kGatekeeperFlagsAddr = 0x00660080u;
constexpr std::uint32_t kPlayer2ObjPtr       = 0x006601a4u;

// 1. Health Scalar Hook (Sub-Zero Damage Deduction)
inline void hook_subzero_health(ps2::Cpu &c) {
    const std::uint32_t mode        = ps2::ld32(kModeWordAddr);
    const std::uint8_t flags_660080 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr));
    const std::uint8_t flags_660084 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr + 4));

    // Gatekeeper: Non-standard / knockout modes delegate explicitly to authentic unit code
    static std::uint64_t s_health_calls = 0;
    const bool is_target_combat = (mode < 2 && (flags_660080 & 4u) != 0 && (flags_660084 & 4u) == 0);
    if (++s_health_calls <= 5 || is_target_combat) {
        std::fprintf(stderr, "[mka-health] invocation #%llu: mode=%u flags=0x%02X/0x%02X (%s)\n",
                     static_cast<unsigned long long>(s_health_calls), mode, flags_660080, flags_660084,
                     is_target_combat ? "intercepting" : "delegating");
    }

    if (!is_target_combat) {
        ps2::hook::unregister_hook(kHealthScalarGuestPc);
        ps2::call(c, kHealthScalarGuestPc, 0);
        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
        return;
    }

    // Standard Hit Path: Bit-cast delta and commit with IEEE-754 integrity
    const float delta   = from_bits(c.f[12]);
    const float current = from_bits(ps2::ld32(kSubZeroHealthAddr));
    const std::uint32_t ret_addr = c.r[31].w[0];

    const float new_health = armageddon::retail::apply_observed_hit_to_health(
        current, delta, mode, flags_660080, flags_660084);

    ps2::st32(kSubZeroHealthAddr, to_bits(new_health)); // Preserves 0x3f6d288d

    const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
    if (p2 != 0) {
        const float old_acc = from_bits(ps2::ld32(p2 + 0x3dc));
        ps2::st32(p2 + 0x3dc, to_bits(old_acc - delta));
    }

    std::fprintf(stderr, "[health] delta %.4ff applied to current %.4ff -> new %.4ff (mode=%u)\n",
                 delta, current, new_health, mode);

    c.pc = ret_addr; // Clean unwind to unit_0041 RA (0x00207728)
}

// 2. PAD Scanner Input Hook (Player 1 Triangle Press)
inline void hook_pad_scanner_event(ps2::Cpu &c) {
    const std::uint32_t channel       = c.r[4].w[0]; // a0
    const std::uint32_t button_code   = c.r[5].w[0]; // a1 (4 = PAD_RUP / Triangle)
    const std::uint32_t tick          = c.r[6].w[0]; // a2
    const std::uint32_t label_addr    = c.r[7].w[0]; // a3
    const std::uint32_t mapping_index = c.r[8].w[0]; // t0
    const std::uint32_t ret_addr      = c.r[31].w[0];

    // Gatekeeper: Non-target buttons delegate to authentic unit_0018
    static std::uint64_t s_pad_events = 0;
    const bool is_target_triangle = (channel == 0 && button_code == 4);
    if (++s_pad_events <= 5 || is_target_triangle) {
        std::fprintf(stderr, "[mka-pad] event #%llu: ch=%u btn=%u (target=%s)\n",
                     static_cast<unsigned long long>(s_pad_events), channel, button_code,
                     is_target_triangle ? "TRIANGLE_HIT" : "delegate");
    }

    if (!is_target_triangle) {
        ps2::hook::unregister_hook(kPadScannerGuestPc);
        ps2::call(c, kPadScannerGuestPc, 0);
        ps2::hook::register_hook(kPadScannerGuestPc, hook_pad_scanner_event, "MKA_PadScanner");
        return;
    }

    // Native Path: Reconstruct channel state and commit atomically
    armageddon::retail::PadEventChannel0 pad_ch{};
    pad_ch.write_index     = ps2::ld32(kWriteIndexAddr);
    pad_ch.recent_index    = ps2::ld32(kRecentIndexAddr);
    pad_ch.last_event_tick = ps2::ld32(kLastTickAddr);

    for (std::size_t i = 0; i < pad_ch.records.size(); ++i) {
        const std::uint32_t at = kRingBufferAddr + static_cast<std::uint32_t>(i * 20);
        pad_ch.records[i] = {
            ps2::ld32(at), ps2::ld32(at + 4), ps2::ld32(at + 8), ps2::ld32(at + 12), ps2::ld32(at + 16)
        };
    }
    for (std::size_t i = 0; i < pad_ch.recent_mappings.size(); ++i) {
        pad_ch.recent_mappings[i] = ps2::ld32(kRecentRingAddr + static_cast<std::uint32_t>(i * 4));
    }

    const std::uint32_t prev_index = pad_ch.write_index;
    armageddon::retail::append_pad_rup_event(pad_ch, tick, label_addr, 0u, mapping_index);

    ps2::st32(kWriteIndexAddr,  pad_ch.write_index);
    ps2::st32(kRecentIndexAddr, pad_ch.recent_index);
    ps2::st32(kLastTickAddr,    pad_ch.last_event_tick);

    const std::uint32_t write_record_addr = kRingBufferAddr + (pad_ch.write_index * 20);
    const auto &rec = pad_ch.records[pad_ch.write_index];
    ps2::st32(write_record_addr + 0,  rec.code);
    ps2::st32(write_record_addr + 4,  rec.tick);
    ps2::st32(write_record_addr + 8,  rec.label_address);
    ps2::st32(write_record_addr + 12, rec.context);
    ps2::st32(write_record_addr + 16, rec.mapping_index);

    const std::uint32_t recent_entry_addr = kRecentRingAddr + (pad_ch.recent_index * 4);
    ps2::st32(recent_entry_addr, pad_ch.recent_mappings[pad_ch.recent_index]);

    std::fprintf(stderr, "[ring] write_index advanced from %u to %u (tick=0x%04X, button=%u)\n",
                 prev_index, pad_ch.write_index, tick, button_code);

    c.pc = ret_addr; // Clean unwind to caller RA
}

// 3. Registration Trigger
inline void register_mka_combat_hooks() {
    ps2::hook::register_hook(kPadScannerGuestPc, hook_pad_scanner_event, "MKA_PadScanner");
    ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
    std::fprintf(stderr, "[mka-hooks] Combat interception armed for guest PCs 0x%08X and 0x%08X\n",
                 kPadScannerGuestPc, kHealthScalarGuestPc);
}

} // namespace mka::combat
