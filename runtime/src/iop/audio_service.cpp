#include "audio_service.hpp"
#include "compliance_census.hpp"

#include "hw.hpp"
#include "iop.hpp"
#include "kernel.hpp"
#include "../load_stats.hpp"
#include "ps2/os.hpp"

#include "ps2/state.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <vector>

namespace ps2::iop::audio_native {

constexpr std::uint32_t kRamWords = 1u << 20;
constexpr std::uint32_t kRamMask = kRamWords - 1;
constexpr std::uint64_t kCyclesPerSample = 768;
constexpr unsigned kBatch = 32;
constexpr unsigned kInputFrames = 512;
constexpr unsigned kRingFrames = 512;  // 512 stereo frames = 2048 bytes (one half buffer)
constexpr std::uint32_t kInputBase[2] = {0x2000u, 0x2400u};
constexpr unsigned kSpuLine = 9;

bool valid_iop_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = phys(address);
    return physical < kRamSize && size <= static_cast<std::uint64_t>(kRamSize - physical);
}
constexpr std::uint32_t kRingWords = 0x200;
static_assert(kRingWords % kBatch == 0, "a batch must not straddle the ring end");

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

struct Transfer {
    std::uint32_t madr = 0, bytes = 0;
    int channel = -1;
};

struct Core {
    Voice voices[24];
    std::uint16_t attr = 0, admas = 0;
    std::uint32_t irqa = 0x800u, tsa = 0;
    std::uint16_t mvol[2] = {0x3FFF, 0x3FFF}, bvol[2] = {0x7FFF, 0x7FFF}, avol[2] = {0x7FFF, 0x7FFF}, evol[2] = {};
    // The retail's mixer setup, decoded into named fields. The emulator kept
    // this in a register array indexed by the sound device's own register
    // numbers; that array was the device model. What the game puts in it is
    // setup data - which voices go dry and into the reverb, the reverb's ring
    // addresses and volumes, the main volumes - so the service keeps it as
    // named state.
    std::uint16_t mix_control = 0;                       // the mixer control (MMIX)
    std::uint32_t voice_dry[2] = {}, voice_wet[2] = {};   // the voice mix masks
    std::uint16_t iir_alpha = 0, iir_beta = 0;
    std::uint32_t reverb_start = 0;                       // the reverb ring's first word
    std::uint16_t reverb_end = 0;                         // the ring's last page
    std::uint32_t reverb_address[22] = {};                // the ring's 22 addresses
    std::uint16_t reverb_volume[10] = {};                 // the ring's ten volumes
    bool irq = false;
    std::int16_t input[kInputFrames][2] = {};
    unsigned input_read = 0, input_count = 0;
    std::deque<Transfer> transfers;
};

std::vector<std::uint16_t> g_ram(kRamWords);
Core g_cores[2];
bool g_block_end[2][24][2];

bool *block_end(const Voice &v) {
    const int c = &v >= g_cores[1].voices ? 1 : 0;
    return g_block_end[c][&v - g_cores[c].voices];
}
std::uint16_t g_spdif_out = 0, g_spdif_mode = 0, g_spdif_media = 0;
std::function<void(const std::int16_t *, std::size_t)> g_sink;
std::uint64_t g_next_tick = 0;
bool g_reverb_off = false;  // PS2_SPU2_REVERB=0 leaves the reverb out (to compare with the sound from before it existed)
bool g_tick_pending = false;
std::uint64_t g_first_tick = 0, g_last_tick = 0;

// A 48000 Hz 16-bit stereo WAV file written while the sound is made, so a long run needs no memory for it. The header is
// brought up to date about once a second, so a run that is killed still leaves a file that plays up to that point.
class WavFile {
public:
    ~WavFile() { close(); }
    bool open(const std::string &path) {
        if (file_) return false;
        file_ = os::open(path, "wb");
        if (!file_) {
            std::fprintf(stderr, "[spu2] cannot write %s\n", path.c_str());
            return false;
        }
        path_ = path;
        frames_ = synced_ = 0;
        header();
        return true;
    }
    explicit operator bool() const { return file_ != nullptr; }
    void write(const std::int16_t *samples, std::size_t frames) {
        if (!file_ || frames_ + frames > kMaxFrames) return;
        std::fwrite(samples, 4, frames, file_);
        frames_ += frames;
        if (frames_ - synced_ >= 48000) header();
    }
    // closes the file and says how much went in: the chip makes 48000 samples per second of emulated IOP time from its first
    // sample on, which the second figure checks
    void close() {
        if (!file_) return;
        header();
        std::fclose(file_);
        file_ = nullptr;
        const double emulated = g_first_tick ? static_cast<double>(g_last_tick - g_first_tick) / 36864000.0 + kBatch / 48000.0 : 0.0;
        std::fprintf(stderr, "[spu2] wrote %llu samples (%.2f s) to %s; the chip ran %.2f s of emulated time\n",
                     static_cast<unsigned long long>(frames_), static_cast<double>(frames_) / 48000.0, path_.c_str(), emulated);
    }

private:
    static constexpr std::uint64_t kMaxFrames = (0xFFFFFFFFull - 36) / 4;
    void header() {
        const std::uint32_t bytes = static_cast<std::uint32_t>(frames_ * 4);
        const std::uint32_t words[] = {0x46464952u, 36 + bytes, 0x45564157u, 0x20746D66u, 16,         0x00020001u,
                                       48000,       48000 * 4,  0x00100004u, 0x61746164u, bytes};
        std::fseek(file_, 0, SEEK_SET);
        std::fwrite(words, 4, sizeof words / 4, file_);
        std::fseek(file_, 0, SEEK_END);
        std::fflush(file_);
        synced_ = frames_;
    }
    std::FILE *file_ = nullptr;
    std::string path_;
    std::uint64_t frames_ = 0, synced_ = 0;
};

// PS2_DUMP_AUDIO: everything the sound chip puts out, before the host's sound queue. PS2_AUDIO_DUMP is the sound that goes to
// the sound device: with a device (a window run) the host writes that file itself (host/window.cpp), without one there is no
// queue in between and it is the same chip output, written here.
WavFile g_chip_wav, g_out_wav;

void check_irq(std::uint32_t address, std::uint32_t words) {
    for (Core &k : g_cores) {
        if (!(k.attr & 0x40u) || k.irq) continue;
        if (((k.irqa - address) & kRamMask) >= words) continue;
        k.irq = true;
        static const bool trace = std::getenv("PS2_TRACE_SPU2") != nullptr;
        if (trace) std::fprintf(stderr, "[spu2] core %d irq at 0x%05X (access 0x%05X+%u) time %llu\n", static_cast<int>(&k - g_cores), k.irqa, address, words,
                                static_cast<unsigned long long>(kernel::now()));
        hw::raise_interrupt(kSpuLine);
    }
}

void decode_block(Voice &v, std::int16_t *out) {
    static const int kFilter[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    const std::uint32_t at = v.next & kRamMask;
    const std::uint16_t header = g_ram[at];
    const int shift = header & 0xF, filter = std::min((header >> 4) & 0xF, 4);
    const unsigned flags = header >> 8;
    for (int i = 0; i < 28; ++i) {
        const std::uint16_t word = g_ram[(at + 1 + i / 4) & kRamMask];
        std::int32_t sample = static_cast<std::int16_t>(((word >> ((i & 3) * 4)) & 0xF) << 12) >> shift;
        sample += (v.s1 * kFilter[filter][0] + v.s2 * kFilter[filter][1] + 32) >> 6;
        if (sample >= 32767 || sample <= -32768) {
            static std::uint64_t s_clip_count = 0;
            if (++s_clip_count <= 20) {
                std::fprintf(stderr, "[spu2-warn] ADPCM rail saturation sample=%d at=0x%06X header=0x%04X shift=%d filter=%d\n",
                             sample, at, header, shift, filter);
            }
        }
        sample = std::clamp(sample, -32768, 32767);
        v.s2 = v.s1;
        v.s1 = sample;
        out[i] = static_cast<std::int16_t>(sample);
    }
    v.block[out == v.buffer ? 0 : 1] = at;
    block_end(v)[out == v.buffer ? 0 : 1] = (flags & 1u) != 0;
    if ((flags & 4u) && !v.lsax_written) v.lsax = at;
    v.next = (at + 8) & kRamMask;
    if (flags & 1u) {
        v.next = v.lsax;
        if (!(flags & 2u)) {
            v.phase = kRelease;
            v.env.level = 0;
        }
    }
}

void key_on(Voice &v) {
    v.key_on = false;
    v.lsax_written = false;
    v.end = false;
    v.next = v.ssa;
    v.s1 = v.s2 = 0;
    v.position = 0;
    v.phase = kAttack;
    v.env = Envelope{};
    decode_block(v, v.buffer);
    decode_block(v, v.buffer + 28);
    check_irq(v.block[0], 8);
}

void update_envelope(Voice &v) {
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

// The volume register is 15 bits: 0x0000 is silence and 0x7FFF is full scale.
// It is a magnitude, so it must not be shifted into the sign bit: 0x7FFF << 1
// overflows a signed 16-bit value to -2, and a voice multiplied by -2 is
// silent. The level stays in the register's own 0..0x7FFF range and the mixer
// scales by it directly.
std::int32_t volume(std::uint16_t reg, Envelope &e) {
    if (!(reg & 0x8000u)) return static_cast<std::int32_t>(reg & 0x7FFFu);
    e.step(reg & 0x7F, reg & 0x2000u, reg & 0x4000u);
    return e.level;
}

std::int16_t clamp16(std::int32_t v) { return static_cast<std::int16_t>(std::clamp(v, -32768, 32767)); }

// Where one core sends what it makes (registers VMIXL/VMIXR = dry left/right, VMIXEL/VMIXER = into the reverb, one bit per voice, and
// MMIX for the mixer as a whole: bits 9 and 8 let the voices into the reverb left and right, bits 5 and 4 do the same for the sound
// that comes in by DMA (ADMA)). The reverb is only on while bit 7 of the core's attribute register is set.
struct Routing {
    std::uint32_t dry[2] = {}, wet[2] = {};
    bool input_wet[2] = {};
    bool reverb = false;
};

Routing routing(const Core &k) {
    const std::uint16_t mmix = k.mix_control;
    Routing r;
    r.reverb = (k.attr & 0x80u) != 0 && !g_reverb_off;
    r.dry[0] = k.voice_dry[0];
    r.dry[1] = k.voice_dry[1];
    if (r.reverb) {
        r.wet[0] = (mmix & 0x200u) ? k.voice_wet[0] : 0;
        r.wet[1] = (mmix & 0x100u) ? k.voice_wet[1] : 0;
        r.input_wet[0] = (mmix & 0x20u) != 0;
        r.input_wet[1] = (mmix & 0x10u) != 0;
    }
    return r;
}

void render_voice(Voice &v, unsigned voice, const Routing &route, std::int32_t (*dry)[2], std::int32_t (*wet)[2], std::int16_t *raw) {
    if (v.key_on) key_on(v);
    const std::uint32_t bit = 1u << voice;
    const std::uint32_t step = std::min<std::uint32_t>(v.pitch, 0x3FFFu);
    const bool sweep_l = v.voll & 0x8000u, sweep_r = v.volr & 0x8000u;
    const std::int32_t fixed_l = static_cast<std::int32_t>(v.voll & 0x7FFFu), fixed_r = static_cast<std::int32_t>(v.volr & 0x7FFFu);
    std::uint32_t position = v.position;
    std::uint64_t rendered = 0;
    for (unsigned n = 0; n < kBatch; ++n) {
        if (v.phase == kOff) break;
        const std::uint32_t index = position >> 12, frac = position & 0xFFFu;
        const std::int32_t sample = (v.buffer[index] * static_cast<std::int32_t>(0x1000 - frac) + v.buffer[index + 1] * static_cast<std::int32_t>(frac)) >> 12;
        position += step;
        while ((position >> 12) >= 28) {
            position -= 28u << 12;
            std::memmove(v.buffer, v.buffer + 28, 28 * sizeof(std::int16_t));
            v.end |= block_end(v)[0];
            v.block[0] = v.block[1];
            block_end(v)[0] = block_end(v)[1];
            v.position = position;
            check_irq(v.block[0], 8);
            decode_block(v, v.buffer + 28);
            ps2::census::audio_native_adpcm_block_decoded();
        }
        update_envelope(v);
        ++rendered;
        const std::int32_t out = sample * v.env.level >> 15;
        if (raw) raw[n] = clamp16(out);
        // the volume sweeps step once per sample whatever the voice is sent to
        const std::int32_t l = out * (sweep_l ? volume(v.voll, v.vol[0]) : fixed_l) >> 15;
        const std::int32_t r = out * (sweep_r ? volume(v.volr, v.vol[1]) : fixed_r) >> 15;
        if (route.dry[0] & bit) dry[n][0] += l;
        if (route.dry[1] & bit) dry[n][1] += r;
        if (route.wet[0] & bit) wet[n][0] += l;
        if (route.wet[1] & bit) wet[n][1] += r;
    }
    v.position = position;
    if (rendered) ps2::census::audio_native_voice_frame_rendered(rendered);
}

void feed_input(int core) {
    if (audio_native::has_active_stream(core)) return;
    Core &k = g_cores[core];
    while (!k.transfers.empty() && k.input_count + 256 <= kInputFrames) {
        Transfer &t = k.transfers.front();
        const std::uint32_t chunk = std::min<std::uint32_t>(t.bytes, 1024);
        const std::uint32_t frames = std::max<std::uint32_t>(chunk / 4, 1);
        const std::uint8_t *src = ram_ptr(t.madr);
        static const char *const trace_pieces = std::getenv("PS2_TRACE_ADMA");  // "2": every piece the chip takes in, with the time (see dma())
        if (trace_pieces && trace_pieces[0] == '2')
            std::fprintf(stderr, "[adma-piece] %.5f s core %d reads 0x%06X (%u bytes), %u bytes of the block left, %u frames waiting\n",
                         static_cast<double>(kernel::now()) / 36864000.0, core, t.madr, chunk, t.bytes, k.input_count);
        // PS2_DUMP_ADMA=file (porting work): the sound exactly as the chip takes it in, piece by piece, at the moment it takes it
        static const char *const dump_adma = std::getenv("PS2_DUMP_ADMA");
        if (dump_adma) {
            if (std::FILE *f = std::fopen(dump_adma, "ab")) {
                std::fwrite(src, 1, chunk, f);
                std::fclose(f);
            }
        }
        for (std::uint32_t i = 0; i < frames; ++i) {
            std::int16_t l, r;
            std::memcpy(&l, src + i * 2, 2);
            std::memcpy(&r, src + chunk / 2 + i * 2, 2);
            const unsigned slot = (k.input_read + k.input_count + i) % kInputFrames;
            k.input[slot][0] = l;
            k.input[slot][1] = r;
            g_ram[kInputBase[core] + slot] = static_cast<std::uint16_t>(l);
            g_ram[kInputBase[core] + 0x200 + slot] = static_cast<std::uint16_t>(r);
        }
        k.input_count += frames;
        t.madr += chunk;
        t.bytes -= chunk;
        if (!t.bytes) {
            const int channel = t.channel;
            k.transfers.pop_front();
            if (channel >= 0) kernel::schedule(kernel::now() + 32, kernel::Job{kernel::kJobDmaDone, static_cast<std::uint32_t>(channel)});
        }
    }
}

void render_input(int core, const Routing &route, std::int32_t *dry, std::int32_t *wet, std::int16_t &raw_l, std::int16_t &raw_r);
void render_stream_sample(int core, const bool input_wet[2], std::int32_t dry[2], std::int32_t wet[2],
                          std::int16_t &raw_l, std::int16_t &raw_r, const std::uint16_t bvol[2]);

void render_input(int core, const Routing &route, std::int32_t *dry, std::int32_t *wet, std::int16_t &raw_l, std::int16_t &raw_r) {
    if (has_active_stream(core)) {
        render_stream_sample(core, route.input_wet, dry, wet, raw_l, raw_r, g_cores[core].bvol);
        return;
    }
    Core &k = g_cores[core];
    if (!k.input_count) return;
    const unsigned slot = k.input_read;
    check_irq(kInputBase[core] + slot, 1);
    raw_l = k.input[slot][0];
    raw_r = k.input[slot][1];
    const std::int32_t l = k.input[slot][0] * static_cast<std::int16_t>(k.bvol[0]) >> 15, r = k.input[slot][1] * static_cast<std::int16_t>(k.bvol[1]) >> 15;
    dry[0] += l;
    dry[1] += r;
    if (route.input_wet[0]) wet[0] += l;
    if (route.input_wet[1]) wet[1] += r;
    k.input_read = (k.input_read + 1) % kInputFrames;
    --k.input_count;
}

// The main volume is a 15-bit magnitude as well: 0x7FFF is full scale. The
// shift the emulator used overflowed that to -2, which silenced the whole
// voice mix, while the sound that arrives by DMA (scaled by the input volume,
// which was used unsigned) stayed audible - so the game was audible only in
// its movies.
std::int32_t master(std::uint16_t reg) {
    if (reg & 0x8000u) return 0x7FFF;
    return static_cast<std::int32_t>(reg & 0x7FFFu);
}

// The reverb. The chip's reverb is the one of the first PlayStation's sound chip, written about in the public PlayStation hardware
// documentation (nocash's psx-spx, "SPU Reverb"): a short ring of memory (from ESA to the end given by EEA), two same-side and two
// cross-side reflections with a damping filter, four early echoes (combs) and two all-pass filters, once for the left and once for the
// right side. This version runs one step for every two output samples (24000 steps a second; the register values of the game are
// those of the first chip, counted in 16-bit words) and reads the sum of everything sent to it (the wet side of the mixer). Where
// each register sits (the 22 addresses from 0x2E4, the 10 volumes in the master block from 0x10) is read from what libsd writes:
// the values of its "Studio Large" preset (dAPF1 0x38C = 0xE3 * 4, volumes 6F60 4FA8 BCE0 4510 BEF0 A680 5680 52C0 8000 8000, which sit
// in the master block at 0x14 to 0x26 of each core; 0x10 and 0x12 are two more words nobody here uses).
// The master block from 0x10 on is kept in the unused end of the core's register array (0x3E0 on), so a saved state needs nothing new
// (a state saved before this existed has no volumes in it: its reverb stays silent until the game sets the reverb again).
enum { kRevAddress = 0x2E4, kRevVolume = 0x3E4 };
enum { kApf1Size, kApf2Size, kSameLDst, kSameRDst, kComb1L, kComb1R, kComb2L, kComb2R, kSameLSrc, kSameRSrc, kDiffLDst, kDiffRDst, kComb3L, kComb3R,
       kComb4L, kComb4R, kDiffLSrc, kDiffRSrc, kApf1LDst, kApf1RDst, kApf2LDst, kApf2RDst };
enum { kVolIir, kVolComb1, kVolComb2, kVolComb3, kVolComb4, kVolWall, kVolApf1, kVolApf2, kVolInL, kVolInR };

std::int32_t mul15(std::int32_t sample, std::uint16_t volume) { return sample * static_cast<std::int16_t>(volume) >> 15; }

// The 'out' of 32 samples for the 32 samples of the wet side 'in'. step_base counts the reverb steps from the start of the run.
void run_reverb(Core &k, std::uint64_t step_base, const std::int32_t (*in)[2], std::int32_t (*out)[2], std::int32_t (&last)[2]) {
    const std::uint32_t start = k.reverb_start & kRamMask, end = ((static_cast<std::uint32_t>(k.reverb_end) << 16) | 0xFFFFu) & kRamMask;
    const std::uint64_t size = end >= start ? end - start + 1u : 0u;
    if (size < 64) {
        for (unsigned n = 0; n < kBatch; ++n) out[n][0] = out[n][1] = 0;
        return;
    }
    const std::uint32_t *address = k.reverb_address;
    const std::uint16_t *vol = k.reverb_volume;
    for (unsigned t = 0; t < kBatch / 2; ++t) {
        const std::uint64_t at = (step_base + t) % size;
        // a cell of the ring, 'offset' words away from where this step is (a step further on, the same cell is one nearer to the writer)
        const auto cell = [&](std::uint32_t offset, int adjust = 0) -> std::uint16_t & { return g_ram[start + (at + offset + size + adjust) % size]; };
        const std::int32_t in_l = mul15((in[t * 2][0] + in[t * 2 + 1][0]) / 2, vol[kVolInL]), in_r = mul15((in[t * 2][1] + in[t * 2 + 1][1]) / 2, vol[kVolInR]);
        const auto reflect = [&](std::int32_t input, std::uint32_t source, std::uint32_t target) {
            const std::int32_t before = static_cast<std::int16_t>(cell(target, -1));
            const std::int32_t sum = input + mul15(static_cast<std::int16_t>(cell(source)), vol[kVolWall]) - before;
            cell(target) = static_cast<std::uint16_t>(clamp16(mul15(sum, vol[kVolIir]) + before));
        };
        reflect(in_l, address[kSameLSrc], address[kSameLDst]);
        reflect(in_r, address[kSameRSrc], address[kSameRDst]);
        reflect(in_l, address[kDiffRSrc], address[kDiffLDst]);
        reflect(in_r, address[kDiffLSrc], address[kDiffRDst]);
        const auto side = [&](unsigned c1, unsigned c2, unsigned c3, unsigned c4, unsigned apf1, unsigned apf2) {
            std::int32_t o = mul15(static_cast<std::int16_t>(cell(address[c1])), vol[kVolComb1]) + mul15(static_cast<std::int16_t>(cell(address[c2])), vol[kVolComb2]) +
                             mul15(static_cast<std::int16_t>(cell(address[c3])), vol[kVolComb3]) + mul15(static_cast<std::int16_t>(cell(address[c4])), vol[kVolComb4]);
            o = clamp16(o);
            const std::uint32_t back1 = address[kApf1Size], back2 = address[kApf2Size];
            const std::int32_t d1 = static_cast<std::int16_t>(cell(address[apf1] + size - back1 % size));
            o = clamp16(o - mul15(d1, vol[kVolApf1]));
            cell(address[apf1]) = static_cast<std::uint16_t>(o);
            o = clamp16(mul15(o, vol[kVolApf1]) + d1);
            const std::int32_t d2 = static_cast<std::int16_t>(cell(address[apf2] + size - back2 % size));
            o = clamp16(o - mul15(d2, vol[kVolApf2]));
            cell(address[apf2]) = static_cast<std::uint16_t>(o);
            return clamp16(mul15(o, vol[kVolApf2]) + d2);
        };
        const std::int32_t left = side(kComb1L, kComb2L, kComb3L, kComb4L, kApf1LDst, kApf2LDst);
        const std::int32_t right = side(kComb1R, kComb2R, kComb3R, kComb4R, kApf1RDst, kApf2RDst);
        // two output samples for one step: halfway from the last step's result, then the new one
        out[t * 2][0] = (last[0] + left) / 2;
        out[t * 2][1] = (last[1] + right) / 2;
        out[t * 2 + 1][0] = left;
        out[t * 2 + 1][1] = right;
        last[0] = left;
        last[1] = right;
    }
}

// Each core streams every sample into fixed rings in SPU RAM, per core c at c*0x800: input L/R (core 1's input is core 0's
// output), voice 1, voice 3; at 0x1000+c*0x800: dry L/R, wet L/R. Those writes trip IRQA, and some games (Burnout 3:
// IRQA 0x400) use that as their audio clock. One range check per ring per batch keeps it cheap.
void write_rings(int core, const std::int16_t (*rings)[kBatch]) {
    const auto pos = static_cast<std::uint32_t>(g_next_tick / (kBatch * kCyclesPerSample) % (kRingWords / kBatch) * kBatch);
    for (std::uint32_t r = 0; r < 8; ++r) {
        const std::uint32_t at = (r < 4 ? 0u : 0x1000u) + core * 0x800u + (r & 3u) * kRingWords + pos;
        for (unsigned n = 0; n < kBatch; ++n) g_ram[at + n] = static_cast<std::uint16_t>(rings[r][n]);
        check_irq(at, kBatch);
    }
}

// The frame hook: called once per mixed batch with the number of frames it
// covers, so the native sound service can advance its sequencer's note timing
// in step with the mixer.
void (*g_frame_hook)(std::size_t frames) = nullptr;

void set_frame_hook(void (*hook)(std::size_t frames)) { g_frame_hook = hook; }

void tick() {
    const cost::Timer time_it(cost::kSpuTick);
    // The native sound service's sequencer advances with the mixer.
    if (g_frame_hook) g_frame_hook(kBatch);
    std::int16_t out[kBatch * 2];
    std::int32_t sum[2][kBatch][2] = {}, wet[2][kBatch][2] = {}, echo[2][kBatch][2] = {};
    std::int16_t rings[2][8][kBatch] = {};
    static std::int32_t last_echo[2][2] = {};
    static const bool count = std::getenv("PS2_SPU2_COUNT") != nullptr;
    static std::uint64_t ticks = 0, active = 0, phases[5] = {};
    for (int c = 0; c < 2; ++c) {
        const Routing route = routing(g_cores[c]);
        for (int i = 0; i < 24; ++i) {
            Voice &v = g_cores[c].voices[i];
            if (!v.key_on && v.phase == kOff) continue;
            if (count) {
                ++active;
                ++phases[v.phase];
            }
            render_voice(v, static_cast<unsigned>(i), route, sum[c], wet[c], i == 1 ? rings[c][2] : i == 3 ? rings[c][3] : nullptr);
        }
        for (unsigned n = 0; n < kBatch; ++n) {
            rings[c][4][n] = clamp16(sum[c][n][0]);
            rings[c][5][n] = clamp16(sum[c][n][1]);
        }
        std::int16_t unused[2];
        for (unsigned n = 0; n < kBatch; ++n)
            render_input(c, route, sum[c][n], wet[c][n], c ? unused[0] : rings[0][0][n], c ? unused[1] : rings[0][1][n]);
        if (route.reverb) {
            run_reverb(g_cores[c], g_next_tick / (2 * kCyclesPerSample), wet[c], echo[c], last_echo[c]);
            for (unsigned n = 0; n < kBatch; ++n) {
                rings[c][6][n] = clamp16(wet[c][n][0]);
                rings[c][7][n] = clamp16(wet[c][n][1]);
                sum[c][n][0] += echo[c][n][0] * master(g_cores[c].evol[0]) >> 15;
                sum[c][n][1] += echo[c][n][1] * master(g_cores[c].evol[1]) >> 15;
            }
        }
    }
    for (unsigned n = 0; n < kBatch; ++n) {
        std::int32_t mix[2] = {};
        for (int c = 0; c < 2; ++c) {
            const std::int32_t l = sum[c][n][0] * master(g_cores[c].mvol[0]) >> 15, r = sum[c][n][1] * master(g_cores[c].mvol[1]) >> 15;
            if (!c) {
                rings[1][0][n] = clamp16(l);
                rings[1][1][n] = clamp16(r);
            }
            mix[0] += l;
            mix[1] += r;
        }
        out[n * 2] = clamp16(mix[0]);
        out[n * 2 + 1] = clamp16(mix[1]);
        ps2::census::spu2_peak_sample(out[n * 2], out[n * 2 + 1]);
    }
    write_rings(0, rings[0]);
    write_rings(1, rings[1]);
    if (count && ++ticks % 15000 == 0)
        std::fprintf(stderr, "[spu2] %.1f voices per batch (attack %.1f decay %.1f sustain %.1f release %.1f)\n", static_cast<double>(active) / ticks,
                     static_cast<double>(phases[1]) / ticks, static_cast<double>(phases[2]) / ticks, static_cast<double>(phases[3]) / ticks, static_cast<double>(phases[4]) / ticks);
    if (!g_first_tick) g_first_tick = kernel::now();
    g_last_tick = kernel::now();
    g_chip_wav.write(out, kBatch);
    g_out_wav.write(out, kBatch);
    if (g_sink) g_sink(out, kBatch);
    feed_input(0);
    feed_input(1);
    g_next_tick += kBatch * kCyclesPerSample;
    kernel::schedule(g_next_tick, kernel::Job{kernel::kJobSpuTick});
}

void keep_ticking_internal() {
    if (g_tick_pending) return;
    g_tick_pending = true;
    g_next_tick = kernel::now() + kBatch * kCyclesPerSample;
    kernel::schedule(g_next_tick, kernel::Job{kernel::kJobSpuTick});
}

std::uint32_t set_hi(std::uint32_t address, std::uint32_t v) { return ((v & 0xFu) << 16) | (address & 0xFFFFu); }
std::uint32_t set_lo(std::uint32_t address, std::uint32_t v) { return (address & 0xF0000u) | (v & 0xFFFFu); }

std::uint32_t read_voice(Core &k, std::uint32_t off) {
    if (off < 0x180u) {
        const Voice &v = k.voices[off >> 4];
        switch (off & 0xFu) {
        case 0x0: return v.voll;
        case 0x2: return v.volr;
        case 0x4: return v.pitch;
        case 0x6: return v.adsr1;
        case 0x8: return v.adsr2;
        case 0xA: return static_cast<std::uint32_t>(v.env.level);
        case 0xC: return static_cast<std::uint32_t>(v.vol[0].level);
        default: return static_cast<std::uint32_t>(v.vol[1].level);
        }
    }
    const Voice &v = k.voices[(off - 0x1C0u) / 12];
    const std::uint32_t nax = (v.block[0] + 1 + (v.position >> 14)) & kRamMask;
    switch ((off - 0x1C0u) % 12) {
    case 0: return v.ssa >> 16;
    case 2: return v.ssa & 0xFFFFu;
    case 4: return v.lsax >> 16;
    case 6: return v.lsax & 0xFFFFu;
    case 8: return nax >> 16;
    default: return nax & 0xFFFFu;
    }
}

// PS2_TRACE_SPU2 also reports voice volumes written in sweep mode (bit 15), which fades a voice
// instead of setting it. Every one of them is logged by default; PS2_TRACE_SWEEP=<n> samples one
// in n (0 turns the line off). Nothing here is dropped silently.
bool trace_sweep(std::uint32_t value) {
    static const bool trace = std::getenv("PS2_TRACE_SPU2") != nullptr;
    static const long every = [] {
        const char *value = std::getenv("PS2_TRACE_SWEEP");
        return (value && *value) ? std::strtol(value, nullptr, 10) : 1L;  // unset - every sweep write
    }();
    static unsigned long reported = 0;
    if (!trace || !(value & 0x8000u) || every <= 0) return false;
    return every == 1 || (++reported % static_cast<unsigned long>(every) == 0);
}

void write_voice(Core &k, std::uint32_t off, std::uint32_t value) {
    if (off < 0x180u) {
        Voice &v = k.voices[off >> 4];
        switch (off & 0xFu) {
        case 0x0:
            if (trace_sweep(value)) std::fprintf(stderr, "[spu2] voice volume sweep L 0x%04X (voice %u)\n", value, off >> 4);
            v.voll = static_cast<std::uint16_t>(value);
            if (!(value & 0x8000u)) v.vol[0].level = static_cast<std::int16_t>(value & 0x7FFFu);
            break;
        case 0x2:
            if (trace_sweep(value)) std::fprintf(stderr, "[spu2] voice volume sweep R 0x%04X (voice %u)\n", value, off >> 4);
            v.volr = static_cast<std::uint16_t>(value);
            if (!(value & 0x8000u)) v.vol[1].level = static_cast<std::int16_t>(value & 0x7FFFu);
            break;
        case 0x4: v.pitch = static_cast<std::uint16_t>(value); break;
        case 0x6: v.adsr1 = static_cast<std::uint16_t>(value); break;
        case 0x8: v.adsr2 = static_cast<std::uint16_t>(value); break;
        case 0xA: v.env.level = static_cast<std::int16_t>(value); break;
        default: break;
        }
        return;
    }
    Voice &v = k.voices[(off - 0x1C0u) / 12];
    switch ((off - 0x1C0u) % 12) {
    case 0: v.ssa = set_hi(v.ssa, value); break;
    case 2: v.ssa = set_lo(v.ssa, value); break;
    case 4: v.lsax = set_hi(v.lsax, value); v.lsax_written = true; break;
    case 6: v.lsax = set_lo(v.lsax, value); v.lsax_written = true; break;
    case 8: v.next = set_hi(v.next, value); break;
    default: v.next = set_lo(v.next, value); break;
    }
}

void key(Core &k, std::uint32_t mask, bool on) {
    const int core_id = (&k == &g_cores[1]) ? 1 : 0;
    std::fprintf(stderr, "[spu2-key] core=%d mask=0x%06X on=%d\n", core_id, mask, on ? 1 : 0);
    for (int i = 0; i < 24; ++i) {
        if (!(mask & (1u << i))) continue;
        Voice &v = k.voices[i];
        if (on) {
            v.key_on = true;
            ps2::census::spu2_key_on(core_id, i);
            std::fprintf(stderr, "  [spu2-voice-on] core=%d voice=%d ssa=0x%06X pitch=0x%04X vol=%d/%d\n",
                         core_id, i, v.ssa, v.pitch, v.vol[0].level, v.vol[1].level);
        } else if (v.key_on) {
            v.key_on = false;
            ps2::census::spu2_key_off(core_id, i);
            std::fprintf(stderr, "  [spu2-voice-cancel] core=%d voice=%d\n", core_id, i);
        } else if (v.phase != kOff) {
            v.phase = kRelease;
            ps2::census::spu2_key_off(core_id, i);
            std::fprintf(stderr, "  [spu2-voice-release] core=%d voice=%d\n", core_id, i);
        }
    }
}

std::uint32_t read_core(int core, std::uint32_t off) {
    Core &k = g_cores[core];
    if (off < 0x180u || (off >= 0x1C0u && off < 0x2E0u)) return read_voice(k, off);
    switch (off) {
    case 0x19A: return k.attr;
    case 0x19C: return k.irqa >> 16;
    case 0x19E: return k.irqa & 0xFFFFu;
    case 0x1A8: return k.tsa >> 16;
    case 0x1AA: return k.tsa & 0xFFFFu;
    case 0x1B0: return k.admas;
    case 0x340:
    case 0x342: {
        std::uint32_t endx = 0;
        for (int i = 0; i < 24; ++i)
            if (k.voices[i].end) endx |= 1u << i;
        return off == 0x340 ? endx & 0xFFFFu : endx >> 16;
    }
    case 0x344: return 0x80u;
    default: break;
    }
    // The setup the service keeps and the guest can read back.
    switch (off) {
    case 0x180: return k.iir_alpha;
    case 0x184: return k.iir_beta;
    case 0x188: return k.voice_dry[0] & 0xFFFFu;
    case 0x18A: return (k.voice_dry[0] >> 16) & 0xFFu;
    case 0x18C: return k.voice_wet[0] & 0xFFFFu;
    case 0x18E: return (k.voice_wet[0] >> 16) & 0xFFu;
    case 0x190: return k.voice_dry[1] & 0xFFFFu;
    case 0x192: return (k.voice_dry[1] >> 16) & 0xFFu;
    case 0x194: return k.voice_wet[1] & 0xFFFFu;
    case 0x196: return (k.voice_wet[1] >> 16) & 0xFFu;
    case 0x198: return k.mix_control;
    case 0x2E0: return k.reverb_start >> 16;
    case 0x2E2: return k.reverb_start & 0xFFFFu;
    case 0x33C: return k.reverb_end;
    default: break;
    }
    if (off >= 0x2E4u && off < 0x334u) {
        const std::uint32_t slot = (off - 0x2E4u) / 4u;
        const bool high = (off & 2u) == 0;
        return high ? (k.reverb_address[slot] >> 16) : (k.reverb_address[slot] & 0xFFFFu);
    }
    return 0;
}

// Some of the retail's setup values are two 16-bit registers: the reverb's
// ring addresses hold the high half first, the voice mix masks hold the low
// half first. These helpers keep both orders in one place.
void set_high_half(std::uint32_t &field, std::uint32_t off, std::uint32_t value) {
    if ((off & 2u) == 0) field = (field & 0xFFFFu) | (value << 16);
    else field = (field & 0xFFFF0000u) | (value & 0xFFFFu);
}

void set_low_half24(std::uint32_t &field, std::uint32_t off, std::uint32_t value) {
    if ((off & 2u) == 0) field = (field & 0xFFFF0000u) | (value & 0xFFFFu);
    else field = (field & 0xFFFFFFu) | ((value & 0xFFu) << 16);
}

void write_core(int core, std::uint32_t off, std::uint32_t value) {
    Core &k = g_cores[core];
    static const bool trace = std::getenv("PS2_TRACE_SPU2") != nullptr;
    if (trace && off >= 0x180u && !(off >= 0x1C0u && off < 0x2E0u) && off != 0x1AC)
        std::fprintf(stderr, "[spu2] %.3f s core %d reg 0x%03X = 0x%04X\n", g_first_tick ? static_cast<double>(kernel::now() - g_first_tick) / 36864000.0 : 0.0, core, off,
                     value);
    if (off < 0x180u || (off >= 0x1C0u && off < 0x2E0u)) {
        write_voice(k, off, value);
        return;
    }
    switch (off) {
    case 0x180: k.iir_alpha = static_cast<std::uint16_t>(value); return;
    case 0x184: k.iir_beta = static_cast<std::uint16_t>(value); return;
    case 0x188: case 0x18A: set_low_half24(k.voice_dry[0], off, value); return;
    case 0x18C: case 0x18E: set_low_half24(k.voice_wet[0], off, value); return;
    case 0x190: case 0x192: set_low_half24(k.voice_dry[1], off, value); return;
    case 0x194: case 0x196: set_low_half24(k.voice_wet[1], off, value); return;
    case 0x198: k.mix_control = static_cast<std::uint16_t>(value); return;
    case 0x19A:
        k.attr = static_cast<std::uint16_t>(value);
        if (!(value & 0x40u)) k.irq = false;
        return;
    case 0x19C: k.irqa = set_hi(k.irqa, value); return;
    case 0x19E: k.irqa = set_lo(k.irqa, value); return;
    case 0x1A0: key(k, value, true); return;
    case 0x1A2: key(k, value << 16, true); return;
    case 0x1A4: key(k, value, false); return;
    case 0x1A6: key(k, value << 16, false); return;
    case 0x1A8: k.tsa = set_hi(k.tsa, value); return;
    case 0x1AA: k.tsa = set_lo(k.tsa, value); return;
    case 0x1AC:
        check_irq(k.tsa, 1);
        g_ram[k.tsa] = static_cast<std::uint16_t>(value);
        k.tsa = (k.tsa + 1) & kRamMask;
        return;
    case 0x1B0:
        k.admas = static_cast<std::uint16_t>(value);
        if (!value) k.input_count = k.input_read = 0;
        return;
    case 0x340:
        for (int i = 0; i < 16; ++i) k.voices[i].end = false;
        return;
    case 0x342:
        for (int i = 16; i < 24; ++i) k.voices[i].end = false;
        return;
    case 0x2E0: case 0x2E2: set_high_half(k.reverb_start, off, value); return;
    case 0x33C: k.reverb_end = static_cast<std::uint16_t>(value); return;
    case 0x33E: return;  // the low half of the ring's end address is not used
    default: break;
    }
    if (off >= 0x2E4u && off < 0x334u) {
        set_high_half(k.reverb_address[(off - 0x2E4u) / 4u], off, value);
        return;
    }
}

// The master block of a core: the main volumes, the input volumes and the
// reverb's ten volumes.
bool master_read(const Core &k, std::uint32_t off, std::uint16_t &value) {
    switch (off) {
    case 0x0: value = k.mvol[0]; return true;
    case 0x2: value = k.mvol[1]; return true;
    case 0x4: value = k.evol[0]; return true;
    case 0x6: value = k.evol[1]; return true;
    case 0x8: value = k.avol[0]; return true;
    case 0xA: value = k.avol[1]; return true;
    case 0xC: value = k.bvol[0]; return true;
    case 0xE: value = k.bvol[1]; return true;
    default: break;
    }
    if (off >= 0x14u && off <= 0x26u) {
        value = k.reverb_volume[(off - 0x14u) / 2u];
        return true;
    }
    return false;
}

bool master_write(Core &k, std::uint32_t off, std::uint16_t value) {
    switch (off) {
    case 0x0: k.mvol[0] = value; return true;
    case 0x2: k.mvol[1] = value; return true;
    case 0x4: k.evol[0] = value; return true;
    case 0x6: k.evol[1] = value; return true;
    case 0x8: k.avol[0] = value; return true;
    case 0xA: k.avol[1] = value; return true;
    case 0xC: k.bvol[0] = value; return true;
    case 0xE: k.bvol[1] = value; return true;
    default: break;
    }
    if (off >= 0x14u && off <= 0x26u) {
        k.reverb_volume[(off - 0x14u) / 2u] = value;
        return true;
    }
    return false;
}

std::uint32_t read_global(std::uint32_t off) {
    switch (off) {
    case 0x7C0: return g_spdif_out;
    case 0x7C2: return (g_cores[0].irq ? 4u : 0u) | (g_cores[1].irq ? 8u : 0u);
    case 0x7C6: return g_spdif_mode;
    case 0x7C8: return g_spdif_media;
    default: return 0;
    }
}

void write_global(std::uint32_t off, std::uint32_t value) {
    switch (off) {
    case 0x7C0: g_spdif_out = static_cast<std::uint16_t>(value); break;
    case 0x7C6: g_spdif_mode = static_cast<std::uint16_t>(value); break;
    case 0x7C8: g_spdif_media = static_cast<std::uint16_t>(value); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// The ADMA stream: the game streaming sound into the service.
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
                s.current_iop_addr = s.iop_base + (s.half_bytes * s.active_buf);
            } else {
                s.active = false;
                break;
            }
        }
    }
}

// The ADMA stream's samples: the game streaming sound into the service.
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

void reset() {
    kernel::register_job(kernel::kJobSpuTick, [](const kernel::Job &) { tick(); });
    std::fill(g_ram.begin(), g_ram.end(), 0);
    for (Core &k : g_cores) k = Core{};
    std::memset(g_block_end, 0, sizeof g_block_end);
    g_spdif_out = g_spdif_mode = g_spdif_media = 0;
    g_next_tick = 0;
    g_reverb_off = os::env("PS2_SPU2_REVERB") == "0";
    g_tick_pending = false;
    g_first_tick = g_last_tick = 0;
    const std::string chip = os::env("PS2_DUMP_AUDIO"), out = os::env("PS2_AUDIO_DUMP");
    if (!chip.empty() && g_chip_wav.open(chip)) std::fprintf(stderr, "[spu2] writing the sound chip's output to %s\n", chip.c_str());
    // the sink is set before the machine resets (host/window.cpp opens the sound device first)
    if (!out.empty() && out != chip && !g_sink && g_out_wav.open(out)) std::fprintf(stderr, "[spu2] writing the sound to %s (no sound device)\n", out.c_str());
}

std::uint16_t *ram() { return g_ram.data(); }

// The service's own window on the guest's sound registers. This is the
// service's interface, decoded into its state - not a device behind a
// boundary: there is no emulated sound chip left to call into.
std::uint32_t read(std::uint32_t a, unsigned size) {
    keep_ticking_internal();
    const std::uint32_t off = a & 0x7FEu;
    std::uint32_t value = 0;
    if (off < 0x760u) value = read_core((off & 0x400u) ? 1 : 0, off & 0x3FEu);
    else if (off < 0x7B0u) {
        std::uint16_t reg = 0;
        if (master_read(g_cores[(off - 0x760u) / 0x28u], (off - 0x760u) % 0x28u, reg)) value = reg;
    } else value = read_global(off);
    if (size == 4) value |= read(a + 2, 2) << 16;
    return value;
}

void write(std::uint32_t a, std::uint32_t v, unsigned size) {
    keep_ticking_internal();
    if (size == 4) {
        write(a, v & 0xFFFFu, 2);
        write(a + 2, v >> 16, 2);
        return;
    }
    const std::uint32_t off = a & 0x7FEu;
    v &= 0xFFFFu;
    if (off < 0x760u) write_core((off & 0x400u) ? 1 : 0, off & 0x3FEu, v);
    else if (off < 0x7B0u) {
        master_write(g_cores[(off - 0x760u) / 0x28u], (off - 0x760u) % 0x28u, static_cast<std::uint16_t>(v));
        static const bool trace = std::getenv("PS2_TRACE_SPU2") != nullptr;
        if (trace)
            std::fprintf(stderr, "[spu2] %.3f s core %u master reg 0x%02X = 0x%04X\n", g_first_tick ? static_cast<double>(kernel::now() - g_first_tick) / 36864000.0 : 0.0,
                         (off - 0x760u) / 0x28, (off - 0x760u) % 0x28, v);
    } else write_global(off, v);
}

void dma(int core, std::uint32_t madr, std::uint32_t words, bool to_spu, int channel) {
    keep_ticking_internal();
    ps2::census::spu2_dma(core, words * 4, to_spu);
    Core &k = g_cores[core];
    if (to_spu && (k.admas & (1u << core))) {
        static const bool trace_adma = std::getenv("PS2_TRACE_ADMA") != nullptr;  // porting work: when each ADMA block starts, and the blocks
        if (trace_adma) std::fprintf(stderr, "[adma] core %d dma of %u bytes from 0x%06X at %.4f s, %u frames already waiting, %zu transfers queued\n", core, words * 4, madr,
                                                        g_first_tick ? static_cast<double>(kernel::now() - g_first_tick) / 36864000.0 : 0.0, k.input_count, k.transfers.size());
        k.transfers.push_back(Transfer{madr, words * 4, channel});
        feed_input(core);
        return;
    }
    const std::uint32_t halfwords = words * 2;
    std::fprintf(stderr, "[spu2-dma] core=%d to_spu=%d madr=0x%08X words=%u tsa=0x%06X ch=%d\n",
                 core, to_spu ? 1 : 0, madr, words, k.tsa, channel);
    if (to_spu) check_irq(k.tsa, halfwords);
    for (std::uint32_t i = 0; i < halfwords; ++i) {
        std::uint16_t *spu = &g_ram[(k.tsa + i) & kRamMask];
        std::uint8_t *iop = ram_ptr(madr + i * 2);
        if (to_spu) std::memcpy(spu, iop, 2);
        else std::memcpy(iop, spu, 2);
    }
    k.tsa = (k.tsa + halfwords) & kRamMask;
    if (channel >= 0) kernel::schedule(kernel::now() + 64 + words / 4, kernel::Job{kernel::kJobDmaDone, static_cast<std::uint32_t>(channel)});
}

// The game stopped the DMA channel of the core (the start bit written as 0): the auto-DMA blocks that were still waiting to go into the
// input buffer are dropped, as the stopped channel would not have sent them. Without this the old blocks stay in the queue, play ahead of
// the blocks of the next transfer and keep the stream one block off for as long as the game streams.
void dma_stop(int core) {
    Core &k = g_cores[core & 1];
    std::fprintf(stderr, "[spu2-dma-stop] core=%d\n", core);
    static const bool trace_adma = std::getenv("PS2_TRACE_ADMA") != nullptr;
    if (trace_adma && !k.transfers.empty())
        std::fprintf(stderr, "[adma] core %d dma stopped at %.4f s, %zu transfers dropped, %u frames waiting\n", core,
                     g_first_tick ? static_cast<double>(kernel::now() - g_first_tick) / 36864000.0 : 0.0, k.transfers.size(), k.input_count);
    k.transfers.clear();
    k.admas &= ~(1u << (core & 1));
    k.input_count = k.input_read = 0;
}

std::uint32_t dma_address(int core) {
    const Core &k = g_cores[core & 1];
    if (k.transfers.empty()) return 0;
    return k.transfers.front().madr;
}

void set_sink(std::function<void(const std::int16_t *, std::size_t)> sink) { g_sink = std::move(sink); }

void dump_voices() {
    for (int c = 0; c < 2; ++c) {
        const Core &k = g_cores[c];
        std::fprintf(stderr, "[spu2] core %d master volume %04X %04X dry %04X %04X wet %04X %04X input %04X %04X attr %04X admas %04X\n", c, k.mvol[0], k.mvol[1], k.avol[0], k.avol[1],
                     k.evol[0], k.evol[1], k.bvol[0], k.bvol[1], k.attr, k.admas);
        std::fprintf(stderr, "[spu2] core %d registers from 0x180:", c);
        std::fprintf(stderr, " 180=%04X 184=%04X 188=%06X 18C=%06X 190=%06X 194=%06X 198=%04X", k.iir_alpha, k.iir_beta,
                     k.voice_dry[0], k.voice_wet[0], k.voice_dry[1], k.voice_wet[1], k.mix_control);
        std::fprintf(stderr, "\n");
    }
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 24; ++i) {
            const Voice &v = g_cores[c].voices[i];
            if (v.phase == kOff && !v.end) continue;
            const std::uint16_t header = g_ram[v.block[0] & kRamMask];
            std::fprintf(stderr, "[spu2] core %d voice %d phase %d ssa 0x%05X lsax 0x%05X block 0x%05X flags %X next 0x%05X pitch 0x%04X env %d end %d\n", c, i,
                         static_cast<int>(v.phase), v.ssa, v.lsax, v.block[0], header >> 8, v.next, v.pitch, v.env.level, v.end ? 1 : 0);
        }
}

// Called when the run ends (with PS2_DUMP_AUDIO set): closes the files the sound went into while it ran, the one path names
// included. A file left open is closed when the program exits normally, or keeps its last header update.
void dump_audio(const char *) {
    g_chip_wav.close();
    g_out_wav.close();
}

// ---------------------------------------------------------------------------
// The retail sound library's parameter interface.
// The library addressed the sound device's parameters by an index, a core and
// a voice. The service decodes each index into its own named state: the voice
// registers, the mixer's routing, the reverb's ring and volumes, the
// attribute register and the transfer registers. This table is the decoding
// map (compatibility data), not a register file: what the library wrote
// lands in named fields.
// ---------------------------------------------------------------------------

constexpr std::array<std::uint16_t, 41> kParamRegisterOffsets = {
    0x000, 0x002, 0x004, 0x006, 0x008, 0x00A, 0x00C, 0x00E,
    0x198,
    0x760, 0x762, 0x764, 0x766, 0x768, 0x76A, 0x76C, 0x76E, 0x770, 0x772,
    0x180, 0x184, 0x1A0, 0x1A4, 0x340, 0x188, 0x18C, 0x190, 0x194,
    0x2E0, 0x33C, 0x1A8, 0x19C, 0x1C0, 0x1C4, 0x1C8, 0x19A, 0x1A8,
    0x1AC, 0x1AE, 0x1B0, 0x334,
};

// The master's block lives at 0x760 with one block per core.
constexpr bool is_master_index(std::uint32_t index) { return index >= 9u && index <= 18u; }

// The address the library's index wrote, in the service's own numbering.
std::uint32_t param_address(std::uint32_t index, std::uint32_t core, std::uint32_t voice,
                            std::uint32_t voice_stride) {
    if (index >= kParamRegisterOffsets.size()) return 0xFFFFFFFFu;
    const std::uint32_t base = kParamRegisterOffsets[index];
    if (is_master_index(index)) return base + (core ? 0x28u : 0u) + voice * voice_stride;
    return base + (core ? 0x400u : 0u) + voice * voice_stride;
}

// Writes one of the library's parameters into the service's state.
void write_service(std::uint32_t address, std::uint32_t value) {
    const std::uint32_t off = address & 0x7FEu;
    if (off < 0x760u) {
        write_core((off & 0x400u) ? 1 : 0, off & 0x3FEu, value);
        return;
    }
    if (off < 0x7B0u) {
        const std::uint32_t block = (off - 0x760u) / 0x28u;
        if (block > 1u) return;  // there are two cores
        master_write(g_cores[block], (off - 0x760u) % 0x28u, static_cast<std::uint16_t>(value));
        return;
    }
    write_global(off, value);
}

std::uint32_t read_service(std::uint32_t address) {
    const std::uint32_t off = address & 0x7FEu;
    if (off < 0x760u) return read_core((off & 0x400u) ? 1 : 0, off & 0x3FEu);
    if (off < 0x7B0u) {
        const std::uint32_t block = (off - 0x760u) / 0x28u;
        if (block > 1u) return 0;
        std::uint16_t reg = 0;
        if (!master_read(g_cores[block], (off - 0x760u) % 0x28u, reg)) return 0;
        return reg;
    }
    return read_global(off);
}

bool set_param(int core, int voice, std::uint32_t index, std::uint16_t value) {
    const std::uint32_t address = param_address(index, static_cast<std::uint32_t>(core & 1),
                                                 static_cast<std::uint32_t>(voice & 0x1Fu), 16u);
    if (address == 0xFFFFFFFFu) return false;
    write_service(address, value);
    return true;
}

bool get_param(int core, int voice, std::uint32_t index, std::uint16_t &value) {
    const std::uint32_t address = param_address(index, static_cast<std::uint32_t>(core & 1),
                                                 static_cast<std::uint32_t>(voice & 0x1Fu), 16u);
    if (address == 0xFFFFFFFFu) return false;
    value = static_cast<std::uint16_t>(read_service(address) & 0xFFFFu);
    return true;
}

// The switch parameters: key on, key off, the end flags and the voice mix
// masks.
bool set_switch(int core, std::uint32_t index, std::uint32_t value) {
    Core &k = g_cores[core & 1];
    switch (index) {
    case 8: k.mix_control = static_cast<std::uint16_t>(value); return true;
    case 19: k.iir_alpha = static_cast<std::uint16_t>(value); return true;
    case 20: k.iir_beta = static_cast<std::uint16_t>(value); return true;
    case 21: key(k, value & 0xFFFFu, true); return true;
    case 22: key(k, value & 0xFFFFu, false); return true;
    case 23:
        for (int i = 0; i < 24; ++i) k.voices[i].end = false;
        return true;
    case 24: k.voice_dry[0] = value & 0xFFFFFFu; return true;
    case 25: k.voice_wet[0] = value & 0xFFFFFFu; return true;
    case 26: k.voice_dry[1] = value & 0xFFFFFFu; return true;
    case 27: k.voice_wet[1] = value & 0xFFFFFFu; return true;
    default: return false;
    }
}

bool get_switch(int core, std::uint32_t index, std::uint32_t &value) {
    const Core &k = g_cores[core & 1];
    switch (index) {
    case 8: value = k.mix_control; return true;
    case 19: value = k.iir_alpha; return true;
    case 20: value = k.iir_beta; return true;
    case 23: {
        std::uint32_t endx = 0;
        for (int i = 0; i < 24; ++i)
            if (k.voices[i].end) endx |= 1u << i;
        value = endx;
        return true;
    }
    case 24: value = k.voice_dry[0]; return true;
    case 25: value = k.voice_wet[0]; return true;
    case 26: value = k.voice_dry[1]; return true;
    case 27: value = k.voice_wet[1]; return true;
    default: return false;
    }
}

// A voice's sample address. The library passes the address in bits 1..20 and
// the "next address" flag in bit 0.
bool set_addr(int core, int voice, std::uint32_t index, std::uint32_t value) {
    if (voice < 0 || voice >= 24) return false;
    Core &k = g_cores[core & 1];
    Voice &v = k.voices[voice];
    const std::uint32_t address = ((value >> 17) & 0xFu) << 16 | ((value >> 1) & 0xFFF8u);
    switch (index) {
    case 32: v.ssa = address; return true;
    case 33: v.lsax = address; v.lsax_written = true; return true;
    case 34: v.next = address; return true;
    default: break;
    }
    const std::uint32_t offset = param_address(index, static_cast<std::uint32_t>(core & 1),
                                               static_cast<std::uint32_t>(voice & 0x1Fu), 12u);
    if (offset == 0xFFFFFFFFu) return false;
    write_service(offset, (value >> 17) & 0xFFFFu);
    if (index != 29u) write_service(offset + 2u, (value >> 1) & 0xFFF8u);
    return true;
}

bool get_addr(int core, int voice, std::uint32_t index, std::uint32_t &value) {
    if (voice < 0 || voice >= 24) return false;
    const Core &k = g_cores[core & 1];
    const Voice &v = k.voices[voice];
    switch (index) {
    case 32: value = v.ssa; return true;
    case 33: value = v.lsax; return true;
    case 34: {
        const std::uint32_t nax = (v.block[0] + 1 + (v.position >> 14)) & kRamMask;
        value = nax;
        return true;
    }
    default: break;
    }
    const std::uint32_t offset = param_address(index, static_cast<std::uint32_t>(core & 1),
                                               static_cast<std::uint32_t>(voice & 0x1Fu), 12u);
    if (offset == 0xFFFFFFFFu) return false;
    const std::uint32_t high = read_service(offset) & 0xFFFFu;
    if (index == 29u) {
        value = high << 17 | 0x1FFFFu;
    } else {
        const std::uint32_t low = read_service(offset + 2u) & 0xFFFFu;
        value = high << 17 | low << 1 | 1u;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The voice interface the native sound service uses.
// ---------------------------------------------------------------------------

void key_on(int core, std::uint32_t mask) {
    // A keyed voice needs the mixer to run. The native MSL service programs
    // the voices through this interface (there is no guest driver left to
    // touch the sound registers, which is what used to start the tick), so
    // the tick starts here: without it the voices are keyed but never
    // rendered, and the game is silent.
    keep_ticking_internal();
    const int c = core & 1;
    Core &k = g_cores[c];
    for (int i = 0; i < 24; ++i) {
        if (!(mask & (1u << i))) continue;
        ps2::census::audio_native_voice_keyed_on(c, i);
    }
    key(k, mask & 0xFFFFFFu, true);
}

void key_off(int core, std::uint32_t mask) {
    key(g_cores[core & 1], mask & 0xFFFFFFu, false);
}

bool set_voice_param(int core, int voice, std::uint32_t index, std::uint16_t value) {
    if (voice < 0 || voice >= 24) return false;
    Voice &v = g_cores[core & 1].voices[voice];
    switch (index) {
    case 0:  // the volume left
        v.voll = value;
        if (!(value & 0x8000u)) v.vol[0].level = static_cast<std::int16_t>(value & 0x7FFFu);
        return true;
    case 1:  // the volume right
        v.volr = value;
        if (!(value & 0x8000u)) v.vol[1].level = static_cast<std::int16_t>(value & 0x7FFFu);
        return true;
    case 2: v.pitch = value; return true;
    case 3: v.adsr1 = value; return true;
    case 4: v.adsr2 = value; return true;
    case 5: v.env.level = static_cast<std::int16_t>(value); return true;
    default: return false;
    }
}

bool get_voice_param(int core, int voice, std::uint32_t index, std::uint16_t &value) {
    if (voice < 0 || voice >= 24) return false;
    const Voice &v = g_cores[core & 1].voices[voice];
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
    if (voice < 0 || voice >= 24) return false;
    Voice &v = g_cores[core & 1].voices[voice];
    switch (index) {
    case 32: v.ssa = value; return true;
    case 33: v.lsax = value; v.lsax_written = true; return true;
    case 34: v.next = value; return true;
    default: return false;
    }
}

bool get_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t &value) {
    if (voice < 0 || voice >= 24) return false;
    const Voice &v = g_cores[core & 1].voices[voice];
    switch (index) {
    case 32: value = v.ssa; return true;
    case 33: value = v.lsax; return true;
    case 34: value = (v.block[0] + 1 + (v.position >> 14)) & kRamMask; return true;
    default: return false;
    }
}

// ---------------------------------------------------------------------------
// The ADMA stream.
// ---------------------------------------------------------------------------

bool has_active_stream(int core) { return g_streams[core & 1].active; }

void start_stream(int core, std::uint32_t mode, std::uint32_t iop_address, std::uint32_t bytes,
                  std::uint32_t start_address) {
    CoreStream &s = g_streams[core & 1];
    s = {};
    s.mode = mode;
    s.direction = mode & 3u;
    s.iop_base = iop_address;
    s.total_bytes = bytes;
    s.half_bytes = bytes / 2u;
    s.loop = (mode & 0x10u) != 0;
    if (s.direction == 1u) {
        // Reading from the sound memory into the IOP's memory.
        s.active_buf = 0;
        std::uint32_t offset = start_address & 0x3FFu;
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
    keep_ticking_internal();
    refill_ring(core & 1);
}

void stop_stream(int core) {
    CoreStream &s = g_streams[core & 1];
    s.active = false;
    s.ring_count = 0;
    s.ring_read = 0;
}

std::uint32_t get_active_buffer(int core) { return g_streams[core & 1].active_buf; }

std::uint32_t get_next_fill_address(int core) {
    const CoreStream &s = g_streams[core & 1];
    return s.iop_base + (s.active_buf * s.half_bytes);
}

std::uint32_t get_current_address(int core) { return g_streams[core & 1].current_iop_addr; }

// The service's window on the guest's sound registers.
std::uint32_t window_read(std::uint32_t address, unsigned size) { return read(address, size); }
void window_write(std::uint32_t address, std::uint32_t value, unsigned size) { write(address, value, size); }

std::uint16_t *sound_ram() { return g_ram.data(); }

// The service's state in a save/load stream.
void state(ps2::state::Stream &s) {
    s.tag("spu2");
    s.vec(g_ram);
    for (Core &k : g_cores) {
        s.pod(k.voices);
        if (s.loading())
            for (const Voice &v : k.voices)
                for (int i = 0; i < 2; ++i) block_end(v)[i] = (g_ram[v.block[i] & kRamMask] >> 8) & 1u;
        s.pod(k.attr);
        s.pod(k.admas);
        s.pod(k.irqa);
        s.pod(k.tsa);
        s.pod(k.mvol);
        s.pod(k.bvol);
        s.pod(k.avol);
        s.pod(k.evol);
        s.pod(k.irq);
        s.pod(k.input);
        s.pod(k.input_read);
        s.pod(k.input_count);
        s.deque(k.transfers, [&s](Transfer &t) {
            s.pod(t.madr);
            s.pod(t.bytes);
            s.pod(t.channel);
        });
        s.pod(k.mix_control);
        s.pod(k.voice_dry);
        s.pod(k.voice_wet);
        s.pod(k.iir_alpha);
        s.pod(k.iir_beta);
        s.pod(k.reverb_start);
        s.pod(k.reverb_end);
        s.pod(k.reverb_address);
        s.pod(k.reverb_volume);
    }
    s.pod(g_spdif_out);
    s.pod(g_spdif_mode);
    s.pod(g_spdif_media);
    s.pod(g_next_tick);
    s.pod(g_tick_pending);
}

void keep_ticking() {
    keep_ticking_internal();
}

}
