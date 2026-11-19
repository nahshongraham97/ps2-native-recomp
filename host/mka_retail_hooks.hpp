#pragma once

#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"
#include "ps2/native_hook.hpp"
#include "ps2/os.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace mka::retail {

// Transitional EE-ABI observation. These helpers never replace retail gameplay
// or movie logic. The fixed address ABI itself still requires native conversion.
constexpr std::uint32_t kRingBufferAddr = 0x0065ed40u;
constexpr std::uint32_t kWriteIndexAddr = 0x0064c3acu;
constexpr std::uint32_t kLastTickAddr = 0x0064e0e4u;
constexpr std::uint32_t kSubZeroHealthAddr = 0x00660180u;
constexpr std::uint32_t kModeWordAddr = 0x0064dea4u;
constexpr std::uint32_t kGatekeeperFlagsAddr = 0x00660080u;
using RetailUnit = void (*)(ps2::Cpu&, std::uint32_t, std::uint32_t);

inline bool is_interception_enabled() { return true; } // Observation only.
inline float from_bits(std::uint32_t bits) {
    float result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

inline void handle_subzero_health(ps2::Cpu& c, std::uint32_t depth, RetailUnit original) {
    const auto before = ps2::ld32(kSubZeroHealthAddr);
    const auto delta = c.f[12];
    const auto mode = ps2::ld32(kModeWordAddr);
    original(c, 204u, depth);
    std::fprintf(stderr,
        "[mka-health] after AOT segment mode=%u health_bits=%08X->%08X requested_delta_bits=%08X r2=%08X pc=%08X budget=%d\n",
        mode, before, ps2::ld32(kSubZeroHealthAddr), delta, c.r[2].w[0], c.pc, c.budget);
}

inline void handle_pad_event(ps2::Cpu& c, std::uint32_t depth, RetailUnit original) {
    // a0..a4 are the original function arguments; s3 is not the channel ABI.
    const auto channel = c.r[4].w[0];
    const auto code = c.r[5].w[0];
    const auto tick = c.r[6].w[0];
    const auto label = c.r[7].w[0];
    const auto mapping = c.r[8].w[0];
    original(c, 309u, depth);
    std::fprintf(stderr,
        "[mka-pad] retail event channel=%u code=%u tick=%08X label=%08X mapping=%u\n",
        channel, code, tick, label, mapping);
}

inline void handle_video_stream(ps2::Cpu& c, std::uint32_t depth, RetailUnit original) {
    const auto id = c.r[4].w[0];
    const auto argument = c.r[5].w[0];
    std::fprintf(stderr, "[mka-video] retail request id=%u argument=%08X\n", id, argument);
    original(c, 126u, depth);
    std::fprintf(stderr, "[mka-video] after AOT segment id=%u r2=%08X pc=%08X budget=%d\n",
        id, c.r[2].w[0], c.pc, c.budget);
}

inline void process_audio(const std::int16_t* samples, std::size_t frames) {
    // An immutable output tap: no sound selection, synthesized events or DSP.
    static std::uint64_t blocks = 0;
    ++blocks;
    if (blocks != 1 && blocks % 256 != 0) return;
    std::fprintf(stderr, "[mka-audio] observed output block=%llu frames=%zu samples=%s\n",
        static_cast<unsigned long long>(blocks), frames, samples ? "present" : "null");
}

inline void register_retail_observers_and_leafs() {
    // Health remains the original compiled retail body, reached by the linker
    // observer. find_hook is a replacement seam and must not swallow that call.
    ps2::hook::unregister_hook(0x00197bc0u);
    std::fprintf(stderr, "[mka-hooks] read-only retail call observers enabled; no authored combat or movie bypass\n");
    // Host-native replacement hooks for Metrowerks CodeWarrior Overlay static initializers:
    // Original three-instruction leaf: ADDIU v1,zero,2; JR ra; SW v1,-20304(gp).
    ps2::hook::register_hook(0x0084A100u, [](ps2::Cpu &c) {
        std::fprintf(stderr, "[mka-overlay] MOVIER.OVL static ctor intercepted at 0x%08X (RA=0x%08X)\n", c.pc, c.r[31].w[0]);
        c.r[3].sd[0] = 2;
        ps2::st32(c.r[28].w[0] - 20304u, 2u);
        c.pc = c.r[31].w[0];
    }, "MOVIER_ctor");

    // MOVIER.OVL playback state setter function at 0x007DC1F8
    ps2::hook::register_hook(0x007DC1F8u, [](ps2::Cpu &c) {
        c.r[2].sd[0] = static_cast<std::int32_t>(0x00830000u);
        c.r[3].sd[0] = static_cast<std::int32_t>(0x00830000u);
        ps2::st32(0x0082C774u, c.r[4].w[0]);
        ps2::st32(0x0082C778u, c.r[5].w[0]);
        c.pc = c.r[31].w[0];
    }, "movier_007DC1F8");

    // MOVIER.OVL stream state initializer at 0x00812750 / 0x00812760
    ps2::hook::register_hook(0x00812750u, [](ps2::Cpu &c) {
        ps2::st32(c.r[4].w[0] + 0u, 0u);
        ps2::st32(c.r[4].w[0] + 8u, c.r[5].w[0]);
        ps2::st32(c.r[4].w[0] + 4u, 0u);
        c.pc = c.r[31].w[0];
    }, "movier_00812750");
    ps2::hook::register_hook(0x00812760u, [](ps2::Cpu &c) {
        c.pc = c.r[31].w[0];
    }, "movier_00812760");

    // Original three-instruction leaf: ADDIU v1,zero,1; JR ra; SW v1,-20304(gp).
    ps2::hook::register_hook(0x007C4980u, [](ps2::Cpu &c) {
        std::fprintf(stderr, "[mka-overlay] GAMER.OVL static ctor intercepted at 0x%08X (RA=0x%08X)\n", c.pc, c.r[31].w[0]);
        c.r[3].sd[0] = 1;
        ps2::st32(c.r[28].w[0] - 20304u, 1u);
        c.pc = c.r[31].w[0];
    }, "GAMER_ctor");

    // NETR's initializer is 0x00856700. 0x00856780 is file-end/BSS, not code.
    ps2::hook::register_hook(0x00856700u, [](ps2::Cpu &c) {
        std::fprintf(stderr, "[mka-overlay] NETR.OVL static ctor intercepted at 0x%08X (RA=0x%08X)\n", c.pc, c.r[31].w[0]);
        c.r[3].sd[0] = 3;
        ps2::st32(c.r[28].w[0] - 20304u, 3u);
        c.pc = c.r[31].w[0];
    }, "NETR_ctor");
}

} // namespace mka::retail
