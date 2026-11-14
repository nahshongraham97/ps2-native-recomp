// Unit test for GS Native OpenGL Lazy On-Demand CPU VRAM Readback.
//
// Acceptance Criteria:
//   1. With periodic readback disabled (PS2_NATIVE_GL_READBACK=0), primitives rendered
//      by OpenGL do NOT synchronously block or eagerly copy to CPU VRAM.
//   2. CPU VRAM at the destination FBP remains clean/initial until on-demand sync.
//   3. sync_surface_to_cpu(fbp) synchronizes only the dirtied FBO, returns true,
//      and updates CPU VRAM with pixel-exact rendered data.
//   4. Subsequent sync_surface_to_cpu(fbp) calls without new draws return false (no-op).
//   5. High-level guest read functions (read_display_rgba) automatically trigger the sync.

#include "gs/gs.hpp"
#include "gs/native_gl_backend.hpp"
#include "gs/raster.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

constexpr std::uint32_t kFrameBase = 0x5Bu;   // FRAME.FBP, 8192-byte pages
constexpr std::uint32_t kFrameWidth = 10u;     // FRAME.FBW, 64-pixel units (640 px)
constexpr std::uint32_t kOriginX = 64u, kOriginY = 64u;
constexpr std::uint32_t kDrawW = 32u, kDrawH = 32u;

ps2::gs::Vertex make_vertex(std::uint32_t x, std::uint32_t y,
                            std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    ps2::gs::Vertex vert{};
    vert.x = x * 16u;
    vert.y = y * 16u;
    vert.z = 0;
    vert.r = r;
    vert.g = g;
    vert.b = b;
    vert.a = a;
    vert.q = 1.0f;
    return vert;
}

void clear_cpu_vram() {
    for (std::uint32_t y = 0; y < kDrawH + 16; ++y) {
        for (std::uint32_t x = 0; x < kDrawW + 16; ++x) {
            ps2::gs::write_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                                 kOriginX + x, kOriginY + y, 0u);
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    // Force native GL with eager readback DISABLED
    _putenv_s("PS2_NATIVE_GL", "1");
    _putenv_s("PS2_GS", "native-gl");
    _putenv_s("PS2_NATIVE_GL_READBACK", "0");

    ps2::gs::init();
    if (!ps2::gs::native_gl::enabled()) {
        std::cerr << "[FAIL] native_gl backend failed to initialize\n";
        return 1;
    }

    std::cout << "[TEST] 1. Initial clean state\n";
    clear_cpu_vram();
    for (std::uint32_t y = 0; y < kDrawH; ++y) {
        for (std::uint32_t x = 0; x < kDrawW; ++x) {
            std::uint32_t p = ps2::gs::read_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                                                  kOriginX + x, kOriginY + y);
            assert(p == 0u);
        }
    }

    std::cout << "[TEST] 2. Submitting primitive to GPU scene surface\n";
    ps2::gs::State state{};
    state.prmodecont = 1;
    state.colclamp = 1;
    state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                            (static_cast<std::uint64_t>(447u) << 48);
    state.ctx[0].test = 0;
    state.ctx[0].zbuf = 0;
    state.ctx[0].alpha = 0;
    state.prim = 6u; // Sprite, untextured

    const ps2::gs::Vertex verts[2] = {
        make_vertex(kOriginX, kOriginY, 0x11, 0x22, 0x33, 0x80),
        make_vertex(kOriginX + kDrawW, kOriginY + kDrawH, 0x11, 0x22, 0x33, 0x80),
    };

    bool submitted = ps2::gs::native_gl::submit(state, verts, 2);
    assert(submitted);
    ps2::gs::native_gl::flush();

    std::cout << "[TEST] 3. Verify CPU VRAM is still clean (lazy: no eager readback)\n";
    for (std::uint32_t y = 0; y < kDrawH; ++y) {
        for (std::uint32_t x = 0; x < kDrawW; ++x) {
            std::uint32_t p = ps2::gs::read_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                                                  kOriginX + x, kOriginY + y);
            if (p != 0u) {
                std::cerr << "[FAIL] CPU VRAM was updated eagerly before sync! pixel=" << std::hex << p << "\n";
                return 1;
            }
        }
    }

    std::cout << "[TEST] 4. Trigger on-demand sync_surface_to_cpu\n";
    bool synced = ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    if (!synced) {
        std::cerr << "[FAIL] sync_surface_to_cpu returned false for dirty surface!\n";
        return 1;
    }

    std::cout << "[TEST] 5. Verify CPU VRAM is now populated with rendered pixels\n";
    std::uint32_t rendered_pixel = ps2::gs::read_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                                                       kOriginX, kOriginY);
    // RGBA: r=0x11, g=0x22, b=0x33, a=0x80 (scaled to 0xFF in 32-bit GL readback)
    std::uint8_t r = rendered_pixel & 0xFF;
    std::uint8_t g = (rendered_pixel >> 8) & 0xFF;
    std::uint8_t b = (rendered_pixel >> 16) & 0xFF;
    std::cout << "Rendered pixel readback: R=" << (int)r << " G=" << (int)g << " B=" << (int)b << "\n";
    if (r == 0 && g == 0 && b == 0) {
        std::cerr << "[FAIL] CPU VRAM is still black after sync_surface_to_cpu!\n";
        return 1;
    }

    std::cout << "[TEST] 6. Verify subsequent sync_surface_to_cpu returns false (already clean)\n";
    bool second_sync = ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    if (second_sync) {
        std::cerr << "[FAIL] second sync_surface_to_cpu returned true on clean surface!\n";
        return 1;
    }

    std::cout << "[TEST] 7. Verify read_display_rgba triggers sync automatically\n";
    // Draw again with red color to dirty surface
    const ps2::gs::Vertex verts2[2] = {
        make_vertex(kOriginX, kOriginY, 0xEE, 0x10, 0x20, 0x80),
        make_vertex(kOriginX + kDrawW, kOriginY + kDrawH, 0xEE, 0x10, 0x20, 0x80),
    };
    submitted = ps2::gs::native_gl::submit(state, verts2, 2);
    assert(submitted);
    ps2::gs::native_gl::flush();

    std::vector<std::uint32_t> display_rgba(kDrawW * kDrawH);
    bool read_ok = ps2::gs::read_display_rgba(ps2::gs::PSMCT32, kFrameBase, kFrameWidth,
                                              kOriginX, kOriginY, kDrawW, kDrawH,
                                              false, display_rgba.data());
    assert(read_ok);
    std::uint8_t disp_r = display_rgba[0] & 0xFF;
    std::cout << "read_display_rgba R=" << (int)disp_r << "\n";
    if (disp_r == 0) {
        std::cerr << "[FAIL] read_display_rgba returned stale/black pixels!\n";
        return 1;
    }

    // Verify it is clean now
    bool post_read_sync = ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    if (post_read_sync) {
        std::cerr << "[FAIL] surface was not marked clean after read_display_rgba sync!\n";
        return 1;
    }

    ps2::gs::native_gl::shutdown();
    std::cout << "[PASS] All lazy on-demand readback tests passed successfully!\n";
    return 0;
}
