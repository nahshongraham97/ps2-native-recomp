#include "audio_native.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "iop.hpp"
#include "kernel.hpp"
#include "spu2.hpp"
#include "compliance_census.hpp"

namespace ps2::iop::audio_native {

namespace {

constexpr unsigned kRingFrames = 512; // 512 stereo frames = 2048 bytes (one half buffer)

bool valid_iop_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = phys(address);
    return physical < kRamSize && size <= static_cast<std::uint64_t>(kRamSize - physical);
}

struct CoreStream {
    bool active = false;
    bool loop = false;
    std::uint32_t mode = 0;
    std::uint32_t direction = 0;
    std::uint32_t iop_base = 0;
    std::uint32_t total_bytes = 0;
    std::uint32_t half_bytes = 0;
    std::uint32_t active_buf = 0;
    std::uint32_t current_iop_addr = 0;
    std::uint32_t bytes_pulled_in_half = 0;

    std::int16_t ring[kRingFrames][2] = {};
    unsigned ring_read = 0;
    unsigned ring_count = 0;
};

CoreStream g_streams[2];

void refill_ring(int core) {
    CoreStream &s = g_streams[core & 1];
    if (!s.active) return;

    while (s.active && s.ring_count + 256 <= kRingFrames) {
        const std::uint32_t chunk = std::min<std::uint32_t>(1024u, s.half_bytes - s.bytes_pulled_in_half);
        if (chunk == 0) break;
        if (!valid_iop_range(s.current_iop_addr, chunk)) {
            std::fprintf(stderr, "[audio-native] core %d invalid IOP range 0x%08X size %u\n", core, s.current_iop_addr, chunk);
            s.active = false;
            break;
        }

        const std::uint8_t *src = ram_ptr(s.current_iop_addr);
        const std::uint32_t frames = chunk / 4u;
        for (std::uint32_t i = 0; i < frames; ++i) {
            std::int16_t l, r;
            std::memcpy(&l, src + i * 2, 2);
            std::memcpy(&r, src + chunk / 2 + i * 2, 2);
            const unsigned slot = (s.ring_read + s.ring_count + i) % kRingFrames;
            s.ring[slot][0] = l;
            s.ring[slot][1] = r;
        }
        s.ring_count += frames;
        s.current_iop_addr += chunk;
        s.bytes_pulled_in_half += chunk;

        if (s.bytes_pulled_in_half >= s.half_bytes) {
            s.bytes_pulled_in_half = 0;
            ps2::census::audio_native_block_consumed();
            // Signal DMA completion job to wake up guest interrupt / callbacks
            kernel::schedule(kernel::now() + 32, kernel::Job{kernel::kJobDmaDone, (core & 1) ? 7u : 4u});
            if (s.loop) {
                s.active_buf = 1 - s.active_buf;
                s.current_iop_addr = s.iop_base + (s.active_buf * s.half_bytes);
            } else {
                s.active = false;
                break;
            }
        }
    }
}

} // namespace

void reset() {
    g_streams[0] = {};
    g_streams[1] = {};
}

bool has_active_stream(int core) {
    return g_streams[core & 1].active;
}

void start_stream(int core, std::uint32_t mode, std::uint32_t iop_address, std::uint32_t bytes, std::uint32_t start_address) {
    CoreStream &s = g_streams[core & 1];
    s = {};
    s.mode = mode;
    s.direction = mode & 3u;
    s.loop = (mode & 0x10u) != 0;
    s.total_bytes = bytes;
    s.half_bytes = s.loop ? (bytes / 2u) : bytes;
    s.iop_base = iop_address;

    if (s.direction == 3u) { // kDmaWriteFrom
        if (start_address == 0) start_address = iop_address;
        std::uint32_t offset = start_address - iop_address;
        if (offset > s.half_bytes) {
            if (s.loop) {
                offset -= s.half_bytes;
                s.active_buf = 1;
            } else {
                return;
            }
        } else {
            s.active_buf = 0;
        }
        if (offset & 1023u) offset = (offset + 1023u) & ~1023u;
        s.current_iop_addr = s.iop_base + (s.half_bytes * s.active_buf) + offset;
    } else {
        s.active_buf = 0;
        s.current_iop_addr = s.iop_base;
    }

    s.active = true;
    s.ring_read = 0;
    s.ring_count = 0;
    s.bytes_pulled_in_half = 0;

    std::fprintf(stderr, "[audio-native] core %d start_stream: base=0x%08X total=%u half=%u loop=%d dir=%u\n",
                 core & 1, s.iop_base, s.total_bytes, s.half_bytes, s.loop ? 1 : 0, s.direction);

    spu2::keep_ticking();
    refill_ring(core & 1);
}

void stop_stream(int core) {
    CoreStream &s = g_streams[core & 1];
    std::fprintf(stderr, "[audio-native] core %d stop_stream\n", core & 1);
    s.active = false;
    s.ring_count = 0;
    s.ring_read = 0;
}

std::uint32_t get_active_buffer(int core) {
    return g_streams[core & 1].active_buf;
}

std::uint32_t get_current_address(int core) {
    return g_streams[core & 1].current_iop_addr;
}

void render_stream_sample(int core, const bool input_wet[2], std::int32_t dry[2], std::int32_t wet[2],
                          std::int16_t &raw_l, std::int16_t &raw_r, const std::uint16_t bvol[2]) {
    CoreStream &s = g_streams[core & 1];
    if (!s.active) {
        raw_l = 0;
        raw_r = 0;
        return;
    }

    refill_ring(core & 1);

    if (!s.ring_count) {
        raw_l = 0;
        raw_r = 0;
        return;
    }

    const unsigned slot = s.ring_read;
    raw_l = s.ring[slot][0];
    raw_r = s.ring[slot][1];
    s.ring_read = (s.ring_read + 1) % kRingFrames;
    --s.ring_count;

    const std::int32_t l = static_cast<std::int32_t>(raw_l) * static_cast<std::int16_t>(bvol[0]) >> 15;
    const std::int32_t r = static_cast<std::int32_t>(raw_r) * static_cast<std::int16_t>(bvol[1]) >> 15;
    dry[0] += l;
    dry[1] += r;
    if (input_wet[0]) wet[0] += l;
    if (input_wet[1]) wet[1] += r;

    ps2::census::audio_native_frame_mixed();
    ps2::census::spu2_peak_sample(raw_l, raw_r);
}

} // namespace ps2::iop::audio_native
