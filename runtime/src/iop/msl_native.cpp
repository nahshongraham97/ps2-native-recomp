#include "msl_native.hpp"

#include "audio_service.hpp"
#include "mwfilefr.hpp"
#include "sif.hpp"

#include <ps2/memory.hpp>

#include <algorithm>
#include <cctype>
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
constexpr const char *kBankPath = "cdrom0:\\SNDSPS2\\MSLASSET.MS2;1";

static const char *const kBankNames[344] = {
    "permanent", // 0
    "shell", // 1
    "shell_music", // 2
    "shell_cap", // 3
    "music_one_player_path", // 4
    "shell_jukebox", // 5
    "crypt", // 6
    "crypt_bats", // 7
    "announcr", // 8
    "fightfx1", // 9
    "music_endings", // 10
    "cap_fight_vo", // 11
    "bg_red_dragon", // 12
    "bg_subway_station", // 13
    "bg_prison", // 14
    "bg_armory", // 15
    "bg_goros_lair", // 16
    "bg_battle_arena", // 17
    "bg_bell_tower", // 18
    "bg_wasteland", // 19
    "bg_jungle_arena", // 20
    "bg_lumber_mill", // 21
    "bg_evil_tower", // 22
    "bg_fire_well", // 23
    "bg_hell", // 24
    "bg_meteor_storm", // 25
    "bg_reptiles_lair", // 26
    "bg_soul_chamber", // 27
    "bg_the_pyramid", // 28
    "bg_throne_room", // 29
    "bg_hell_foundry", // 30
    "bg_nether_belly", // 31
    "bg_sky_temple", // 32
    "bg_kollapsing_kliffs", // 33
    "bg_innerspire", // 34
    "bg_balcony", // 35
    "bg_shinnok_throneroom", // 36
    "bg_shaokahn_throneroom", // 37
    "bg_fire_arena", // 38
    "bg_forest_arena", // 39
    "bg_finalkqbattle", // 40
    "bg_scorpionslair", // 41
    "bg_outworld", // 42
    "c_jax", // 43
    "c_scorpion", // 44
    "c_subzero", // 45
    "c_baraka", // 46
    "c_ermac", // 47
    "c_hutaro", // 48
    "c_kenshi", // 49
    "c_limai", // 50
    "c_liukang", // 51
    "c_mileena", // 52
    "c_nightwolf", // 53
    "c_sindel", // 54
    "c_boraicho", // 55
    "c_ashrah", // 56
    "c_jade", // 57
    "c_kabal", // 58
    "c_darrius", // 59
    "c_dragon_king", // 60
    "c_kira", // 61
    "c_dairu", // 62
    "c_havik", // 63
    "c_kobra", // 64
    "c_raiden", // 65
    "c_shujinko", // 66
    "c_tanya", // 67
    "c_blaze", // 68
    "c_kitana", // 69
    "c_frost", // 70
    "c_cap", // 71
    "c_kai", // 72
    "c_stryker", // 73
    "c_rain", // 74
    "c_jarek", // 75
    "c_sheeva", // 76
    "c_shinnok", // 77
    "c_fujin", // 78
    "c_reiko", // 79
    "c_taven", // 80
    "c_kintaro", // 81
    "c_sareena", // 82
    "c_sektor", // 83
    "c_goro", // 84
    "c_motaro", // 85
    "c_shao_kahn", // 86
    "c_cyrax", // 87
    "c_drahmin", // 88
    "c_hsu_hao", // 89
    "c_johnnycage", // 90
    "c_kano", // 91
    "c_kunglao", // 92
    "c_mavado", // 93
    "c_mokap", // 94
    "c_moloch", // 95
    "c_nitara", // 96
    "c_quanchi", // 97
    "c_reptile", // 98
    "c_shangtsung", // 99
    "c_sonya", // 100
    "c_noob", // 101
    "c_smoke", // 102
    "c_chameleon", // 103
    "c_daegon", // 104
    "c_meat", // 105
    "c_cap_skykick", // 106
    "c_cap_iceprojectile", // 107
    "c_cap_neckbreak", // 108
    "c_cap_summonflames", // 109
    "c_cap_j_teleport", // 110
    "c_cap_boomerang", // 111
    "c_cap_spear", // 112
    "c_cap_m_roll", // 113
    "c_cap_shadowkick", // 114
    "c_cap_fireball", // 115
    "c_cap_bellybounce", // 116
    "c_cap_brkaprojectile", // 117
    "c_cap_dprojectile", // 118
    "c_cap_dthump", // 119
    "c_cap_flamefist", // 120
    "c_cap_flamekick", // 121
    "c_cap_fujdivekick", // 122
    "c_cap_fujlift", // 123
    "c_cap_fujprojectile", // 124
    "c_cap_fujspin", // 125
    "c_cap_hprojectile", // 126
    "c_cap_hvkprojectile", // 127
    "c_cap_hvkskrape", // 128
    "c_cap_hvktwist", // 129
    "c_cap_jbackslam", // 130
    "c_cap_jball", // 131
    "c_cap_kaiduckkick", // 132
    "c_cap_kaipropel", // 133
    "c_cap_kanoball", // 134
    "c_cap_kanolaser", // 135
    "c_cap_kirakiss", // 136
    "c_cap_kiraprojectile", // 137
    "c_cap_kobprojectile", // 138
    "c_cap_kobteleport", // 139
    "c_cap_limai360kick", // 140
    "c_cap_limaiscissor", // 141
    "c_cap_limaizap", // 142
    "c_cap_mprojectile", // 143
    "c_cap_nwcharge", // 144
    "c_cap_nwlightning", // 145
    "c_cap_nwprojectile", // 146
    "c_cap_rainlightning", // 147
    "c_cap_rainpropel", // 148
    "c_cap_rainteleport", // 149
    "c_cap_rainthrow", // 150
    "c_cap_rcharge", // 151
    "c_cap_rspinkick", // 152
    "c_cap_rteleport", // 153
    "c_cap_rthrow", // 154
    "c_cap_sarcharge", // 155
    "c_cap_sarkicks", // 156
    "c_cap_sarthrow", // 157
    "c_cap_scrpteleport", // 158
    "c_cap_sekflame", // 159
    "c_cap_sekmissile", // 160
    "c_cap_sekteleport", // 161
    "c_cap_shinhandshake", // 162
    "c_cap_shinprojectile", // 163
    "c_cap_shinteleport", // 164
    "c_cap_shvastompquake", // 165
    "c_cap_sindprojectile", // 166
    "c_cap_sindscream", // 167
    "c_cap_sindslide", // 168
    "c_cap_skcharge", // 169
    "c_cap_skforcefield", // 170
    "c_cap_sklaser", // 171
    "c_cap_switcharoo", // 172
    "c_cap_szpropel", // 173
    "c_cap_tdrill", // 174
    "c_cap_thighprojectile", // 175
    "c_cap_tlegslam", // 176
    "c_cap_tprojectile", // 177
    "c_cap_vomit", // 178
    "c_cap_waterjet", // 179
    "c_cap_hpowerpunch", // 180
    "c_cap_hteleport", // 181
    "fs_concrete", // 182
    "fs_marble", // 183
    "fs_dirt", // 184
    "fs_wood", // 185
    "fs_metal", // 186
    "fighter_mlna", // 187
    "fighter_sz", // 188
    "fighter_brc", // 189
    "fighter_ashrah", // 190
    "fighter_baraka", // 191
    "fighter_dairu", // 192
    "fighter_ermac", // 193
    "fighter_hotaru", // 194
    "fighter_jade", // 195
    "fighter_kabal", // 196
    "fighter_kenshi", // 197
    "fighter_kira", // 198
    "fighter_kobra", // 199
    "fighter_limei", // 200
    "fighter_lk", // 201
    "fighter_nwolf", // 202
    "fighter_raiden", // 203
    "fighter_scorp", // 204
    "fighter_shuj", // 205
    "fighter_sindel", // 206
    "fighter_tanya", // 207
    "fighter_dking", // 208
    "fighter_fujin", // 209
    "fighter_goro", // 210
    "fighter_jarek", // 211
    "fighter_kahn", // 212
    "fighter_kai", // 213
    "fighter_kintro", // 214
    "fighter_motaro", // 215
    "fighter_rain", // 216
    "fighter_reiko", // 217
    "fighter_sarena", // 218
    "fighter_sekt", // 219
    "fighter_sheeva", // 220
    "fighter_shin", // 221
    "fighter_sonya", // 222
    "fighter_stryk", // 223
    "fighter_taven", // 224
    "fighter_cage", // 225
    "fighter_kano", // 226
    "fighter_st", // 227
    "fighter_jax", // 228
    "fighter_kit", // 229
    "fighter_kl", // 230
    "fighter_rept", // 231
    "fighter_cham", // 232
    "fighter_cyrax", // 233
    "fighter_meat", // 234
    "fighter_qc", // 235
    "fighter_nit", // 236
    "fighter_drahm", // 237
    "fighter_frost", // 238
    "fighter_hsu", // 239
    "fighter_mav", // 240
    "fighter_mokap", // 241
    "fighter_moloch", // 242
    "fighter_blaze", // 243
    "fighter_havik", // 244
    "fighter_darrius", // 245
    "fighter_noob", // 246
    "fighter_smoke", // 247
    "fighter_daegon", // 248
    "motor_kombat", // 249
    "motor_kombat_2", // 250
    "music_motor_kombat", // 251
    "motor_engines", // 252
    "motor_driver_vo", // 253
    "mtr_track_ice_world", // 254
    "mtr_track_lava_world", // 255
    "mtr_track_desert_world", // 256
    "mtr_track_jungle_world", // 257
    "mtr_track_motor_town", // 258
    "kq_music", // 259
    "kq_fx_general", // 260
    "kq_vo_chapter_1", // 261
    "kq_vo_chapter_2", // 262
    "kq_vo_chapter_3", // 263
    "kq_vo_chapter_4", // 264
    "kq_vo_chapter_5", // 265
    "kq_vo_chapter_6", // 266
    "kq_vo_chapter_7", // 267
    "kq_vo_chapter_8", // 268
    "kq_fx_ch1_transition_1", // 269
    "kq_fx_ch1_transition_2", // 270
    "kq_fx_ch2_transition", // 271
    "kq_fx_ch3_transition", // 272
    "kq_fx_ch4_transition", // 273
    "kq_fx_ch5_transition", // 274
    "kq_fx_ch7_transition", // 275
    "kq_fx_ch8_transition", // 276
    "kq_nis", // 277
    "kq_fx_ch2_icepath1", // 278
    "kq_fx_ch2_icepath1c", // 279
    "kq_fx_ch2_icepath3", // 280
    "kq_fx_ch2_icepalace2", // 281
    "kq_fx_ch2_icepalace4", // 282
    "kq_fx_ch4_forest1", // 283
    "kq_fx_ch5_cave1", // 284
    "kq_fx_ch6_firemount1", // 285
    "kq_fx_ch6_lowerspire1", // 286
    "kq_fx_ch6_upperspire1", // 287
    "kq_fx_ch1_jungle1", // 288
    "kq_fx_ch1_jungle3", // 289
    "kq_fx_ch1_temple1", // 290
    "kq_fx_ch1_temple3", // 291
    "kq_fx_ch1_temple5", // 292
    "kq_fx_ch1_sektek1", // 293
    "kq_fx_ch1_sektek3", // 294
    "kq_fx_ch5_cave3", // 295
    "kq_fx_ch7_dungeon3", // 296
    "kq_fx_ch7_dungeon5", // 297
    "kq_fx_ch7_entrance_1", // 298
    "kq_fx_ch7_grandhallway6", // 299
    "kq_fx_ch7_maproom7", // 300
    "kq_fx_ch8_forbiddenpass1", // 301
    "kq_fx_obelisk", // 302
    "kq_fx_obelisk_4", // 303
    "kq_vo_hero", // 304
    "kq_vo_npc_dm1", // 305
    "kq_vo_npc_dm2", // 306
    "kq_vo_npc_f1", // 307
    "kq_vo_npc_m1", // 308
    "kq_vo_npc_m2", // 309
    "kq_vo_npc_mon1", // 310
    "kq_vo_npc_yeti_big", // 311
    "kq_vo_npc_sw", // 312
    "kq_vo_npc_sw1", // 313
    "kq_vo_npc_dmn1", // 314
    "kq_vo_npc_zom1", // 315
    "kq_vo_npc_dm3", // 316
    "kq_vo_npc_dm4", // 317
    "kq_vo_npc_dm5", // 318
    "kq_vo_npc_dmn2", // 319
    "kq_vo_npc_dr1", // 320
    "kq_vo_npc_f2", // 321
    "kq_vo_npc_f3", // 322
    "kq_vo_npc_m3", // 323
    "kq_vo_npc_m4", // 324
    "kq_vo_npc_m5", // 325
    "kq_vo_npc_sw2", // 326
    "kq_vo_npc_tek1", // 327
    "kq_vo_npc_wr1", // 328
    "kq_vo_npc_wr2", // 329
    "kq_vo_npc_wr3", // 330
    "kq_vo_npc_wr4", // 331
    "kq_vo_npc_zom2", // 332
    "kq_vo_npc_zom3", // 333
    "kq_vo_npc_zom4", // 334
    "kq_vo_npc_ba1", // 335
    "kq_vo_npc_ud1", // 336
    "kq_vo_npc_ud2", // 337
    "kq_vo_npc_ud3", // 338
    "kq_vo_npc_tek2", // 339
    "kq_vo_npc_tek3", // 340
    "kq_vo_npc_imp1", // 341
    "kq_vo_npc_imp2", // 342
    "kq_vo_npc_sks1", // 343
};

static std::int32_t find_bank_index(const char *name) {
    if (!name || !*name) return -1;
    char clean[64] = {};
    std::size_t len = 0;
    while (name[len] && len + 1 < sizeof(clean)) {
        clean[len] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[len])));
        ++len;
    }
    clean[len] = '\0';
    if (len > 4 && std::strcmp(clean + len - 4, ".msb") == 0) {
        clean[len - 4] = '\0';
    }
    for (int i = 0; i < 344; ++i) {
        if (std::strcmp(clean, kBankNames[i]) == 0) {
            return i;
        }
    }
    return -1;
}


struct SoundEntry {
    std::uint32_t id;
    std::uint32_t offset;
    std::uint32_t size;
};

struct Bank {
    bool loaded = false;
    std::uint32_t count = 0;      // the header's sound count
    std::vector<SoundEntry> sounds;
    std::map<std::uint32_t, std::size_t> by_id;
};

// One sample of the bank: the ADPCM data's place in it.
struct Sample {
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t sample_rate = 22050;
};

Bank g_bank;

struct Step {
    Sample sample;                // where the sample's ADPCM data is
    std::uint32_t ticks = 0;      // remaining ticks
    std::uint32_t length = 0;     // ticks in total
};

std::vector<Step> g_steps;
std::size_t g_step = 0;
int g_voice = 0;

static std::uint32_t s_slot_to_bank[256] = {};
static std::uint32_t s_handle_to_bank[65536] = {};
static bool s_banks_initialized = false;

static void init_bank_maps() {
    if (s_banks_initialized) return;
    std::fill(std::begin(s_slot_to_bank), std::end(s_slot_to_bank), 0xFFFFFFFFu);
    std::fill(std::begin(s_handle_to_bank), std::end(s_handle_to_bank), 0xFFFFFFFFu);
    s_banks_initialized = true;
}

static std::uint32_t resolve_bank_index(std::uint32_t bank) {
    if ((bank & 0xFFFFF000u) == 0x00791000u) {
        std::uint32_t idx = bank & 0xFFFu;
        if (idx < 344u) return idx;
    }
    init_bank_maps();
    const std::uint32_t key = bank & 0xFFFFu;
    if (s_handle_to_bank[key] != 0xFFFFFFFFu) {
        return s_handle_to_bank[key];
    }
    const std::uint32_t slot = bank & 0xFFu;
    if (s_slot_to_bank[slot] != 0xFFFFFFFFu) {
        return s_slot_to_bank[slot];
    }
    if (key < 344u) {
        return key;
    }
    return 0u; // Default to permanent (Bank 0)
}

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

bool start_voice(const Sample &sample, float volume = 1.0f) {
    if (!load_bank()) return false;
    if (sample.size <= 20u) return false;

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
    // Each voice gets its own 64 KiB slice of the 2 MiB sound RAM.
    const std::uint32_t ssa = 0x1000u + static_cast<std::uint32_t>(voice) * 0x8000u;
    const std::uint32_t words = std::min(adpcm_size / 2u, 0x8000u);
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
    audio_native::set_switch(0, 24, 0x00FFFFFFu);   // the left dry bus
    audio_native::set_switch(0, 26, 0x00FFFFFFu);   // the right dry bus
    audio_native::key_on(0, 1u << voice);
    audio_native::keep_ticking();
    return true;
}

void play_sound(std::uint32_t bank_raw, std::uint32_t sound_idx, float volume = 1.0f) {
    if (!load_bank()) return;
    init_bank_maps();
    const std::uint32_t b_idx = resolve_bank_index(bank_raw);
    if (b_idx + 1 >= g_bank.sounds.size()) {
        std::fprintf(stderr, "[msl-native] bank %u (entry %u) out of range (%zu entries)\n",
                     b_idx, b_idx + 1, g_bank.sounds.size());
        return;
    }
    const SoundEntry &bank_entry = g_bank.sounds[b_idx + 1];
    if (bank_entry.size < 44u) {
        std::fprintf(stderr, "[msl-native] bank %u size too small (%u bytes)\n", b_idx, bank_entry.size);
        return;
    }

    // Read the 44-byte bank header
    std::uint8_t b_hdr[44];
    if (!mwfilefr_read_guest_range(kBankPath, bank_entry.offset, b_hdr, sizeof(b_hdr))) {
        std::fprintf(stderr, "[msl-native] cannot read bank %u header at 0x%08X\n", b_idx, bank_entry.offset);
        return;
    }
    const std::uint32_t sound_cnt = read_le32(b_hdr + 8) & 0xFFFFu;
    const std::uint32_t snd_tbl_off = read_le32(b_hdr + 16);

    std::uint32_t chosen_idx = sound_idx;
    if (chosen_idx >= sound_cnt) {
        // Search if sound_idx matches sample_id in any entry
        bool found = false;
        for (std::uint32_t s = 0; s < sound_cnt; ++s) {
            std::uint8_t probe[36];
            if (mwfilefr_read_guest_range(kBankPath, bank_entry.offset + snd_tbl_off + s * 36u, probe, sizeof(probe))) {
                if (read_le32(probe + 8) == sound_idx) {
                    chosen_idx = s;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            std::fprintf(stderr, "[msl-native] bank %u sound %u out of range (count=%u)\n",
                         b_idx, sound_idx, sound_cnt);
            return;
        }
    }

    // Read the 36-byte sound entry: 9 words
    // [w0, w1, w2=sample_id, w3=sample_rate, w4=sample_offset, w5=sample_size, w6, w7, w8]
    const std::uint32_t entry_file_off = bank_entry.offset + snd_tbl_off + chosen_idx * 36u;
    std::uint8_t snd_entry_raw[36];
    if (!mwfilefr_read_guest_range(kBankPath, entry_file_off, snd_entry_raw, sizeof(snd_entry_raw))) {
        std::fprintf(stderr, "[msl-native] cannot read sound entry at 0x%08X\n", entry_file_off);
        return;
    }

    const std::uint32_t rate = read_le32(snd_entry_raw + 12);
    const std::uint32_t sample_offset = read_le32(snd_entry_raw + 16);
    const std::uint32_t sample_size = read_le32(snd_entry_raw + 20);

    if (sample_size <= 20u) {
        std::fprintf(stderr, "[msl-native] sound sample size too small (%u bytes)\n", sample_size);
        return;
    }

    Sample sample{sample_offset, sample_size, rate};
    start_voice(sample, volume);
    std::fprintf(stderr, "[msl-native] play sound: bank=%u ('%s') sound=%u sample_off=0x%08X size=%u rate=%u vol=%.2f\n",
                 b_idx, (b_idx < 344 ? kBankNames[b_idx] : "unknown"), chosen_idx,
                 sample_offset, sample_size, rate, volume);
}

void advance(std::size_t frames) {
    // Reserved for streaming / sequenced playback
}

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

        if (op == 0x3C) { // 60: load sound bank (e.g. permanent.msb, shell.msb)
            char bank_name[64] = {};
            if (cmd_len > 6) {
                std::memcpy(bank_name, cmd + (i + 6) * 4u, std::min<std::size_t>(sizeof(bank_name) - 1, (cmd_len - 6) * 4u));
            }
            std::int32_t b_idx = find_bank_index(bank_name);
            std::fprintf(stderr, "[msl-native] opcode 0x3C (load bank): tag=0x%08X name=%s -> bank_idx=%d\n",
                         tag, bank_name, b_idx);
            load_bank();
            if (tag != 0) {
                init_bank_maps();
                const std::uint32_t slot = tag & 0xFFu;
                const std::uint32_t handle = (b_idx >= 0)
                    ? (0x00791000u | static_cast<std::uint32_t>(b_idx))
                    : (0x0079C000u | (tag & 0xFFFFu));
                if (b_idx >= 0) {
                    s_slot_to_bank[slot] = static_cast<std::uint32_t>(b_idx);
                    s_handle_to_bank[handle & 0xFFFFu] = static_cast<std::uint32_t>(b_idx);
                }
                ReplyEvent ev;
                ev.tag = tag;
                ev.payload.push_back(0); // status 0 = OK (read by LAB_0012B380 / FUN_003151D0)
                ev.payload.push_back(handle); // bank handle (non-zero!)
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
                        init_bank_maps();
                        const std::uint32_t cb = read_le32(banks + b_idx * 24 + 8);
                        ReplyEvent ev;
                        ev.tag = tag;
                        const std::uint32_t handle = 0x0079C000u | b_idx;
                        if (cb == 0x003151D0u) {
                            // Sound bank loader callback (FUN_003151d0)
                            ev.payload.push_back(0); // status 0 = OK
                            ev.payload.push_back(handle); // bank handle
                            s_slot_to_bank[b_idx & 0xFFu] = b_idx;
                            s_handle_to_bank[handle & 0xFFFFu] = b_idx;
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
            play_sound(bank, sound_id, vol);
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
    std::fill(std::begin(s_slot_to_bank), std::end(s_slot_to_bank), 0xFFFFFFFFu);
    std::fill(std::begin(s_handle_to_bank), std::end(s_handle_to_bank), 0xFFFFFFFFu);
    s_banks_initialized = true;
}

void install() {
    sif::register_hle_server(kSid, msl_rpc);
    audio_native::set_frame_hook([](std::size_t frames) { msl_native::tick(frames); });
    audio_native::keep_ticking();
    std::fprintf(stderr, "[msl-native] native sound service owns SIF sid 0x%08X (RPC %u)\n", kSid,
                 kRpcSound);
}

} // namespace ps2::iop::msl_native
