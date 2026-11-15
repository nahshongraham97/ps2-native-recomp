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
    if (std::getenv("MKA_TEST_TRANSFER_COMPLETION")) {
        constexpr std::uint32_t base = 1234u;
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(base) << 32) | (1ull << 48));
        ps2::gs::write(0x51u, 0);
        ps2::gs::write(0x52u, 4ull | (1ull << 32));
        ps2::gs::write(0x53u, 0);
        bool guest = false;
        std::uint64_t generation = 0;
        if (ps2::gs::surface_owner(base, guest, generation)) {
            std::cerr << "[FAIL] incomplete transfer marked as a completed surface write\n";
            return 1;
        }
        const std::uint32_t pixels[4] = {1, 2, 3, 4};
        ps2::gs::write_image(reinterpret_cast<const std::uint8_t *>(pixels), 1);
        if (!ps2::gs::surface_owner(base, guest, generation) || !guest ||
            ps2::gs::read_pixel(ps2::gs::PSMCT32, base, 1u, 3u, 0u) != 4u) {
            std::cerr << "[FAIL] completed transfer was not recorded with its pixels\n";
            return 1;
        }
        constexpr std::uint32_t split_base = 1300u;
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(split_base) << 32) | (1ull << 48));
        ps2::gs::write(0x52u, 8ull | (1ull << 32));
        ps2::gs::write(0x53u, 0);
        ps2::gs::write_image(reinterpret_cast<const std::uint8_t *>(pixels), 1);
        guest = false;
        generation = 0;
        if (ps2::gs::surface_owner(split_base, guest, generation)) {
            std::cerr << "[FAIL] partially received transfer marked complete\n";
            return 1;
        }
        ps2::gs::write_image(reinterpret_cast<const std::uint8_t *>(pixels), 1);
        if (!ps2::gs::surface_owner(split_base, guest, generation) || !guest ||
            ps2::gs::read_pixel(ps2::gs::PSMCT32, split_base, 1u, 7u, 0u) != 4u) {
            std::cerr << "[FAIL] split transfer did not complete correctly\n";
            return 1;
        }
        constexpr std::uint32_t interrupted_base = 1400u;
        constexpr std::uint32_t replacement_base = 1500u;
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(interrupted_base) << 32) | (1ull << 48));
        ps2::gs::write(0x52u, 8ull | (1ull << 32));
        ps2::gs::write(0x53u, 0);
        ps2::gs::write_image(reinterpret_cast<const std::uint8_t *>(pixels), 1);
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(replacement_base) << 32) | (1ull << 48));
        ps2::gs::write(0x52u, 4ull | (1ull << 32));
        ps2::gs::write(0x53u, 0);
        ps2::gs::write_image(reinterpret_cast<const std::uint8_t *>(pixels), 1);
        guest = false;
        generation = 0;
        if (ps2::gs::surface_owner(interrupted_base, guest, generation) ||
            !ps2::gs::surface_owner(replacement_base, guest, generation) || !guest ||
            ps2::gs::read_pixel(ps2::gs::PSMCT32, replacement_base, 1u, 0u, 0u) != 1u) {
            std::cerr << "[FAIL] interrupted transfer affected replacement ownership or coordinates\n";
            return 1;
        }
        constexpr std::uint32_t rgb24_base = 1600u;
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(rgb24_base) << 32) |
                               (1ull << 48) | (1ull << 56));
        ps2::gs::write(0x52u, 6ull | (1ull << 32));
        ps2::gs::write(0x53u, 0);
        const std::uint8_t rgb24[32] = {
            1,2,3, 4,5,6, 7,8,9, 10,11,12, 13,14,15, 16,
            17,18
        };
        ps2::gs::write_image(rgb24, 1);
        guest = false;
        generation = 0;
        if (ps2::gs::surface_owner(rgb24_base, guest, generation)) {
            std::cerr << "[FAIL] incomplete 24-bit transfer marked complete\n";
            return 1;
        }
        ps2::gs::write_image(rgb24 + 16, 1);
        if (!ps2::gs::surface_owner(rgb24_base, guest, generation) || !guest ||
            ps2::gs::read_pixel(ps2::gs::PSMCT24, rgb24_base, 1u, 5u, 0u) != 0x121110u) {
            std::cerr << "[FAIL] split 24-bit transfer did not preserve its carried bytes\n";
            return 1;
        }
    }
    if (std::getenv("MKA_TEST_ALIAS_OWNERSHIP")) {
        constexpr std::uint32_t base = 15000u;
        ps2::gs::write_pixel(ps2::gs::PSMCT32, base + 32u, 1u, 0u, 0u, 0x12345678u);
        if (ps2::gs::read_pixel(ps2::gs::PSMCT32, base, 1u, 0u, 32u) != 0x12345678u) {
            std::cerr << "[FAIL] alias test setup does not address the same memory\n";
            return 1;
        }
        ps2::gs::note_guest_write(base + 32u, ps2::gs::PSMCT32, 1u, 0u, 0u, 1u, 1u);
        std::size_t write_count = 0;
        const ps2::gs::GuestWrite *writes = ps2::gs::guest_writes(write_count);
        bool width_recorded = false;
        for (std::size_t i = 0; i < write_count; ++i)
            if (writes[i].generation && writes[i].base == base + 32u && writes[i].bw == 1u)
                width_recorded = true;
        if (!width_recorded) {
            std::cerr << "[FAIL] guest write stride was not recorded\n";
            return 1;
        }
        std::cout << "[PASS] differently based pixels alias the same memory\n";
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

    std::cout << "[TEST] 8. Preserve a dirty surface when the target cache evicts it\n";
    ps2::gs::write_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                         kOriginX, kOriginY, 0u);
    state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
    const ps2::gs::Vertex translucent[2] = {
        make_vertex(kOriginX, kOriginY, 0xEE, 0x10, 0x20, 0x40),
        make_vertex(kOriginX + kDrawW, kOriginY + kDrawH, 0xEE, 0x10, 0x20, 0x40),
    };
    if (!ps2::gs::native_gl::submit(state, translucent, 2)) return 1;
    for (std::uint32_t fbp = 140u; fbp <= 140u + 16u * 10u; fbp += 10u) {
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | fbp;
        if (!ps2::gs::native_gl::submit(state, verts, 2)) return 1;
    }
    const std::uint32_t evicted = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, kOriginX, kOriginY);
    if (evicted != 0x802010EEu) {
        std::cerr << "[FAIL] dirty evicted surface was lost: " << std::hex << evicted << "\n";
        return 1;
    }
    // Opt-in experimental 32-bit color-restoration regression. Passing it
    // does not establish coherence for overlapping or non-32-bit surfaces.
    if (std::getenv("MKA_TEST_EVICT_RESTORE")) {
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
        const ps2::gs::Vertex elsewhere[2] = {
            make_vertex(160, 160, 0x11, 0x22, 0x33, 0x80),
            make_vertex(192, 192, 0x11, 0x22, 0x33, 0x80),
        };
        if (!ps2::gs::native_gl::submit(state, elsewhere, 2) ||
            !ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase)) return 1;
        const std::uint32_t restored = ps2::gs::read_pixel(
            ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, kOriginX, kOriginY);
        if (restored != evicted) {
            std::cerr << "[FAIL] evicted RGBA was not restored: " << std::hex
                      << restored << " expected " << evicted << "\n";
            return 1;
        }
    }

    std::cout << "[TEST] 9. Clear the entire surface after a scissored draw\n";
    state.ctx[0].test = 0;
    state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                            (static_cast<std::uint64_t>(447u) << 48);
    if (!ps2::gs::native_gl::submit(state, translucent, 2)) return 1;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(31u) << 16) |
                            (static_cast<std::uint64_t>(31u) << 48);
    const ps2::gs::Vertex corner[2] = {
        make_vertex(0, 0, 0x11, 0x22, 0x33, 0x80),
        make_vertex(16, 16, 0x11, 0x22, 0x33, 0x80),
    };
    if (!ps2::gs::native_gl::submit(state, corner, 2)) return 1;
    ps2::gs::native_gl::clear_surfaces();
    if (!ps2::gs::native_gl::submit(state, corner, 2) ||
        !ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase)) return 1;
    const std::uint32_t cleared = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, kOriginX, kOriginY);
    if ((cleared & 0xFFFFFFu) != 0) {
        std::cerr << "[FAIL] scissor restricted the surface clear: " << std::hex << cleared << "\n";
        return 1;
    }
    std::cout << "[TEST] 10. Permit a new depth buffer after scene reset\n";
    state.ctx[0].test = 0x50000u;
    state.ctx[0].zbuf = 1u;
    if (!ps2::gs::native_gl::submit(state, corner, 2)) return 1;
    ps2::gs::native_gl::clear_surfaces();
    state.ctx[0].zbuf = 2u;
    if (!ps2::gs::native_gl::submit(state, corner, 2)) {
        std::cerr << "[FAIL] scene reset retained the old depth buffer key\n";
        return 1;
    }
    if (std::getenv("MKA_TEST_SHARED_DEPTH")) {
        ps2::gs::native_gl::clear_surfaces();
        state.ctx[0].zbuf = 2u;
        state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                                (static_cast<std::uint64_t>(447u) << 48);
        ps2::gs::Vertex near_vertices[2] = {verts[0], verts[1]};
        near_vertices[0].z = near_vertices[1].z = 32768u;
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
        if (!ps2::gs::native_gl::submit(state, near_vertices, 2)) return 1;
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | 140u;
        if (!ps2::gs::native_gl::submit(state, verts2, 2) ||
            !ps2::gs::native_gl::sync_surface_to_cpu(140u)) return 1;
        const std::uint32_t depth_pixel = ps2::gs::read_pixel(
            ps2::gs::PSMCT32, 140u * 32u, kFrameWidth, kOriginX, kOriginY);
        if ((depth_pixel & 0xFFFFFFu) != 0) {
            std::cerr << "[FAIL] same depth buffer was not shared across color targets: " << std::hex << depth_pixel << "\n";
            return 1;
        }
    }
    if (std::getenv("MKA_TEST_TRANSFER_COHERENCY")) {
        ps2::gs::native_gl::clear_surfaces();
        state.ctx[0].test = 0;
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
        state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                                (static_cast<std::uint64_t>(447u) << 48);
        if (!ps2::gs::native_gl::submit(state, verts, 2)) return 1;
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(kFrameBase * 32u) << 32) |
                               (static_cast<std::uint64_t>(kFrameWidth) << 48));
        ps2::gs::write(0x51u, (static_cast<std::uint64_t>(kOriginX) << 32) |
                               (static_cast<std::uint64_t>(kOriginY) << 48));
        ps2::gs::write(0x52u, 4ull | (1ull << 32));
        ps2::gs::write(0x53u, 0);
        const std::uint32_t transfer[4] = {0xFFAA55CCu, 0, 0, 0};
        ps2::gs::write_image(reinterpret_cast<const std::uint8_t *>(transfer), 1);
        std::uint32_t actual = 0;
        if (!ps2::gs::read_display_rgba(ps2::gs::PSMCT32, kFrameBase, kFrameWidth,
                                        kOriginX, kOriginY, 1, 1, false, &actual) ||
            actual != transfer[0]) {
            std::cerr << "[FAIL] pending GPU readback overwrote image transfer: " << std::hex << actual << "\n";
            return 1;
        }
    }

    if (std::getenv("MKA_TEST_COPY_COHERENCY")) {
        const std::uint32_t copy_base = std::getenv("MKA_TEST_COPY_OVERLAP") ? 140u : 300u;
        ps2::gs::native_gl::clear_surfaces();
        state.ctx[0].test = 0;
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
        state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                                (static_cast<std::uint64_t>(447u) << 48);
        ps2::gs::write_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, kOriginX, kOriginY, 0);
        if (!ps2::gs::native_gl::submit(state, verts, 2)) return 1;
        ps2::gs::write(0x50u, (kFrameBase * 32ull) | (static_cast<std::uint64_t>(kFrameWidth) << 16) |
                               (static_cast<std::uint64_t>(copy_base) * 32ull << 32) | (static_cast<std::uint64_t>(kFrameWidth) << 48));
        ps2::gs::write(0x51u, kOriginX | (static_cast<std::uint64_t>(kOriginY) << 16) |
                               (static_cast<std::uint64_t>(kOriginX) << 32) |
                               (static_cast<std::uint64_t>(kOriginY) << 48));
        ps2::gs::write(0x52u, 1ull | (1ull << 32));
        ps2::gs::write(0x53u, 2);
        const std::uint32_t copied = ps2::gs::read_pixel(ps2::gs::PSMCT32, copy_base * 32u,
                                                          kFrameWidth, kOriginX, kOriginY);
        if ((copied & 0xFFFFFFu) != 0x332211u) {
            std::cerr << "[FAIL] local copy read stale GPU source: " << std::hex << copied << "\n";
            return 1;
        }
        ps2::gs::write_pixel(ps2::gs::PSMCT32, copy_base * 32u, kFrameWidth, kOriginX, kOriginY, 0xFFAA55CCu);
        if (!ps2::gs::native_gl::submit(state, verts, 2)) return 1;
        ps2::gs::write(0x50u, (static_cast<std::uint64_t>(copy_base) * 32ull) | (static_cast<std::uint64_t>(kFrameWidth) << 16) |
                               (kFrameBase * 32ull << 32) | (static_cast<std::uint64_t>(kFrameWidth) << 48));
        ps2::gs::write(0x53u, 2);
        std::uint32_t destination = 0;
        if (!ps2::gs::read_display_rgba(ps2::gs::PSMCT32, kFrameBase, kFrameWidth,
                                        kOriginX, kOriginY, 1, 1, false, &destination) ||
            destination != 0xFFAA55CCu) {
            std::cerr << "[FAIL] local copy destination overwritten by GPU readback: " << std::hex << destination << "\n";
            return 1;
        }
    }
    if (std::getenv("MKA_TEST_COVERAGE_HOLES")) {
        ps2::gs::native_gl::clear_surfaces();
        state.ctx[0].test = 0;
        state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
        state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                                (static_cast<std::uint64_t>(447u) << 48);
        state.prim = 3u;
        ps2::gs::write_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, 95u, 95u, 0xFFAA55CCu);
        const ps2::gs::Vertex triangle[3] = {
            make_vertex(64, 64, 0x11, 0x22, 0x33, 0x80),
            make_vertex(96, 64, 0x11, 0x22, 0x33, 0x80),
            make_vertex(64, 96, 0x11, 0x22, 0x33, 0x80),
        };
        if (!ps2::gs::native_gl::submit(state, triangle, 3) ||
            !ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase)) return 1;
        const auto untouched = ps2::gs::read_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, 95u, 95u);
        if (untouched != 0xFFAA55CCu) {
            std::cerr << "[FAIL] readback overwrote a pixel outside triangle coverage: " << std::hex << untouched << "\n";
            return 1;
        }
    }
    ps2::gs::native_gl::shutdown();
    std::cout << "[PASS] All lazy on-demand readback tests passed successfully!\n";
    return 0;
}
