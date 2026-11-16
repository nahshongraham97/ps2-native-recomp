#include "msl_native.hpp"

#include "audio_service.hpp"
#include "mwfilefr.hpp"
#include "sif.hpp"

#include <ps2/memory.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
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
    // The entry of an id: the game asks for a sound by its id, and the table's
    // positions are not the ids (from the 345th entry on they differ).
    std::map<std::uint32_t, std::size_t> by_id;
};

// One sample of the bank: the ADPCM data's place in it.
struct Sample {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

Bank g_bank;

// The sequencer: a sound is a list of sub-samples (the data at the entry's
// offset is [count][count * sub-index]); they play one after another. Each
// step lasts as long as its sample would take to play.
struct Step {
    Sample sample;                // where the sample's ADPCM data is
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
    for (std::size_t i = 0; i < g_bank.sounds.size(); ++i)
        g_bank.by_id.emplace(g_bank.sounds[i].id, i);
    g_bank.count = count;
    g_bank.loaded = true;
    std::fprintf(stderr, "[msl-native] sound bank loaded: %u sounds, %u table entries, %u bytes\n",
                 count, entries, table_bytes);
    return true;
}

// The sample records of a sound: the bank's groups list the sub-sounds and
// then the samples themselves, each with the offset and the size of its ADPCM
// data. The records are the words with the 0x20000000 flag set, four words
// each (the id, the flags, the offset and the size), and the list ends with
// the id 0.
std::vector<Sample> group_samples(std::uint32_t offset, std::uint32_t size) {
    std::vector<Sample> samples;
    if (size < 8) return samples;
    std::vector<std::uint8_t> data(size);
    if (!mwfilefr_read_guest_range(kBankPath, offset, data.data(), size)) return samples;
    const std::uint32_t count = read_le32(data.data());
    if (count == 0 || count > 4096) return samples;
    const std::uint32_t words = size / 4u;
    // The records start at the first flagged word after the sub-sound list.
    std::uint32_t at = 1u + count;
    while (at + 4u <= words && !(read_le32(data.data() + at * 4u) & 0x20000000u)) ++at;
    while (at + 4u <= words) {
        const std::uint32_t id = read_le32(data.data() + at * 4u);
        if (!id) break;
        const std::uint32_t sample_offset = read_le32(data.data() + (at + 2) * 4u);
        const std::uint32_t sample_size = read_le32(data.data() + (at + 3) * 4u);
        if (sample_size && sample_size < (1u << 24)) samples.push_back(Sample{sample_offset, sample_size});
        at += 4u;
    }
    return samples;
}

// The samples of a sound: its own records plus the records of the sub-sounds
// it lists. A sub-sound is an entry of its own, so the walk is recursive. The
// groups may name each other in a circle, so every entry is visited once.
void collect_samples(std::uint32_t id, std::vector<Sample> &out,
                     std::set<std::uint32_t> &visited) {
    if (out.size() >= 64) return;
    if (!visited.insert(id).second) return;
    const auto found = g_bank.by_id.find(id);
    if (found == g_bank.by_id.end()) return;
    const SoundEntry &entry = g_bank.sounds[found->second];
    if (entry.size < 8) return;
    std::vector<std::uint8_t> head(std::min<std::uint32_t>(entry.size, 64u));
    if (!mwfilefr_read_guest_range(kBankPath, entry.offset, head.data(), head.size())) return;
    const std::uint32_t count = read_le32(head.data());
    if (count == 0 || count > 4096) {
        // Not a group: the entry is a sample of its own.
        out.push_back(Sample{entry.offset, entry.size});
        return;
    }
    for (const Sample &sample : group_samples(entry.offset, entry.size)) out.push_back(sample);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t sub = read_le32(head.data() + 4u + i * 4u);
        if (!sub) continue;
        collect_samples(sub & 0xFFFFu, out, visited);
    }
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
bool start_voice(const Sample &sample) {
    if (!load_bank()) return false;
    if (!sample.size) return false;
    std::vector<std::uint8_t> data(sample.size);
    if (!mwfilefr_read_guest_range(kBankPath, sample.offset, data.data(), sample.size)) {
        std::fprintf(stderr, "[msl-native] cannot read the sample at 0x%08X (%u bytes)\n",
                     sample.offset, sample.size);
        return false;
    }
    std::uint16_t *ram = audio_native::sound_ram();
    if (!ram) return false;
    const int voice = g_voice;
    g_voice = (g_voice + 1) % 24;
    // Each voice gets its own 16 KiB slice of the 2 MiB sound RAM.
    const std::uint32_t ssa = 0x1000u + static_cast<std::uint32_t>(voice) * 0x4000u;
    const std::uint32_t words = sample.size / 2u;
    if (ssa + words > (1u << 20)) return false;
    std::memcpy(ram + ssa, data.data(), words * 2u);
    audio_native::set_voice_addr(0, voice, 32, ssa);
    audio_native::set_voice_param(0, voice, 0, 0x7FFFu);   // VOLL
    audio_native::set_voice_param(0, voice, 1, 0x7FFFu);   // VOLR
    audio_native::set_voice_param(0, voice, 2, 0x1000u);   // PITCH
    // The retail server routed every voice to the dry bus when the sound
    // library was initialised. There is no guest driver left to set the
    // mixer's voice masks, so the service sets them here: without them the
    // voices are keyed and routed nowhere, and the game is silent.
    audio_native::set_switch(0, 24, 0x00FFFFFFu);   // the left dry bus
    audio_native::set_switch(0, 26, 0x00FFFFFFu);   // the right dry bus
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
    const auto found = g_bank.by_id.find(sound);
    if (found == g_bank.by_id.end()) {
        std::fprintf(stderr, "[msl-native] sound %u is not in the bank\n", sound);
        return;
    }
    std::vector<Sample> samples;
    std::set<std::uint32_t> visited;
    collect_samples(sound, samples, visited);
    if (samples.empty()) {
        std::fprintf(stderr, "[msl-native] sound %u has no samples\n", sound);
        return;
    }
    g_steps.clear();
    for (const Sample &sample : samples) {
        Step step;
        step.sample = sample;
        step.length = sample_ticks(sample.size);
        step.ticks = step.length;
        g_steps.push_back(step);
    }
    g_step = 0;
    // The sound service's mixer has to run for the sequencer to advance (the
    // sequencer is stepped by the mixer's frame hook). There is no guest
    // driver left to touch the sound registers, so the service starts the
    // mixer itself: without it the sound is queued and never played.
    audio_native::keep_ticking();
    std::fprintf(stderr, "[msl-native] play sound %u: %zu samples\n", sound, g_steps.size());
}

// The sequencer's tick: one per rendered audio frame.
void advance(std::size_t frames) {
    while (frames-- > 0 && !g_steps.empty()) {
        Step &step = g_steps[g_step];
        if (step.ticks > 0) {
            --step.ticks;
            if (step.ticks > 0) continue;
        }
        start_voice(step.sample);
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
        case kOpLoadAlt:
        case 0x00400000u: {
            // 0x00400000 is the load command with the retail's flag bit set;
            // it loads a bank the same way.
            if (i + 6 > words) return true;
            const std::uint32_t id = read_le32(cmd + (i + 1) * 4u);
            if (op == kOpLoad || op == kOpLoadAlt || op == 0x00400000u) load_bank();
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
        case 0: {
            // A zero word ends the command list: the retail server's list is
            // zero terminated, and the 13:21 run logged every zero as an
            // unknown command.
            i = words;
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
