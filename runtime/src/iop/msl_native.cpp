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
    std::uint32_t sample_rate = 22050;
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

std::uint16_t read_le16(const std::uint8_t *p) {
    return static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
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
// each (the id, the sample_rate, the offset and the size), and the list ends with
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
        const std::uint32_t rate = read_le32(data.data() + (at + 1) * 4u);
        const std::uint32_t sample_offset = read_le32(data.data() + (at + 2) * 4u);
        const std::uint32_t sample_size = read_le32(data.data() + (at + 3) * 4u);
        if (sample_size > 20u && sample_size < (1u << 24)) samples.push_back(Sample{sample_offset, sample_size, rate});
        at += 4u;
    }
    return samples;
}

// The samples of a sound: its own records from the sound bank table.
void collect_samples(std::uint32_t id, std::vector<Sample> &out,
                     std::set<std::uint32_t> &visited) {
    if (out.size() >= 64) return;
    if (!visited.insert(id).second) return;
    std::size_t idx = static_cast<std::size_t>(-1);
    const auto found = g_bank.by_id.find(id);
    if (found != g_bank.by_id.end()) {
        idx = found->second;
    } else if (id < g_bank.sounds.size()) {
        idx = id;
    } else {
        return;
    }
    const SoundEntry &entry = g_bank.sounds[idx];
    if (entry.size < 8) return;
    std::vector<std::uint8_t> head(std::min<std::uint32_t>(entry.size, 64u));
    if (!mwfilefr_read_guest_range(kBankPath, entry.offset, head.data(), head.size())) return;
    const std::uint32_t count = read_le32(head.data());
    if (count == 0 || count > 4096) {
        // Not a group: the entry is a sample of its own.
        out.push_back(Sample{entry.offset, entry.size, 22050u});
        return;
    }
    for (const Sample &sample : group_samples(entry.offset, entry.size)) out.push_back(sample);
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
// In MSLASSET.MS2, each sound sample begins with a 20-byte descriptor header:
// 5 words: [dw0: format, dw1: flags, dw2: reserved, dw3: sample_rate, dw4: adpcm_size]
// The raw Sony SPU2 ADPCM payload starts strictly at sample.offset + 20 bytes.
bool start_voice(const Sample &sample, float volume = 1.0f) {
    if (!load_bank()) return false;
    if (sample.size <= 20u) return false;

    std::fprintf(stderr, "[msl-native] start_voice offset=0x%08X size=%u vol=%.2f\n",
                 sample.offset, sample.size, volume);

    // Read the 20-byte descriptor header
    std::uint8_t head[20];
    if (!mwfilefr_read_guest_range(kBankPath, sample.offset, head, sizeof head)) {
        std::fprintf(stderr, "[msl-native] cannot read sample header at 0x%08X\n", sample.offset);
        return false;
    }
    const std::uint32_t hdr_rate = read_le32(head + 12);
    const std::uint32_t data_bytes = read_le32(head + 16);
    const std::uint32_t adpcm_size = (data_bytes && data_bytes <= sample.size - 20u)
                                     ? data_bytes : (sample.size - 20u);
    if (!adpcm_size) return false;

    // Read the actual ADPCM blocks starting strictly after the 20-byte header
    std::vector<std::uint8_t> data(adpcm_size);
    if (!mwfilefr_read_guest_range(kBankPath, sample.offset + 20u, data.data(), adpcm_size)) {
        std::fprintf(stderr, "[msl-native] cannot read sample data at 0x%08X (%u bytes)\n",
                     sample.offset + 20u, adpcm_size);
        return false;
    }

    std::uint16_t *ram = audio_native::sound_ram();
    if (!ram) return false;
    const int voice = g_voice;
    g_voice = (g_voice + 1) % 24;
    // Each voice gets its own 16 KiB slice of the 2 MiB sound RAM.
    const std::uint32_t ssa = 0x1000u + static_cast<std::uint32_t>(voice) * 0x4000u;
    const std::uint32_t words = std::min(adpcm_size / 2u, 0x4000u);
    if (ssa + words > (1u << 20)) return false;
    std::memcpy(ram + ssa, data.data(), words * 2u);

    const std::uint32_t sample_rate = hdr_rate ? hdr_rate : (sample.sample_rate ? sample.sample_rate : 22050u);
    const std::uint16_t pitch = static_cast<std::uint16_t>((sample_rate * 4096u) / 48000u);

    const std::uint16_t vol_val = static_cast<std::uint16_t>(std::clamp(static_cast<int>(0x3FFF * volume), 0, 0x3FFF));
    audio_native::set_voice_addr(0, voice, 32, ssa);
    audio_native::set_voice_param(0, voice, 0, vol_val);   // VOLL
    audio_native::set_voice_param(0, voice, 1, vol_val);   // VOLR
    audio_native::set_voice_param(0, voice, 2, pitch);     // PITCH
    audio_native::set_voice_param(0, voice, 3, 0x00FFu);   // ADSR1 (instant attack, maximum sustain)
    audio_native::set_voice_param(0, voice, 4, 0x1F20u);   // ADSR2 (steady sustain)
    audio_native::set_voice_param(0, voice, 5, 0x7FFFu);   // ENV level (full scale)
    // The retail server routed every voice to the dry bus when the sound
    // library was initialised. There is no guest driver left to set the
    // mixer's voice masks, so the service sets them here: without them the
    // voices are keyed and routed nowhere, and the game is silent.
    audio_native::set_switch(0, 24, 0x00FFFFFFu);   // the left dry bus
    audio_native::set_switch(0, 26, 0x00FFFFFFu);   // the right dry bus
    audio_native::key_on(0, 1u << voice);
    audio_native::keep_ticking();
    return true;
}

// Queues a sound and begins playback.
void play_sound(std::uint32_t sound, float volume = 1.0f) {
    if (!load_bank()) return;
    const auto found = g_bank.by_id.find(sound);
    if (found == g_bank.by_id.end() && sound >= g_bank.sounds.size()) {
        std::fprintf(stderr, "[msl-native] sound %u is not in the bank\n", sound);
        return;
    }
    std::vector<Sample> samples;
    std::set<std::uint32_t> visited;
    collect_samples(sound, samples, visited);
    audio_native::keep_ticking();
    if (samples.empty()) {
        std::fprintf(stderr, "[msl-native] sound %u has no samples\n", sound);
        return;
    }
    g_steps.clear();
    for (const Sample &sample : samples) {
        Step step;
        step.sample = sample;
        const std::uint32_t adpcm_bytes = sample.size > 20u ? sample.size - 20u : sample.size;
        step.length = sample_ticks(adpcm_bytes);
        step.ticks = step.length;
        g_steps.push_back(step);
    }
    g_step = 0;
    if (!g_steps.empty()) {
        start_voice(g_steps[0].sample, volume);
    }
    std::fprintf(stderr, "[msl-native] play sound %u: %zu sample(s) queued, first playing (vol=%.2f)\n",
                 sound, g_steps.size(), volume);
}

// The sequencer's tick: one per rendered audio frame.
void advance(std::size_t frames) {
    while (frames-- > 0 && !g_steps.empty()) {
        Step &step = g_steps[g_step];
        if (step.ticks > 0) {
            --step.ticks;
            if (step.ticks > 0) continue;
        }
        ++g_step;
        if (g_step >= g_steps.size()) {
            g_steps.clear();
            break;
        }
        start_voice(g_steps[g_step].sample);
    }
}

// The command list: the retail server walked it and answered with four words.
bool msl_rpc(std::uint32_t method, std::uint32_t args, std::uint32_t args_size, std::uint32_t ret,
              std::uint32_t ret_size) {
    if (method != kRpcSound) return false;
    const std::uint8_t *cmd = ps2::fast<std::uint8_t>(args & 0x1FFFFFFFu);
    const std::uint32_t words = args_size / 4u;
    static std::uint32_t s_packet_count = 0;
    ++s_packet_count;
    if (s_packet_count <= 5 || s_packet_count % 100 == 0) {
        std::fprintf(stderr, "[msl-rpc #%u] words=%u:", s_packet_count, words);
        for (std::uint32_t w = 0; w < std::min(words, 32u); ++w)
            std::fprintf(stderr, " %08X", read_le32(cmd + w * 4u));
        std::fprintf(stderr, "\n");
        const std::uint8_t *banks = ps2::fast<std::uint8_t>(0x0079A870u);
        if (banks) {
            for (int b = 0; b < 256; ++b) {
                const std::uint32_t w0 = read_le32(banks + b * 24);
                const std::uint16_t s0 = read_le16(banks + b * 24 + 4);
                const std::uint16_t s1 = read_le16(banks + b * 24 + 6);
                const std::uint32_t w2 = read_le32(banks + b * 24 + 8);
                if (s0 || s1 || w2)
                    std::fprintf(stderr, "[msl-bank %d] w0=%08X seq=%u s1=%u w2=%08X\n", b, w0, s0, s1, w2);
            }
        }
    }

    struct ReplyEvent {
        std::uint32_t tag = 0;
        std::vector<std::uint32_t> payload;
    };
    std::vector<ReplyEvent> reply_events;
    const std::uint8_t *banks = ps2::fast<std::uint8_t>(0x0079A870u);

    std::uint32_t i = 0;
    while (i < words) {
        const std::uint32_t cmd_len = read_le32(cmd + i * 4u);
        if (cmd_len == 0 || i + cmd_len > words) break;
        const std::uint32_t tag = read_le32(cmd + (i + 1) * 4u);
        const std::uint32_t op = (cmd_len >= 3) ? read_le32(cmd + (i + 2) * 4u) : 0;

        // Check if this command contains a bank load (opcode 0x00400000 or kOpLoad/kOpLoadAlt)
        bool has_bank_load = false;
        for (std::uint32_t w = i + 2; w < i + cmd_len; ++w) {
            const std::uint32_t word = read_le32(cmd + w * 4u);
            if (word == 0x00400000u || word == kOpLoad || word == kOpLoadAlt) {
                has_bank_load = true;
                break;
            }
        }

        if (has_bank_load) {
            load_bank();
        }

        if (op == 0x3C) { // 60: load sound bank (e.g. permanent.msb, shell.msb)
            char bank_name[64] = {};
            if (cmd_len > 6) {
                std::memcpy(bank_name, cmd + (i + 6) * 4u, std::min<std::size_t>(sizeof(bank_name) - 1, (cmd_len - 6) * 4u));
            }
            std::fprintf(stderr, "[msl-native] opcode 0x3C (load bank): tag=0x%08X name=%s\n", tag, bank_name);
            load_bank();
            if (tag != 0) {
                ReplyEvent ev;
                ev.tag = tag;
                ev.payload.push_back(0); // status 0 = OK (read by LAB_0012B380 / FUN_003151D0)
                ev.payload.push_back(0x0079C000u | (tag & 0xFFFFu)); // bank handle (non-zero!)
                reply_events.push_back(ev);
            }
        } else if (banks && tag != 0) {
            // If this tag corresponds to an active bank sequence waiting in EE memory, queue an event
            const std::uint32_t b_idx = tag & 0xFFFFu;
            const std::uint16_t b_seq = static_cast<std::uint16_t>(tag >> 16u);
            if (b_idx < 256u && b_seq > 0) {
                const std::uint16_t cur_seq = read_le16(banks + b_idx * 24 + 4);
                if (cur_seq == b_seq) {
                    bool already = false;
                    for (const auto &ev : reply_events) {
                        if (ev.tag == tag) { already = true; break; }
                    }
                    if (!already) {
                        const std::uint32_t cb = read_le32(banks + b_idx * 24 + 8);
                        ReplyEvent ev;
                        ev.tag = tag;
                        if (cb == 0x003151D0u) {
                            // Sound bank loader callback (FUN_003151d0)
                            ev.payload.push_back(0); // status 0 = OK
                            ev.payload.push_back(0x0079C000u | b_idx); // bank handle
                        } else {
                            // Initial bank callback (0x003165B0) or other
                            ev.payload.push_back(1);
                        }
                        reply_events.push_back(ev);
                        std::fprintf(stderr, "[msl-native] acknowledging bank %u event seq=%u cb=0x%08X (tag=0x%08X)\n",
                                     b_idx, b_seq, cb, tag);
                    }
                }
            }
        }

        if (op == 106 || op == 54) {
            // Opcode 106 (0x6A, from 0x00315E10) / Opcode 54 (0x36, from 0x00315C70): play sound
            const std::uint32_t bank = (cmd_len >= 4) ? read_le32(cmd + (i + 3) * 4u) : 0;
            const std::uint32_t sound_id = (cmd_len >= 5) ? read_le32(cmd + (i + 4) * 4u) : 0;
            const std::uint32_t raw_vol = (cmd_len >= 8) ? read_le32(cmd + (i + 7) * 4u) : 16384u;
            float vol = 1.0f;
            if (raw_vol > 0) vol = static_cast<float>(raw_vol) / 16384.0f;
            std::fprintf(stderr, "[msl-native] opcode %u (0x%X): play sound id=%u bank=0x%X vol=%.2f\n",
                         op, op, sound_id, bank, vol);
            play_sound(sound_id, vol);
        }

        if (op != 99 && op != 106 && op != 54 && op != 0x3C && op != 1000 && op != 100 && op != 103 && op != 1) {
            std::fprintf(stderr, "[msl-native] opcode %u (0x%X) cmd_len=%u tag=0x%08X\n", op, op, cmd_len, tag);
        }

        i += cmd_len;
    }

    std::vector<std::uint32_t> reply;
    for (const auto &ev : reply_events) {
        reply.push_back(2 + static_cast<std::uint32_t>(ev.payload.size()));
        reply.push_back(ev.tag);
        for (std::uint32_t w : ev.payload) {
            reply.push_back(w);
        }
    }
    reply.push_back(0); // terminate event list

    if (ret && ret_size >= reply.size() * sizeof(std::uint32_t)) {
        std::uint8_t *out = ps2::fast<std::uint8_t>(ret & 0x1FFFFFFFu);
        for (std::size_t w = 0; w < reply.size(); ++w) {
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
    audio_native::keep_ticking();
    std::fprintf(stderr, "[msl-native] native sound service owns SIF sid 0x%08X (RPC %u)\n", kSid,
                 kRpcSound);
}

} // namespace ps2::iop::msl_native
