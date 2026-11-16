#include "msl_native.hpp"

#include "audio_native.hpp"
#include "mwfilefr.hpp"
#include "sif.hpp"

#include <ps2/memory.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ps2::iop::msl_native {

using kernel::mwfilefr_read_guest_range;

namespace {

constexpr std::uint32_t kSid = 0x00012345u;
constexpr std::uint32_t kRpcSound = 56026u;
// The staging buffer the retail server advertised to the game (its reply to
// "op3 id 0x100FC" was "0x0004C9E4"). The game never writes it - the
// instrumented run proved it - but the reply keeps the handshake identical.
constexpr std::uint32_t kStagingAddress = 0x0004C9E4u;
constexpr const char *kBankPath = "cdrom0:\\SNDSPS2\\MSLASSET.MS2";

constexpr std::uint32_t kOpSetParam = 0x03;
constexpr std::uint32_t kOpExecute = 0x04;
constexpr std::uint32_t kOpStage = 0x06;
constexpr std::uint32_t kOpLoadAlt = 0x09;
constexpr std::uint32_t kOpLoad = 0x0A;
constexpr std::uint32_t kOpFlagB = 0x0B;
constexpr std::uint32_t kOpFlagD = 0x0D;
constexpr std::uint32_t kOpFlag11 = 0x11;
constexpr std::uint32_t kOpFlagC = 0x0C;
constexpr std::uint32_t kOpQuery = 0x4B;

constexpr std::uint32_t kParamSoundId = 0x000100FCu;
constexpr std::uint32_t kQueryParam = 0x000100FFu;
// The value the retail server answered for the "0x100FF" query.
constexpr std::uint32_t kQueryValue = 0x00004000u;

struct SoundEntry {
    std::uint32_t id;
    std::uint32_t offset;
    std::uint32_t size;
};

struct Bank {
    bool loaded = false;
    std::uint32_t count = 0;      // the header's sound count
    std::vector<SoundEntry> sounds;
};

Bank g_bank;

// The sequencer: a sound is a list of sub-samples (the data at the entry's
// offset is [count][count * sub-index]); they play one after another. Each
// step lasts as long as its sample would take to play.
struct Step {
    std::uint32_t sound = 0;      // the sub-sample's table index
    std::uint32_t ticks = 0;      // remaining ticks
    std::uint32_t length = 0;     // ticks in total
};

std::vector<Step> g_steps;
std::size_t g_step = 0;
int g_voice = 0;

std::uint32_t read_le32(const std::uint8_t *p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

bool load_bank() {
    if (g_bank.loaded) return true;
    std::uint8_t header[12];
    if (!mwfilefr_read_guest_range(kBankPath, 0, header, sizeof header)) {
        std::fprintf(stderr, "[msl-native] cannot read the sound bank header\n");
        return false;
    }
    const std::uint32_t count = read_le32(header + 4);
    const std::uint32_t table_bytes = read_le32(header + 8);
    const std::uint32_t entries = table_bytes / 12u;
    std::vector<std::uint8_t> table(table_bytes);
    if (!mwfilefr_read_guest_range(kBankPath, 12, table.data(), table_bytes)) {
        std::fprintf(stderr, "[msl-native] cannot read the sound bank table\n");
        return false;
    }
    g_bank.sounds.reserve(entries);
    for (std::uint32_t i = 0; i < entries; ++i) {
        const std::uint8_t *e = table.data() + i * 12u;
        g_bank.sounds.push_back(SoundEntry{read_le32(e), read_le32(e + 4), read_le32(e + 8)});
    }
    g_bank.count = count;
    g_bank.loaded = true;
    std::fprintf(stderr, "[msl-native] sound bank loaded: %u sounds, %u table entries, %u bytes\n",
                 count, entries, table_bytes);
    return true;
}

// One sample's length in sequencer ticks. The samples are SPU2 ADPCM blocks:
// 16 bytes per block, 28 samples per block, 48 kHz, 60 ticks per second.
std::uint32_t sample_ticks(std::uint32_t bytes) {
    const std::uint32_t blocks = (bytes + 15u) / 16u;
    const std::uint32_t samples = blocks * 28u;
    return (samples * 60u) / 48000u + 1u;
}

// Starts one sub-sample in a voice: the sample bytes are copied into the sound
// RAM at the voice's start address and the voice is keyed on.
bool start_voice(std::uint32_t sound_index) {
    if (!load_bank()) return false;
    if (sound_index >= g_bank.sounds.size()) return false;
    const SoundEntry &e = g_bank.sounds[sound_index];
    if (!e.size) return false;
    std::vector<std::uint8_t> data(e.size);
    if (!mwfilefr_read_guest_range(kBankPath, e.offset, data.data(), e.size)) {
        std::fprintf(stderr, "[msl-native] cannot read sample %u (offset 0x%08X, %u bytes)\n",
                     sound_index, e.offset, e.size);
        return false;
    }
    std::uint16_t *ram = audio_native::sound_ram();
    if (!ram) return false;
    const int voice = g_voice;
    g_voice = (g_voice + 1) % 24;
    // Each voice gets its own 16 KiB slice of the 2 MiB sound RAM.
    const std::uint32_t ssa = 0x1000u + static_cast<std::uint32_t>(voice) * 0x4000u;
    const std::uint32_t words = e.size / 2u;
    if (ssa + words > (1u << 20)) return false;
    std::memcpy(ram + ssa, data.data(), words * 2u);
    audio_native::set_voice_addr(0, voice, 32, ssa);
    audio_native::set_voice_param(0, voice, 0, 0x7FFFu);   // VOLL
    audio_native::set_voice_param(0, voice, 1, 0x7FFFu);   // VOLR
    audio_native::set_voice_param(0, voice, 2, 0x1000u);   // PITCH
    audio_native::key_on(0, 1u << voice);
    return true;
}

// Queues a sound: the entry's data is [count][count * sub-index].
void play_sound(std::uint32_t sound) {
    if (!load_bank()) return;
    if (sound >= g_bank.sounds.size()) {
        std::fprintf(stderr, "[msl-native] play %u is outside the bank (%u entries)\n", sound,
                     static_cast<unsigned>(g_bank.sounds.size()));
        return;
    }
    const SoundEntry &e = g_bank.sounds[sound];
    if (!e.size) return;
    std::vector<std::uint8_t> head(e.size);
    if (!mwfilefr_read_guest_range(kBankPath, e.offset, head.data(), e.size)) return;
    const std::uint32_t count = read_le32(head.data());
    if (!count || count > 4096u || count * 4u > e.size) {
        std::fprintf(stderr, "[msl-native] sound %u has an unexpected header (%u entries, %u bytes)\n",
                     sound, count, e.size);
        return;
    }
    g_steps.clear();
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t sub = read_le32(head.data() + 4u + i * 4u);
        Step step;
        step.sound = sub;
        step.length = (sub < g_bank.sounds.size()) ? sample_ticks(g_bank.sounds[sub].size) : 1u;
        step.ticks = step.length;
        g_steps.push_back(step);
    }
    g_step = 0;
    std::fprintf(stderr, "[msl-native] play sound %u: %u sub-samples\n", sound, count);
}

// The sequencer's tick: one per rendered audio frame.
void advance(std::size_t frames) {
    while (frames-- > 0 && !g_steps.empty()) {
        Step &step = g_steps[g_step];
        if (step.ticks > 0) {
            --step.ticks;
            if (step.ticks > 0) continue;
        }
        start_voice(step.sound);
        ++g_step;
        if (g_step >= g_steps.size()) g_steps.clear();
    }
}

// The command list: the retail server walked it and answered with four words.
bool msl_rpc(std::uint32_t method, std::uint32_t args, std::uint32_t args_size, std::uint32_t ret,
              std::uint32_t ret_size) {
    if (method != kRpcSound) return false;
    const std::uint8_t *cmd = ps2::fast<std::uint8_t>(args & 0x1FFFFFFFu);
    const std::uint32_t words = args_size / 4u;
    std::uint32_t reply[4] = {0, 0, 0, 0};
    std::uint32_t i = 0;
    while (i < words) {
        const std::uint32_t op = read_le32(cmd + i * 4u);
        switch (op) {
        case kOpSetParam: {
            if (i + 3 > words) return true;
            const std::uint32_t id = read_le32(cmd + (i + 1) * 4u);
            const std::uint32_t value = read_le32(cmd + (i + 2) * 4u);
            if (id == 0) {
                // The play request: the sound number is the value.
                play_sound(value);
                reply[0] = kOpSetParam; reply[1] = id; reply[2] = value; reply[3] = 0;
            } else if (id == kParamSoundId) {
                // The handshake: the retail server answered with its staging buffer.
                reply[0] = kOpSetParam; reply[1] = id; reply[2] = kStagingAddress; reply[3] = 0;
            } else {
                reply[0] = kOpSetParam; reply[1] = id; reply[2] = value; reply[3] = 0;
            }
            i += 3;
            break;
        }
        case kOpExecute:
        case kOpLoad:
        case kOpLoadAlt: {
            if (i + 6 > words) return true;
            const std::uint32_t id = read_le32(cmd + (i + 1) * 4u);
            if (op == kOpLoad || op == kOpLoadAlt) load_bank();
            reply[0] = kOpExecute; reply[1] = id; reply[2] = 2; reply[3] = 1;
            i += 6;
            break;
        }
        case kOpStage: {
            if (i + 6 > words) return true;
            i += 6;
            break;
        }
        case kOpFlagB:
        case kOpFlagD:
        case kOpFlag11: {
            if (i + 3 > words) return true;
            i += 3;
            break;
        }
        case kOpFlagC: {
            if (i + 2 > words) return true;
            i += 2;
            break;
        }
        case kOpQuery: {
            if (i + 3 > words) return true;
            const std::uint32_t id = read_le32(cmd + (i + 1) * 4u);
            reply[0] = kOpQuery; reply[1] = id;
            reply[2] = (id == kQueryParam) ? kQueryValue : 0u;
            reply[3] = 0;
            i += 3;
            break;
        }
        default: {
            std::fprintf(stderr, "[msl-native] unknown command 0x%08X at word %u\n", op, i);
            i = words;
            break;
        }
        }
    }
    if (ret && ret_size >= sizeof reply) {
        std::uint8_t *out = ps2::fast<std::uint8_t>(ret & 0x1FFFFFFFu);
        for (std::uint32_t w = 0; w < 4; ++w) {
            out[w * 4 + 0] = static_cast<std::uint8_t>(reply[w]);
            out[w * 4 + 1] = static_cast<std::uint8_t>(reply[w] >> 8);
            out[w * 4 + 2] = static_cast<std::uint8_t>(reply[w] >> 16);
            out[w * 4 + 3] = static_cast<std::uint8_t>(reply[w] >> 24);
        }
    }
    return true;
}

} // namespace

void tick(std::size_t frames) { advance(frames); }

void reset() {
    g_bank = Bank{};
    g_steps.clear();
    g_step = 0;
    g_voice = 0;
}

void install() {
    sif::register_hle_server(kSid, msl_rpc);
    audio_native::set_frame_hook([](std::size_t frames) { msl_native::tick(frames); });
    std::fprintf(stderr, "[msl-native] native sound service owns SIF sid 0x%08X (RPC %u)\n", kSid,
                 kRpcSound);
}

} // namespace ps2::iop::msl_native
