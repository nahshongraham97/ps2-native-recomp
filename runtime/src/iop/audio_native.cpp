#include "audio_native.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "iop.hpp"
#include "kernel.hpp"
#include "spu2.hpp"
#include "compliance_census.hpp"

namespace ps2::iop::audio_native {

namespace {

constexpr unsigned kRingFrames = 512; // 512 stereo frames = 2048 bytes (one half buffer)
constexpr std::uint32_t kRamMask = 0xFFFFFu; // 1M words = 2MB sound RAM

bool valid_iop_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = phys(address);
    return physical < kRamSize && size <= static_cast<std::uint64_t>(kRamSize - physical);
}

// ---------------------------------------------------------------------------
// ADMA Streaming Core State
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Native Voice Engine State (Slice 2)
// ---------------------------------------------------------------------------

enum Phase { kOff, kAttack, kDecay, kSustain, kRelease };

struct Envelope {
    int counter = 0;
    std::int32_t level = 0;
    void step(int rate, bool decrease, bool exponential) {
        const int shift = rate >> 2;
        int cycles = 1 << std::max(0, shift - 11);
        if (exponential && !decrease && level > 0x6000) cycles *= 4;
        if (++counter < cycles) return;
        counter = 0;
        int delta = (decrease ? -8 + (rate & 3) : 7 - (rate & 3)) << std::max(0, 11 - shift);
        if (exponential && decrease) delta = delta * level / 0x8000;
        level = std::clamp(level + delta, 0, 0x7FFF);
    }
};

std::int32_t compute_volume(std::uint16_t reg, Envelope &e) {
    if (!(reg & 0x8000u)) return static_cast<std::int16_t>(reg << 1);
    e.step(reg & 0x7F, reg & 0x2000u, reg & 0x4000u);
    return e.level;
}

struct Voice {
    std::uint16_t voll = 0, volr = 0, pitch = 0, adsr1 = 0, adsr2 = 0;
    Envelope vol[2];
    std::uint32_t ssa = 0, lsax = 0, next = 0, block[2] = {};
    bool lsax_written = false, key_on = false, end = false;
    Phase phase = kOff;
    Envelope env;
    std::int16_t buffer[56] = {};
    std::int32_t s1 = 0, s2 = 0;
    std::uint32_t position = 0;
};

struct CoreVoiceState {
    Voice voices[24];
    std::uint32_t dry_mask[2] = {0x00FFFFFFu, 0x00FFFFFFu}; // [0]=VMIXL, [1]=VMIXR
    std::uint32_t wet_mask[2] = {0, 0};                     // [0]=VMIXEL, [1]=VMIXER
    std::uint16_t mvol[2] = {0x3FFFu, 0x3FFFu};
};

CoreVoiceState g_core_voices[2];

void decode_adpcm_block(Voice &v, std::int16_t *out) {
    static const int kFilter[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    std::uint16_t *ram = sound_ram();
    if (!ram) return;

    ps2::census::audio_native_adpcm_block_decoded();
    const std::uint32_t at = v.next & kRamMask;
    const std::uint16_t header = ram[at];
    const int shift = header & 0xF, filter = std::min((header >> 4) & 0xF, 4);
    const unsigned flags = header >> 8;

    for (int i = 0; i < 28; ++i) {
        const std::uint16_t word = ram[(at + 1 + i / 4) & kRamMask];
        std::int32_t sample = static_cast<std::int16_t>(((word >> ((i & 3) * 4)) & 0xF) << 12) >> shift;
        sample += (v.s1 * kFilter[filter][0] + v.s2 * kFilter[filter][1] + 32) >> 6;
        sample = std::clamp(sample, -32768, 32767);
        v.s2 = v.s1;
        v.s1 = sample;
        out[i] = static_cast<std::int16_t>(sample);
    }
    v.block[out == v.buffer ? 0 : 1] = at;
    if ((flags & 4u) && !v.lsax_written) v.lsax = at;
    v.next = (at + 8) & kRamMask;
    if (flags & 1u) {
        v.next = v.lsax;
        if (!(flags & 2u)) {
            v.phase = kRelease;
            v.env.level = 0;
            v.end = true;
        }
    }
}

void activate_voice_key(Voice &v) {
    v.key_on = false;
    v.lsax_written = false;
    v.end = false;
    v.next = v.ssa;
    v.s1 = v.s2 = 0;
    v.position = 0;
    v.phase = kAttack;
    v.env = Envelope{};
    decode_adpcm_block(v, v.buffer);
    decode_adpcm_block(v, v.buffer + 28);
}

void advance_envelope(Voice &v) {
    Envelope &e = v.env;
    switch (v.phase) {
    case kAttack:
        e.step((v.adsr1 >> 8) & 0x7F, false, v.adsr1 & 0x8000u);
        if (e.level >= 0x7FFF) v.phase = kDecay;
        break;
    case kDecay:
        e.step(((v.adsr1 >> 4) & 0xF) << 2, true, true);
        if (e.level <= ((v.adsr1 & 0xF) + 1) * 0x800) v.phase = kSustain;
        break;
    case kSustain:
        e.step((v.adsr2 >> 6) & 0x7F, v.adsr2 & 0x4000u, v.adsr2 & 0x8000u);
        break;
    case kRelease:
        e.step((v.adsr2 & 0x1F) << 2, true, v.adsr2 & 0x20u);
        if (e.level <= 0) v.phase = kOff;
        break;
    default:
        break;
    }
}

} // namespace

// The native sound service's sequencer advances with the mixer (see
// set_frame_hook). Nothing else in the engine depends on it.
void (*g_frame_hook)(std::size_t frames) = nullptr;

void set_frame_hook(void (*hook)(std::size_t frames)) { g_frame_hook = hook; }

std::uint16_t *sound_ram() {
    return spu2::ram();
}

void reset() {
    g_streams[0] = {};
    g_streams[1] = {};
    g_core_voices[0] = {};
    g_core_voices[1] = {};
}

// ---------------------------------------------------------------------------
// ADMA Stream Interface
// ---------------------------------------------------------------------------

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

std::uint32_t get_next_fill_address(int core) {
    const CoreStream &s = g_streams[core & 1];
    return s.iop_base + (s.active_buf * s.half_bytes);
}

std::uint32_t get_current_address(int core) {
    return g_streams[core & 1].current_iop_addr;
}

// ---------------------------------------------------------------------------
// Native Voice Interface
// ---------------------------------------------------------------------------

void key_on(int core, std::uint32_t mask) {
    const int c = core & 1;
    CoreVoiceState &cv = g_core_voices[c];
    for (int i = 0; i < 24; ++i) {
        if (!(mask & (1u << i))) continue;
        Voice &v = cv.voices[i];
        v.key_on = true;
        ps2::census::audio_native_voice_keyed_on(c, i);
        std::fprintf(stderr, "[audio-native-voice-on] core=%d voice=%d ssa=0x%06X pitch=0x%04X vol=%d/%d\n",
                     c, i, v.ssa, v.pitch, v.vol[0].level, v.vol[1].level);
    }
}

void key_off(int core, std::uint32_t mask) {
    const int c = core & 1;
    CoreVoiceState &cv = g_core_voices[c];
    for (int i = 0; i < 24; ++i) {
        if (!(mask & (1u << i))) continue;
        Voice &v = cv.voices[i];
        if (v.key_on) {
            v.key_on = false;
        } else if (v.phase != kOff) {
            v.phase = kRelease;
        }
    }
}

bool set_voice_param(int core, int voice, std::uint32_t index, std::uint16_t value) {
    const int c = core & 1;
    if (voice < 0 || voice >= 24) return false;
    Voice &v = g_core_voices[c].voices[voice];
    switch (index) {
    case 0: // VOLL
        v.voll = value;
        if (!(value & 0x8000u)) v.vol[0].level = static_cast<std::int16_t>(value << 1);
        return true;
    case 1: // VOLR
        v.volr = value;
        if (!(value & 0x8000u)) v.vol[1].level = static_cast<std::int16_t>(value << 1);
        return true;
    case 2: // PITCH
        v.pitch = value;
        return true;
    case 3: // ADSR1
        v.adsr1 = value;
        return true;
    case 4: // ADSR2
        v.adsr2 = value;
        return true;
    case 5: // ENVX (writable)
        v.env.level = static_cast<std::int16_t>(value);
        return true;
    default:
        return false;
    }
}

bool get_voice_param(int core, int voice, std::uint32_t index, std::uint16_t &value) {
    const int c = core & 1;
    if (voice < 0 || voice >= 24) return false;
    const Voice &v = g_core_voices[c].voices[voice];
    switch (index) {
    case 0: value = v.voll; return true;
    case 1: value = v.volr; return true;
    case 2: value = v.pitch; return true;
    case 3: value = v.adsr1; return true;
    case 4: value = v.adsr2; return true;
    case 5: value = static_cast<std::uint16_t>(v.env.level); return true;
    case 6: value = static_cast<std::uint16_t>(v.vol[0].level); return true;
    case 7: value = static_cast<std::uint16_t>(v.vol[1].level); return true;
    default: return false;
    }
}

bool set_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t value) {
    const int c = core & 1;
    if (voice < 0 || voice >= 24) return false;
    Voice &v = g_core_voices[c].voices[voice];
    switch (index) {
    case 32: // SSA
        v.ssa = value;
        return true;
    case 33: // LSAX
        v.lsax = value;
        v.lsax_written = true;
        return true;
    case 34: // next / NAX
        v.next = value;
        return true;
    default:
        return false;
    }
}

bool get_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t &value) {
    const int c = core & 1;
    if (voice < 0 || voice >= 24) return false;
    const Voice &v = g_core_voices[c].voices[voice];
    switch (index) {
    case 32: value = v.ssa; return true;
    case 33: value = v.lsax; return true;
    case 34: {
        const std::uint32_t nax = (v.block[0] + 1 + (v.position >> 14)) & kRamMask;
        value = nax;
        return true;
    }
    default: return false;
    }
}

bool set_switch(int core, std::uint32_t index, std::uint32_t value) {
    const int c = core & 1;
    CoreVoiceState &cv = g_core_voices[c];
    switch (index) {
    case 21: // KON (0x1500)
        key_on(c, value);
        return true;
    case 22: // KOFF (0x1600)
        key_off(c, value);
        return true;
    case 23: // ENDX (0x1700)
        for (int i = 0; i < 24; ++i) cv.voices[i].end = false;
        return true;
    case 24: // VMIXL (0x1800)
        cv.dry_mask[0] = value & 0x00FFFFFFu;
        return true;
    case 25: // VMIXEL (0x1900)
        cv.wet_mask[0] = value & 0x00FFFFFFu;
        return true;
    case 26: // VMIXR (0x1A00)
        cv.dry_mask[1] = value & 0x00FFFFFFu;
        return true;
    case 27: // VMIXER (0x1B00)
        cv.wet_mask[1] = value & 0x00FFFFFFu;
        return true;
    default:
        return false;
    }
}

bool get_switch(int core, std::uint32_t index, std::uint32_t &value) {
    const int c = core & 1;
    const CoreVoiceState &cv = g_core_voices[c];
    switch (index) {
    case 23: { // ENDX (0x1700)
        value = 0;
        for (int i = 0; i < 24; ++i) {
            if (cv.voices[i].end) value |= (1u << i);
        }
        return true;
    }
    case 24: value = cv.dry_mask[0]; return true;
    case 25: value = cv.wet_mask[0]; return true;
    case 26: value = cv.dry_mask[1]; return true;
    case 27: value = cv.wet_mask[1]; return true;
    default: return false;
    }
}

void set_master_volume(int core, std::uint16_t left, std::uint16_t right) {
    const int c = core & 1;
    g_core_voices[c].mvol[0] = left;
    g_core_voices[c].mvol[1] = right;
}

// ---------------------------------------------------------------------------
// Unified Native Mixer (Slice 3)
// ---------------------------------------------------------------------------

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

void render_voices_batch(int core, std::size_t batch_frames, std::int32_t (*dry)[2], std::int32_t (*wet)[2]) {
    const int c = core & 1;
    CoreVoiceState &cv = g_core_voices[c];
    if (g_frame_hook) g_frame_hook(batch_frames);

    for (int i = 0; i < 24; ++i) {
        Voice &v = cv.voices[i];
        if (!v.key_on && v.phase == kOff) continue;
        if (v.key_on) activate_voice_key(v);

        const std::uint32_t bit = 1u << i;
        const std::uint32_t step = std::min<std::uint32_t>(v.pitch, 0x3FFFu);
        const bool sweep_l = (v.voll & 0x8000u) != 0, sweep_r = (v.volr & 0x8000u) != 0;
        const std::int32_t fixed_l = static_cast<std::int16_t>(v.voll << 1), fixed_r = static_cast<std::int16_t>(v.volr << 1);

        std::uint32_t position = v.position;
        for (std::size_t n = 0; n < batch_frames; ++n) {
            if (v.phase == kOff) break;
            const std::uint32_t index = position >> 12, frac = position & 0xFFFu;
            const std::int32_t sample = (v.buffer[index] * static_cast<std::int32_t>(0x1000 - frac) +
                                         v.buffer[index + 1] * static_cast<std::int32_t>(frac)) >> 12;
            position += step;
            while ((position >> 12) >= 28) {
                position -= 28u << 12;
                std::memmove(v.buffer, v.buffer + 28, 28 * sizeof(std::int16_t));
                v.position = position;
                decode_adpcm_block(v, v.buffer + 28);
            }
            advance_envelope(v);

            const std::int32_t out = sample * v.env.level >> 15;
            const std::int32_t l = out * (sweep_l ? compute_volume(v.voll, v.vol[0]) : fixed_l) >> 15;
            const std::int32_t r = out * (sweep_r ? compute_volume(v.volr, v.vol[1]) : fixed_r) >> 15;

            if (cv.dry_mask[0] & bit) dry[n][0] += l;
            if (cv.dry_mask[1] & bit) dry[n][1] += r;
            if (cv.wet_mask[0] & bit) wet[n][0] += l;
            if (cv.wet_mask[1] & bit) wet[n][1] += r;
        }
        v.position = position;
        ps2::census::audio_native_voice_frame_rendered(batch_frames);
    }
}

} // namespace ps2::iop::audio_native
