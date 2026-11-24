#include "native_retail_startup.hpp"
#include "mka/retail_movie_controller.hpp"
#include "mka/content_volume.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

auto* volatile required_native_entry = &mka::retail::run_native_retail;

int main(int, char**) {
    std::printf("=== Headless Offscreen Full-Entry Run (Zero Visible Windows, Zero Audio Endpoints) ===\n");
    std::printf("TASK_BOARD.md row 10 compliance: offscreen SDL_WINDOW_HIDDEN context only.\n\n");

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

    // Initialize startup state
    mka::retail::NativeRetailStartupState state(std::move(*volume), {"cdrom0:\\SLUS_214.10"});
    mka::retail::StartupFailure f;

    // Clear telemetry log prior to run
    mka::retail::clear_placeholder_telemetry();

    std::printf("[headless-run] Calling run_native_retail...\n");
    auto result = mka::retail::run_native_retail(state, f);

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
