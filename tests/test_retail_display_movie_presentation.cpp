// Verification target for production offscreen display service presenting
// authentic retail movie frame (LOGON.SFD) via NativeMovieGlDevice without a visible window.
#include "mka/retail_display_service.hpp"
#include "mka/retail_movie_controller.hpp"
#include "mka/movie_gpu.hpp"
#include "mka/movie_gl_device.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
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
using namespace mka::retail;
using namespace mka::video;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    check(bool(in), ("Failed to open file: " + path.string()).c_str());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::uint64_t compute_fnv1a64(const std::uint8_t* data, std::size_t size) {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

#ifdef _WIN32
std::string compute_sha256_hex(const std::uint8_t* data, std::size_t size) {
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

struct RetirementScope {
    std::shared_ptr<NativeMovieGpuRetirementOwner> owner;
    RetirementScope() {
        MovieGpuError error;
        owner = NativeMovieGpuRetirementOwner::create(error);
        check(owner && error == MovieGpuError::none, "Failed to create host retirement owner");
    }
    ~RetirementScope() {
        for (unsigned i = 0; i < 5000; ++i) {
            auto result = owner->poll();
            if (result.state == MovieFenceState::complete) return;
            if (result.state == MovieFenceState::failed) {
                result = owner->retry_failed();
                if (result.state == MovieFenceState::complete) return;
            }
            Sleep(1);
        }
        std::fputs("FAIL: Physical fixture retained GPU retirement work\n", stderr);
        std::abort();
    }
};

void shutdown_complete(const std::shared_ptr<NativeMovieGlDevice>& device) {
    for (unsigned i = 0; i < 5000; ++i) {
        const auto error = device->shutdown();
        if (error == MovieGpuError::none) return;
        check(error == MovieGpuError::pending, "Unexpected native shutdown failure");
        Sleep(1);
    }
    throw std::runtime_error("Native shutdown remained pending after timeout");
}

void await_fence(const std::shared_ptr<NativeMovieGpuBackend>& backend, const std::shared_ptr<MovieGpuFence>& fence) {
    check(bool(fence), "Submitted GPU fence is null");
    MovieFenceResult status;
    for (unsigned i = 0; i < 5000; ++i) {
        status = backend->poll(fence);
        if (status.state != MovieFenceState::pending) break;
        Sleep(1);
    }
    check(status.state == MovieFenceState::complete && status.error == MovieGpuError::none,
        "Movie GL sync fence did not complete successfully");
}
#endif
} // namespace

int main(int argc, char** argv) try {
#ifdef _WIN32
    std::printf("=== Native Retail Production Display Service & Movie Presentation ===\n");

    // 1. Locate assets
    std::filesystem::path movier_path;
    std::filesystem::path decoded_dir;

    if (argc >= 3) {
        movier_path = argv[1];
        decoded_dir = argv[2];
    } else {
        // Known repo relative paths
        const std::filesystem::path candidates_movier[] = {
            "../../../research/inputs/modules/MOVIER.ovl",
            "../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
            "research/inputs/modules/MOVIER.ovl"
        };
        for (const auto& c : candidates_movier) {
            if (std::filesystem::exists(c)) { movier_path = c; break; }
        }
        const std::filesystem::path candidates_decoded[] = {
            "../../../research/runs/native-movie-decode-20261119/decoded-02",
            "research/runs/native-movie-decode-20261119/decoded-02"
        };
        for (const auto& c : candidates_decoded) {
            if (std::filesystem::exists(c)) { decoded_dir = c; break; }
        }
    }

    check(!movier_path.empty() && std::filesystem::exists(movier_path), "MOVIER.ovl path must exist");
    check(!decoded_dir.empty() && std::filesystem::exists(decoded_dir), "Decoded planes directory must exist");

    // 2. Initialize offscreen production display service (1280x720 primary Xbox target)
    DisplayServiceFailure failure;
    const std::uint32_t host_w = 1280;
    const std::uint32_t host_h = 720;
    auto display = NativeOffscreenDisplayService::create(host_w, host_h, failure);
    check(bool(display) && failure.error == DisplayServiceError::none, "Failed to create offscreen display service");
    check(display->target_width() == host_w && display->target_height() == host_h, "Display target dimensions mismatch");
    check(display->logical_width() == 1280 && display->logical_height() == 720, "Display logical dimensions mismatch");

    // 3. Viewport & Aspect Ratio Handling checks
    // 16:9 full viewport on 1280x720
    auto vp_16_9 = display->compute_viewport(host_w, host_h, DisplayAspectMode::widescreen_16_9);
    check(vp_16_9.x == 0 && vp_16_9.y == 0 && vp_16_9.width == host_w && vp_16_9.height == host_h, "16:9 full 720p viewport");

    // 4:3 standard aspect on 1280x720 -> Pillarbox (960x720 centered, 160px borders)
    auto vp_4_3 = display->compute_viewport(host_w, host_h, DisplayAspectMode::standard_4_3);
    check(vp_4_3.height == host_h, "4:3 in 1280x720 should fit vertically");
    check(vp_4_3.width == static_cast<std::uint32_t>(std::round(double(host_h) * 4.0 / 3.0)), "4:3 width calculation");
    check(vp_4_3.x == (host_w - vp_4_3.width) / 2 && vp_4_3.x == 160, "4:3 pillarbox center x == 160");
    check(vp_4_3.y == 0, "4:3 pillarbox center y == 0");

    // Named PS2 reference mode check (640x448)
    auto ps2_display = NativeOffscreenDisplayService::create(
        640, 448, failure,
        NativeProductionDisplayService::PS2_LOGICAL_WIDTH,
        NativeProductionDisplayService::PS2_LOGICAL_HEIGHT);
    check(bool(ps2_display) && failure.error == DisplayServiceError::none, "Failed to create PS2 reference display");
    auto ps2_vp = ps2_display->compute_viewport(640, 448, DisplayAspectMode::standard_4_3);
    check(ps2_vp.x == 0 && ps2_vp.y == 0 && ps2_vp.width == 640 && ps2_vp.height == 448, "640x448 exact PS2 viewport");

    // 4. Context Ownership Lease
    check(!display->is_context_current(), "Context should not be current before acquire");
    check(display->acquire_context(failure), "Failed to acquire display service OpenGL context");
    check(display->is_context_current(), "Context must be current after acquire");

    // Vsync rejection on offscreen target
    check(!display->set_vsync(true, failure) && failure.error == DisplayServiceError::vsync_unsupported_offscreen,
        "Offscreen display service must reject physical vsync request");

    // 5. Setup Movie Device & Color Table
    RetirementScope retirement_scope;
    MovieGpuError attach_error;
    auto backend = NativeMovieGlDevice::attach_current_context(
        display->native_display_handle(), display->destination_identity(), retirement_scope.owner, attach_error);
    check(bool(backend) && attach_error == MovieGpuError::none, "Failed to attach NativeMovieGlDevice to display context");

    const auto movier_bytes = read_bytes(movier_path);
    check(movier_bytes.size() >= 0x77dd8 + 6144, "MOVIER.ovl table size check");
    MovieColorError color_error;
    const auto table = NativeMovieColorTable::decode(
        MovieColorRole::helper_default, movier_bytes.data() + 0x77dd8, 6144, color_error);
    check(bool(table) && color_error == MovieColorError::none, "Failed to decode movie color table");

    // 5b. Verify Recovered Retail Movie Table (33 entries) and Variant Selection (Xbox 0x0025d5b0 / PS2 0x00181fd0)
    const auto table_span = retail_movie_descriptor_table();
    check(table_span.size() == 33, "Retail movie descriptor table must have exactly 33 entries");
    check(std::string_view(table_span[0].norm_name) == "LOGON.SFD", "Movie 0 normal name must be LOGON.SFD");
    check(std::string_view(table_span[0].wide_name) == "LOGOW.SFD", "Movie 0 wide name must be LOGOW.SFD");
    check(std::string_view(table_span[0].pal60_norm_name) == "LOGO6N.SFD", "Movie 0 PAL60 normal name must be LOGO6N.SFD");
    check(std::string_view(table_span[0].pal60_wide_name) == "LOGO6W.SFD", "Movie 0 PAL60 wide name must be LOGO6W.SFD");
    check(table_span[8].wide_name == nullptr, "Bonus movie V_SNDOUT.SFD must not have a widescreen variant");

    // Variant selection verification
    const auto sel_wide = select_retail_movie_variant(0, /*widescreen_active=*/true);
    check(sel_wide.is_widescreen && std::string_view(sel_wide.filename) == "LOGOW.SFD",
        "Widescreen mode must select LOGOW.SFD");
    check(sel_wide.aspect_mode == DisplayAspectMode::widescreen_16_9, "Widescreen selection must set 16:9 aspect");

    const auto sel_norm = select_retail_movie_variant(0, /*widescreen_active=*/false);
    check(!sel_norm.is_widescreen && std::string_view(sel_norm.filename) == "LOGON.SFD",
        "Standard mode must select LOGON.SFD");
    check(sel_norm.aspect_mode == DisplayAspectMode::standard_4_3, "Standard selection must set 4:3 aspect");

    // Fallback verification for bonus movie without wide variant
    const auto sel_bonus = select_retail_movie_variant(8, /*widescreen_active=*/true);
    check(!sel_bonus.is_widescreen && std::string_view(sel_bonus.filename) == "V_SNDOUT.SFD",
        "Bonus movie without wide stream must fall back to 4:3");

    // Out-of-bounds clamp verification (Xbox 0x0025d5b4)
    const auto sel_clamp = select_retail_movie_variant(99, /*widescreen_active=*/true);
    check(sel_clamp.movie_id == 0 && std::string_view(sel_clamp.filename) == "LOGOW.SFD",
        "Out of range movie ID must clamp to 0");

    // 6. Load Authentic Retail First Movie Frame (LOGOW.SFD frame 0 for 720p widescreen)
    MovieVideoFrame video{};
    video.width = 640;
    video.height = 448;
    for (unsigned i = 0; i < 3; ++i) {
        auto plane_path = decoded_dir / ("LOGOW.SFD.first.plane" + std::to_string(i));
        video.planes[i] = read_bytes(plane_path);
    }
    video.source_pts = {0, 1, 90000, true};
    video.timestamp = video.source_pts;

    auto converted = convert_movie_yuv420(video, *table, color_error);
    check(bool(converted) && color_error == MovieColorError::none, "Failed to convert YUV420 to RGBA");
    const auto expected_frame = *converted;

    // 7. Verify 4:3 pillarbox destination geometry rule without extra draw call
    const auto dest_rule_4_3 = display->compute_viewport(host_w, host_h, DisplayAspectMode::standard_4_3);
    check(dest_rule_4_3.x == 160 && dest_rule_4_3.y == 0 && dest_rule_4_3.width == 960 && dest_rule_4_3.height == 720,
        "Destination rect for 4:3 stream must match pillarboxed 960x720 centered within 1280x720");

    // 8. Present Frame via Recovered Retail Movie Controller Path (PS2 0x00181f90 / Xbox 0x0025d380)
    RECT client{};
    check(GetClientRect(static_cast<HWND>(display->native_display_handle()), &client) != 0, "Failed to get client rect");
    const std::uint32_t client_w = static_cast<std::uint32_t>(client.right);
    const std::uint32_t client_h = static_cast<std::uint32_t>(client.bottom);

    auto play_result = play_retail_movie_frame(0, *display, *backend, *table, video, /*widescreen_active=*/true);
    check(play_result.success, "play_retail_movie_frame must succeed through display service");
    check(play_result.selection.is_widescreen && std::string_view(play_result.selection.filename) == "LOGOW.SFD",
        "Movie controller must select LOGOW.SFD for widescreen");

    // 9. External Frame Capture Verification (Diagnostic readback under allowlist)
    const auto dest_rect = play_result.destination_rect;
    check(dest_rect.x == 0 && dest_rect.y == 0 && dest_rect.width == 1280 && dest_rect.height == 720,
        "Destination rect must match 16:9 full screen 1280x720 edge-to-edge");

    std::vector<std::uint8_t> actual_pixels(std::size_t(client_w) * client_h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, client_w, client_h, GL_RGBA, GL_UNSIGNED_BYTE, actual_pixels.data());
    check(glGetError() == GL_NO_ERROR, "glReadPixels failed during diagnostic capture");
    display->record_diagnostic_readback(1);

    const std::uint64_t frame_fnv = compute_fnv1a64(actual_pixels.data(), actual_pixels.size());
    const std::string frame_sha = compute_sha256_hex(actual_pixels.data(), actual_pixels.size());
    std::printf("  [external-frame-capture] Captured %ux%u frame (driver regression pin): FNV1a64=0x%016llx, SHA256=%s\n",
        client_w, client_h, static_cast<unsigned long long>(frame_fnv), frame_sha.c_str());

    // [native-design-check] Compare against independent CPU reference conversion
    // Sample points across full 16:9 widescreen movie content and compare to expected_frame.rgba
    const struct SamplePoint { unsigned x; unsigned y; } sample_points[] = {
        {640, 360}, // Center dragon logo
        {100, 100}, // Near bottom-left
        {100, 620}, // Near top-left
        {1180, 100}, // Near bottom-right
        {1180, 620}  // Near top-right
    };

    for (const auto& pt : sample_points) {
        const unsigned rx = pt.x - dest_rect.x;
        const unsigned ry = pt.y - dest_rect.y;
        const unsigned sx = (rx * 640) / dest_rect.width;
        const unsigned sy = (ry * 448) / dest_rect.height;
        const std::size_t actual_idx = (std::size_t(client_h - 1 - pt.y) * client_w + pt.x) * 4;
        const std::size_t expected_idx = (std::size_t(sy) * 640 + sx) * 4;

        for (int c = 0; c < 3; ++c) {
            int delta = std::abs(int(actual_pixels[actual_idx + c]) - int(expected_frame.rgba[expected_idx + c]));
            check(delta <= 2, "Pixel color mismatch between GPU readback and CPU reference exceeds tolerance");
        }
    }
    std::printf("  [native-design-check] Verified GPU output vs independent CPU reference conversion (max delta <= 2)\n");

    // Sample center pixel of dragon logo
    const std::size_t center_actual_idx = (std::size_t(client_h - 1 - 360) * client_w + 640) * 4;
    std::printf("  [external-frame-capture] Sampled center RGBA: (%u, %u, %u, %u)\n",
        actual_pixels[center_actual_idx], actual_pixels[center_actual_idx + 1],
        actual_pixels[center_actual_idx + 2], actual_pixels[center_actual_idx + 3]);

    // 10. Clean shutdown
    shutdown_complete(backend);
    check(retirement_scope.owner->poll().state == MovieFenceState::complete, "all explicit native sessions retired");

    // 11. Mandatory Telemetry Counters Output
    const auto tel = display->telemetry();
    std::printf("  [diagnostic-telemetry] GL draw calls = %u, CPU composited pixels = %u, frame readbacks = %u, diagnostic readbacks = %u, fallback pixels = %u\n",
        tel.gl_draw_calls, tel.cpu_composited_pixels, tel.frame_readbacks, tel.diagnostic_readbacks, tel.fallback_pixels);

    check(tel.gl_draw_calls == 1, "GL draw calls must be 1");
    check(tel.cpu_composited_pixels == 0, "CPU composited pixels must be zero");
    check(tel.frame_readbacks == 0, "Frame readbacks must be zero");
    check(tel.diagnostic_readbacks == 1, "Diagnostic readbacks must be 1");
    check(tel.fallback_pixels == 0, "Fallback pixels must be zero");
    check(tel.fallback_pixels == 0, "Fallback pixels must be zero");

    std::printf("PASS: test_retail_display_movie_presentation completed successfully.\n");
    return 0;
#else
    std::fprintf(stderr, "Offscreen display service requires Windows OpenGL\n");
    return 1;
#endif
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL test_retail_display_movie_presentation: %s\n", e.what());
    return 1;
}
