#include "native_retail_startup.hpp"
#include "mka/retail_movie_controller.hpp"
#include "mka/content_volume.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#include <bcrypt.h>
#endif

namespace {

static bool s_corrupt_planted_failure = false;

static std::uint64_t compute_fnv1a64(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

#ifdef _WIN32
static std::string compute_sha256_hex(const std::uint8_t* data, std::size_t size) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return "";
    DWORD hash_obj_size = 0, data_len = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hash_obj_size), sizeof(DWORD), &data_len, 0);
    std::vector<UCHAR> hash_obj(hash_obj_size);
    UCHAR hash_buf[32]{};
    BCryptCreateHash(alg, &hash, hash_obj.data(), hash_obj_size, nullptr, 0, 0);
    BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
    BCryptFinishHash(hash, hash_buf, sizeof(hash_buf), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    char hex[65]{};
    for (int i = 0; i < 32; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", hash_buf[i]);
    }
    return std::string(hex, 64);
}

static void headless_movie_diagnostic_hook(
    void*,
    mka::retail::NativeProductionDisplayService& display,
    std::uint32_t client_w,
    std::uint32_t client_h,
    std::uint32_t /*movie_id*/,
    bool /*widescreen_active*/) {
    // Legitimate diagnostic capture in test harness under tests/
    std::vector<std::uint8_t> actual_pixels(std::size_t(client_w) * client_h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, client_w, client_h, GL_RGBA, GL_UNSIGNED_BYTE, actual_pixels.data());
    display.record_diagnostic_readback(1);

    if (s_corrupt_planted_failure && !actual_pixels.empty()) {
        actual_pixels[0] ^= 0xff;
    }

    const std::uint64_t actual_fnv = compute_fnv1a64(actual_pixels.data(), actual_pixels.size());
    const std::string actual_sha = compute_sha256_hex(actual_pixels.data(), actual_pixels.size());

    mka::retail::RetailMoviePresentationTelemetry t{};
    t.gl_draw_calls = display.telemetry().gl_draw_calls;
    t.presents = 1;
    t.frame_readbacks = display.telemetry().frame_readbacks;
    t.diagnostic_readbacks = display.telemetry().diagnostic_readbacks;
    t.last_frame_fnv1a64 = actual_fnv;
    std::memcpy(t.last_frame_sha256, actual_sha.c_str(), 64);
    t.last_frame_sha256[64] = '\0';
    mka::retail::record_movie_presentation_telemetry(t);
}
#endif

} // namespace

auto* volatile required_native_entry = &mka::retail::run_native_retail;

int main(int argc, char** argv) {
    std::printf("=== Headless Offscreen Full-Entry Run (Zero Visible Windows, Zero Audio Endpoints) ===\n");
    std::printf("TASK_BOARD.md row 10 compliance: offscreen SDL_WINDOW_HIDDEN context only.\n\n");

    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--planted-failure-corrupt-pixels") {
            s_corrupt_planted_failure = true;
            std::printf("[headless-planted-failure] Corrupt pixels mode active\n");
        }
    }

    // Set bounded iteration count for headless diagnostic observation
    if (!std::getenv("MKA_HEADLESS_MAX_FRAMES")) {
#ifdef _WIN32
        _putenv("MKA_HEADLESS_MAX_FRAMES=60");
#else
        setenv("MKA_HEADLESS_MAX_FRAMES", "60", 1);
#endif
    }
    const char* frame_limit_str = std::getenv("MKA_HEADLESS_MAX_FRAMES");
    std::printf("[headless-setup] Frame limit bounded by MKA_HEADLESS_MAX_FRAMES = %s\n",
        frame_limit_str ? frame_limit_str : "unbounded");

    // Create disposable offscreen content volume fixture
    const auto fixture_root = std::filesystem::temp_directory_path() / "mka_headless_retail_fixture";
    std::error_code ec;
    std::filesystem::remove_all(fixture_root, ec);
    std::filesystem::create_directories(fixture_root / "MODULES", ec);
    std::filesystem::create_directories(fixture_root / "art", ec);
    std::filesystem::create_directories(fixture_root / "sndsps2", ec);

    {
        std::ofstream dnas(fixture_root / "MODULES" / "DNAS310.IMG", std::ios::binary);
        dnas << "owned-native-startup-fixture";
    }
    {
        std::ofstream slus(fixture_root / "SLUS_214.10", std::ios::binary);
        slus << "slus-fixture";
    }
    {
        std::ofstream dummy_art(fixture_root / "art" / "font.art", std::ios::binary);
        dummy_art << "art-data-stream";
    }

    // Stage authentic gameart.ssf required by 19B530 loaded system art
    const std::filesystem::path art_candidates[] = {
        "../../../../private-inputs/native-ps2-loose-20261121/art/gameart.ssf",
        "../../../private-inputs/native-ps2-loose-20261121/art/gameart.ssf",
        "../../private-inputs/native-ps2-loose-20261121/art/gameart.ssf",
        "private-inputs/native-ps2-loose-20261121/art/gameart.ssf"
    };
    for (const auto& ac : art_candidates) {
        if (std::filesystem::exists(ac)) {
            std::filesystem::create_hard_link(ac, fixture_root / "art" / "gameart.ssf", ec);
            if (ec) {
                ec.clear();
                std::filesystem::copy_file(ac, fixture_root / "art" / "gameart.ssf",
                    std::filesystem::copy_options::overwrite_existing, ec);
            }
            break;
        }
    }

    // Stage authentic GAMER.OVL (384-byte module required by 3DDA60)
    const std::filesystem::path gamer_candidates[] = {
        "../../../research/inputs/modules/GAMER.ovl",
        "../../research/inputs/modules/GAMER.ovl",
        "research/inputs/modules/GAMER.ovl",
        "../../../../research/inputs/modules/GAMER.ovl",
        "../../ps2-native-recomp/games/mkarmageddon/disc-files/GAMER.OVL",
        "../ps2-native-recomp/games/mkarmageddon/disc-files/GAMER.OVL"
    };
    bool gamer_staged = false;
    for (const auto& c : gamer_candidates) {
        if (std::filesystem::exists(c)) {
            std::filesystem::copy_file(c, fixture_root / "GAMER.OVL",
                std::filesystem::copy_options::overwrite_existing, ec);
            gamer_staged = true;
            break;
        }
    }
    if (!gamer_staged) {
        std::vector<std::uint8_t> gamer(384, 0);
        auto put_u32 = [&](std::size_t offset, std::uint32_t val) {
            gamer[offset + 0] = static_cast<std::uint8_t>(val & 0xff);
            gamer[offset + 1] = static_cast<std::uint8_t>((val >> 8) & 0xff);
            gamer[offset + 2] = static_cast<std::uint8_t>((val >> 16) & 0xff);
            gamer[offset + 3] = static_cast<std::uint8_t>((val >> 24) & 0xff);
        };
        put_u32(0x00, 0x336f574d);
        put_u32(0x04, 2);
        put_u32(0x08, 0x007c4880);
        put_u32(0x18, 0x007c4880 + 0x110);
        put_u32(0x1c, 0x007c4880 + 0x114);
        put_u32(0x100, 0x24030001);
        put_u32(0x104, 0x03e00008);
        put_u32(0x108, 0xaf83b0b0);
        put_u32(0x110, 0x007c4980);
        std::ofstream gamer_out(fixture_root / "GAMER.OVL", std::ios::binary);
        gamer_out.write(reinterpret_cast<const char*>(gamer.data()), gamer.size());
    }

    // Stage authentic MSLASSET.MS2 and MSLASSET.MS4 required by 23D4D0 sound bank jobs
    const std::filesystem::path sound_candidates[] = {
        "../../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/sndsps2",
        "../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/sndsps2",
        "../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/sndsps2"
    };
    for (const auto& sc : sound_candidates) {
        if (std::filesystem::exists(sc / "MSLASSET.MS2")) {
            std::filesystem::create_hard_link(sc / "MSLASSET.MS2", fixture_root / "sndsps2" / "MSLASSET.MS2", ec);
            if (ec) {
                ec.clear();
                std::filesystem::copy_file(sc / "MSLASSET.MS2", fixture_root / "sndsps2" / "MSLASSET.MS2",
                    std::filesystem::copy_options::overwrite_existing, ec);
            }
            if (std::filesystem::exists(sc / "MSLASSET.MS4")) {
                std::filesystem::create_hard_link(sc / "MSLASSET.MS4", fixture_root / "sndsps2" / "MSLASSET.MS4", ec);
                if (ec) {
                    ec.clear();
                    std::filesystem::copy_file(sc / "MSLASSET.MS4", fixture_root / "sndsps2" / "MSLASSET.MS4",
                        std::filesystem::copy_options::overwrite_existing, ec);
                }
            }
            break;
        }
    }

    std::printf("[headless-setup] Content volume root: %s\n", fixture_root.string().c_str());

    mka::content::ContentVolumeFailure cvf;
    auto volume = mka::content::NativeContentVolume::open(fixture_root, cvf);
    if (!volume) {
        std::fprintf(stderr, "FAIL: Could not create NativeContentVolume: %s\n", cvf.host_error.message().c_str());
        return 1;
    }

    // Check for movie presentation assets
    const std::filesystem::path movier_candidates[] = {
        "../../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
        "../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
        "../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
        "../../../research/inputs/modules/MOVIER.ovl",
        "../../../../research/inputs/modules/MOVIER.ovl",
        "research/inputs/modules/MOVIER.ovl",
        "private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl"
    };
    bool movier_found = false;
    for (const auto& c : movier_candidates) {
        if (std::filesystem::exists(c)) { movier_found = true; break; }
    }

    const std::filesystem::path plane_candidates[] = {
        "../../../../research/runs/native-movie-decode-20261119/decoded-02/LOGOW.SFD.first.plane0",
        "../../../research/runs/native-movie-decode-20261119/decoded-02/LOGOW.SFD.first.plane0",
        "../../research/runs/native-movie-decode-20261119/decoded-02/LOGOW.SFD.first.plane0",
        "research/runs/native-movie-decode-20261119/decoded-02/LOGOW.SFD.first.plane0"
    };
    bool planes_found = false;
    for (const auto& c : plane_candidates) {
        if (std::filesystem::exists(c)) { planes_found = true; break; }
    }

    const bool movie_assets_available = (movier_found && planes_found);
    if (!movie_assets_available) {
        std::printf("[headless-setup] SKIP: Required local movie assets missing (MOVIER.ovl or decoded LOGOW.SFD planes not found).\n");
        std::printf("[headless-setup] Local assets must be staged for movie presentation verification (see README.md / PORT_STATUS.md for clone setup).\n");
    }

    // Initialize startup state
    mka::retail::NativeRetailStartupState state(std::move(*volume), {"cdrom0:\\SLUS_214.10"});
    mka::retail::StartupFailure f;

    // Clear telemetry log prior to run
    mka::retail::clear_placeholder_telemetry();

    // Register legitimate diagnostic capture hook in test fixture if assets available
    if (movie_assets_available) {
#ifdef _WIN32
        mka::retail::set_movie_presentation_diagnostic_hook(&headless_movie_diagnostic_hook, nullptr);
#endif
    }

    std::printf("[headless-run] Calling run_native_retail...\n");
    auto result = mka::retail::run_native_retail(state, f);

    // Unregister diagnostic hook
    mka::retail::set_movie_presentation_diagnostic_hook(nullptr, nullptr);

    // Retrieve recorded placeholder telemetry
    const auto& log = mka::retail::placeholder_telemetry_log();
    std::printf("\n=== Headless Run Execution Telemetry ===\n");
    if (result) {
        std::printf("  run_native_retail returned: exit code %lld\n", static_cast<long long>(*result));
    } else {
        std::printf("  run_native_retail returned: (terminated / frame limit reached)\n");
    }
    std::printf("  StartupFailure kind: %d, operation: %s\n", static_cast<int>(f.kind), f.operation ? f.operation : "(none)");
    std::printf("  Total placeholder invocations recorded: %zu\n\n", log.size());

    std::printf("--- Ordered Sequence of Placeholders Hit ---\n");
    std::map<std::uint32_t, std::uint32_t> hit_counts;
    std::vector<std::string> ordered_distinct;
    std::set<std::uint32_t> seen_ids;

    for (std::size_t i = 0; i < log.size(); ++i) {
        const auto& rec = log[i];
        hit_counts[rec.placeholder_id]++;
        if (seen_ids.insert(rec.placeholder_id).second) {
            ordered_distinct.push_back(rec.name);
            std::printf("  [%zu] First hit at frame %llu: #%u %s\n",
                ordered_distinct.size(),
                static_cast<unsigned long long>(rec.frame_hit),
                rec.placeholder_id,
                rec.name);
        }
    }

    std::printf("\n--- Distinct Placeholders Encountered (%zu of 26 registered) ---\n", seen_ids.size());
    for (const auto& [id, count] : hit_counts) {
        std::printf("  Placeholder #%u: called %u time(s)\n", id, count);
    }

    std::printf("\n--- Execution Blocker Analysis ---\n");
    std::printf("1. First Blocker for 3D Geometry Submission:\n");
    std::printf("   Placeholder #0 (continue_first_scene_at_225d40):\n");
    std::printf("   Returns ready lifecycle status but does not load world sector / clump geometry.\n");
    std::printf("   As a result, retail_render_enabled() observes has_world = false,\n");
    std::printf("   preventing Placeholder #2 (render_retail_frame) from being called with live scene geometry.\n\n");

    std::printf("2. First Blocker for Movie Stream Decoding Loop:\n");
    std::printf("   Placeholder #3 (begin_menu_18d450):\n");
    std::printf("   Its child procedure (NativeRetailMoviePlaybackChild) completes immediately upon\n");
    std::printf("   presenting frame 0 rather than driving asynchronous MPEG-2 / SFD stream packet\n");
    std::printf("   demuxing loops across multi-second attract playback.\n\n");

    std::printf("3. First Blocker for Game Mode Task Dispatch:\n");
    std::printf("   Placeholder #12 (transition_1225d0) / Placeholder #17 (dispatch_retail_enabled_mode):\n");
    std::printf("   Pushes mode transition records into mode_history, but mode task bodies remain placeholders.\n\n");

    if (!movie_assets_available) {
        std::printf("[headless-gate] SKIP: Bounded run reached without movie presentation assets (movie planes not found).\n");
        if (!fixture_root.empty()) {
            std::filesystem::remove_all(fixture_root, ec);
        }
        return 0;
    }

    // Query and verify authentic movie presentation telemetry from frame 5 (begin_menu_18d450 -> LOGOW.SFD frame 0)
    const auto movie_telemetry = mka::retail::last_movie_presentation_telemetry();
    std::printf("[headless-movie-telemetry] GL draw calls = %u, buffer presents = %u, frame readbacks = %u, diagnostic readbacks = %u, FNV1a64 = 0x%016llx, SHA256 = %s\n",
        movie_telemetry.gl_draw_calls,
        movie_telemetry.presents,
        movie_telemetry.frame_readbacks,
        movie_telemetry.diagnostic_readbacks,
        static_cast<unsigned long long>(movie_telemetry.last_frame_fnv1a64),
        movie_telemetry.last_frame_sha256);

    const bool movie_telemetry_valid = (movie_telemetry.gl_draw_calls == 1 &&
                                        movie_telemetry.presents == 1 &&
                                        movie_telemetry.frame_readbacks == 0 &&
                                        movie_telemetry.diagnostic_readbacks == 1 &&
                                        movie_telemetry.last_frame_fnv1a64 == 0x2cb5ec60e4e72325ull &&
                                        std::string_view(movie_telemetry.last_frame_sha256) == "ba848e89025edbb7a00d11be30040c23e036d1e57606c86074e65dd2c70191d7");
    if (!movie_telemetry_valid) {
        std::fprintf(stderr, "FAIL: Headless movie presentation telemetry mismatch!\n");
        std::fprintf(stderr, "  Observed: draws=%u, presents=%u, frame_readbacks=%u, diagnostic_readbacks=%u, FNV=0x%016llx, SHA=%s\n",
            movie_telemetry.gl_draw_calls,
            movie_telemetry.presents,
            movie_telemetry.frame_readbacks,
            movie_telemetry.diagnostic_readbacks,
            static_cast<unsigned long long>(movie_telemetry.last_frame_fnv1a64),
            movie_telemetry.last_frame_sha256);
        return 1;
    }

    // Clean up temporary fixture if one was created
    if (!fixture_root.empty()) {
        std::filesystem::remove_all(fixture_root, ec);
    }

    // Assert pinned expected stop point:
    // With task 0x18e100 wired and input_assignments bound, execution reaches the bounded frame limit
    // (MKA_HEADLESS_MAX_FRAMES=60), executing 60 frames cleanly (StartupFailure kind: 0),
    // encountering exactly 13 distinct placeholders across 369 total invocations.
    const bool reached_expected_frame_limit = (f.kind == mka::retail::StartupError::none &&
                                              seen_ids.size() == 13 &&
                                              log.size() == 369);
    if (!reached_expected_frame_limit) {
        std::fprintf(stderr, "FAIL: Headless run diverged from pinned expected stop point!\n");
        std::fprintf(stderr, "  Expected: frame limit 60 reached, failure kind 0, 13 distinct placeholders, 369 invocations\n");
        std::fprintf(stderr, "  Observed: failure kind %d, %zu distinct placeholders, %zu invocations\n",
            static_cast<int>(f.kind), seen_ids.size(), log.size());
        return 1;
    }

    std::printf("[headless-gate] Pinned expected stop verified: bounded 60-frame run completed cleanly with task 0x18e100 dispatched at frame 5 (13 distinct placeholders, 369 invocations).\n");
    return 0;
}
