// Verification target for production offscreen display service presenting
// authentic retail movie frame (LOGON.SFD) via NativeMovieGlDevice without a visible window.
#include "mka/retail_display_service.hpp"
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

    // 2. Initialize offscreen production display service (1103x601 non-4:3 host target)
    DisplayServiceFailure failure;
    const std::uint32_t host_w = 1103;
    const std::uint32_t host_h = 601;
    auto display = NativeOffscreenDisplayService::create(host_w, host_h, failure);
    check(bool(display) && failure.error == DisplayServiceError::none, "Failed to create offscreen display service");
    check(display->target_width() == host_w && display->target_height() == host_h, "Display target dimensions mismatch");

    // 3. Viewport & Aspect Ratio Handling checks
    auto vp_4_3 = display->compute_viewport(host_w, host_h, DisplayAspectMode::standard_4_3);
    check(vp_4_3.height == host_h, "4:3 in 1103x601 should fit vertically");
    check(vp_4_3.width == static_cast<std::uint32_t>(std::round(double(host_h) * 4.0 / 3.0)), "4:3 width calculation");
    check(vp_4_3.x == (host_w - vp_4_3.width) / 2, "4:3 pillarbox center x");
    check(vp_4_3.y == 0, "4:3 pillarbox center y");

    auto vp_exact = display->compute_viewport(640, 448, DisplayAspectMode::standard_4_3);
    check(vp_exact.x == 0 && vp_exact.y == 0 && vp_exact.width == 640 && vp_exact.height == 448, "640x448 exact viewport");

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

    // 6. Load Authentic Retail First Movie Frame (LOGON.SFD frame 0)
    MovieVideoFrame video{};
    video.width = 640;
    video.height = 448;
    for (unsigned i = 0; i < 3; ++i) {
        auto plane_path = decoded_dir / ("LOGON.SFD.first.plane" + std::to_string(i));
        video.planes[i] = read_bytes(plane_path);
    }
    video.source_pts = {0, 1, 90000, true};
    video.timestamp = video.source_pts;

    auto converted = convert_movie_yuv420(video, *table, color_error);
    check(bool(converted) && color_error == MovieColorError::none, "Failed to convert YUV420 to RGBA");
    const auto expected_frame = *converted;

    // 7. GPU Upload
    auto upload_result = backend->upload(*converted);
    check(upload_result.error == MovieGpuError::none && upload_result.frame != nullptr, "Movie frame GPU upload failed");

    // 8. Present Decoded Frame through Production Display Interface
    RECT client{};
    check(GetClientRect(static_cast<HWND>(display->native_display_handle()), &client) != 0, "Failed to get client rect");
    const std::uint32_t client_w = static_cast<std::uint32_t>(client.right);
    const std::uint32_t client_h = static_cast<std::uint32_t>(client.bottom);

    MovieHostLayout layout{
        display->destination_identity(),
        client_w,
        client_h,
        MovieAspectMode::explicit_display_aspect,
        640,
        448,
        MovieWindowPolicy::centered_fit
    };
    NativeMoviePresentPlan plan{
        upload_result.frame,
        {0, 0, 640, 448},
        MovieFilter::nearest,
        MovieAlpha::replace_rgba8
    };

    auto submission = backend->present(plan, layout);
    check(submission.error == MovieGpuError::none, "Movie frame present failed");
    await_fence(backend, submission.fence);
    display->record_gl_draw(1);
    check(display->swap_buffers(failure), "Failed to execute display swap_buffers");

    // 9. External Frame Capture Verification (Diagnostic readback under allowlist)
    const auto dest_rect = movie_destination(plan, layout);
    check(bool(dest_rect), "Valid movie destination rect");

    std::vector<std::uint8_t> actual_pixels(std::size_t(client_w) * client_h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, client_w, client_h, GL_RGBA, GL_UNSIGNED_BYTE, actual_pixels.data());
    check(glGetError() == GL_NO_ERROR, "glReadPixels failed during diagnostic capture");

    const std::uint64_t frame_fnv = compute_fnv1a64(actual_pixels.data(), actual_pixels.size());
    const std::string frame_sha = compute_sha256_hex(actual_pixels.data(), actual_pixels.size());
    std::printf("  [external-frame-capture] Captured %ux%u frame: FNV1a64=0x%016llx, SHA256=%s\n",
        client_w, client_h, static_cast<unsigned long long>(frame_fnv), frame_sha.c_str());

    // Verify center pixel of dragon logo in destination rect
    const unsigned center_x = dest_rect->x + dest_rect->width / 2;
    const unsigned center_y = dest_rect->y + dest_rect->height / 2;
    const std::size_t center_idx = (std::size_t(client_h - 1 - center_y) * client_w + center_x) * 4;
    std::printf("  [external-frame-capture] Sampled center RGBA: (%u, %u, %u, %u)\n",
        actual_pixels[center_idx], actual_pixels[center_idx + 1], actual_pixels[center_idx + 2], actual_pixels[center_idx + 3]);

    // 10. Clean shutdown
    backend->retire(std::move(submission.fence));
    shutdown_complete(backend);
    check(retirement_scope.owner->poll().state == MovieFenceState::complete, "all explicit native sessions retired");

    // 11. Mandatory Telemetry Counters Output
    const auto tel = display->telemetry();
    std::printf("  [diagnostic-telemetry] GL draw calls = %u, CPU composited pixels = %u, frame readbacks = %u, diagnostic readbacks = %u, fallback pixels = %u\n",
        tel.gl_draw_calls, tel.cpu_composited_pixels, tel.frame_readbacks, tel.diagnostic_readbacks, tel.fallback_pixels);

    check(tel.gl_draw_calls > 0, "GL draw calls must be greater than zero");
    check(tel.cpu_composited_pixels == 0, "CPU composited pixels must be zero");
    check(tel.frame_readbacks == 0, "Frame readbacks must be zero");
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
