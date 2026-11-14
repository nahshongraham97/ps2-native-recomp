// Native GL vs software reference for the captured retail ALPHA=0x44 packet
// shape (PSMCT32 + TCC + TFX=MODULATE + source-over).
//
// The expectations used to be hand-derived constants from the fullscreen
// alpha compositor era; two of them were unreachable (the readback path
// stopped syncing after Step 5's lazy readback, and the framebuffer alpha
// byte was asserted as the raw GS value 0x80 although the pipeline stores
// the x2 convention where GS 0x80 = GL 0xFF, commit a3efc7f). Every blend
// expectation is now produced by running the identical draw sequence through
// the reference rasterizer, so the test measures agreement rather than
// re-deriving arithmetic by hand.
//
// Comparison: RGB within 1 unit (GS integer /128 blending vs GL /255),
// alpha within 1 unit of twice the reference byte (the x2 storage
// convention).

#include "gs/gs.hpp"
#include "gs/native_gl_backend.hpp"
#include "gs/raster.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace {

ps2::gs::Vertex make_vertex(std::uint32_t x, std::uint32_t y,
                            std::uint8_t r, std::uint8_t g,
                            std::uint8_t b, std::uint8_t a) {
    ps2::gs::Vertex vertex{};
    vertex.x = x * 16u;
    vertex.y = y * 16u;
    vertex.z = 0;
    vertex.r = r;
    vertex.g = g;
    vertex.b = b;
    vertex.a = a;
    vertex.q = 1.0f;
    return vertex;
}

void draw_triangle(ps2::gs::State &state, std::uint64_t prim,
                   std::uint8_t r, std::uint8_t g,
                   std::uint8_t b, std::uint8_t a, bool use_gl) {
    state.prim = prim;
    const ps2::gs::Vertex vertices[] = {
        make_vertex(10, 10, r, g, b, a),
        make_vertex(100, 10, r, g, b, a),
        make_vertex(10, 100, r, g, b, a),
    };
    if (use_gl) {
        if (!ps2::gs::native_gl::submit(state, vertices, 3)) {
            std::fprintf(stderr, "native GL rejected a sequence primitive (prim %llX)\n",
                         static_cast<unsigned long long>(prim));
        }
    } else {
        ps2::gs::raster::draw(state, vertices, 3);
    }
}

// The full blend sequence, executed once per renderer. Vertex alpha stays at
// 0x80: that is the GS "opaque" convention the pipeline's x2 scaling encodes,
// and values above 0x80 are not representable as GL fragment alpha.
std::uint32_t run_sequence(ps2::gs::State state, bool use_gl) {
    const auto trace = [&](const char *step) {
        if (std::getenv("ALPHA_DEBUG")) {
            if (use_gl) {
                ps2::gs::native_gl::flush();
                ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
            }
            std::fprintf(stderr, "[%s] %s pixel=%08X\n", use_gl ? "gl" : "ref", step,
                         ps2::gs::read_pixel(ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u));
        }
    };
    // Establish a blue destination.
    draw_triangle(state, 4u | 8u, 0, 0, 255, 128, use_gl);
    trace("blue dest");
    // Captured packet shape: PSMCT32 + TCC + TFX=MODULATE + ALPHA=0x44,
    // texture alpha 0x80 -> As=0x80 -> factor 1: the source must replace the
    // destination, not half-blend it.
    constexpr std::uint32_t texture_base = 0x100u;
    ps2::gs::write_pixel(ps2::gs::PSMCT32, texture_base, 1u, 0u, 0u, 0x800000FFu);
    state.ctx[0].tex0 = texture_base | (1ull << 14) | (1ull << 34); // 1x1 CT32, TCC=1, TFX=MODULATE
    draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 6) | (1u << 8), 255, 255, 255, 128, use_gl);
    trace("opaque source-over");

    // Fresh destination, then the same cached-texture path twice in a row
    // with a partially transparent texture (0x20 -> As=0x40 -> factor 1/2).
    draw_triangle(state, 4u | 8u, 0, 0, 255, 128, use_gl);
    trace("blue dest 2");
    constexpr std::uint32_t repeated_texture_base = texture_base + 0x100u;
    ps2::gs::write_pixel(ps2::gs::PSMCT32, repeated_texture_base, 1u, 0u, 0u, 0x200000FFu);
    state.ctx[0].tex0 = repeated_texture_base | (1ull << 14) | (1ull << 34);
    draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 6) | (1u << 8), 255, 255, 255, 128, use_gl);
    trace("half source-over 1");
    draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 6) | (1u << 8), 255, 255, 255, 128, use_gl);
    trace("half source-over 2");

    if (use_gl) {
        ps2::gs::native_gl::flush();
        ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    }
    return ps2::gs::read_pixel(ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
}

void clear_target() {
    for (std::uint32_t y = 0; y < 128; ++y) {
        for (std::uint32_t x = 0; x < 128; ++x) {
            ps2::gs::write_pixel(ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, x, y, 0u);
        }
    }
}

} // namespace

int main() {
    _putenv_s("PS2_NATIVE_GL", "1");
    _putenv_s("PS2_GS", "native-gl");
    // Both backends use GS pixel addressing; initialize its swizzle tables
    // before writing the reference framebuffer or texture.
    ps2::gs::init();

    ps2::gs::State state{};
    state.prmodecont = 1;
    state.colclamp = 1;
    ps2::gs::raster::g_probe.on = std::getenv("PS2_TRACE_PIXEL") != nullptr;
    state.ctx[0].frame = (static_cast<std::uint64_t>(10) << 16) | 0x5Bu;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(127) << 16) |
                            (static_cast<std::uint64_t>(127) << 48);
    state.ctx[0].alpha = 0x44u;

    // The captured retail TEST state uses AFAIL=FB_ONLY, ZTST=ALWAYS and
    // ZBUF.ZMSK=1. Even a zero-alpha fragment must still update color; the
    // alpha test cannot change framebuffer or depth output in this state.
    state.ctx[0].test = 0x3101Bu;
    state.ctx[0].zbuf = 1ull << 32;
    clear_target();
    draw_triangle(state, 4u | 8u, 255, 0, 0, 0, true);
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    const std::uint32_t alpha_test_pixel = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    if (alpha_test_pixel != 0x000000FFu) {
        std::fprintf(stderr, "GS FB_ONLY alpha test mismatch: got %08X expected 000000FF\n",
                     alpha_test_pixel);
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    // Captured TEST=0x5101B: NOTEQUAL alpha test, FB_ONLY on failure,
    // depth GEQUAL, and ZBUF masked. Alpha=1 fails NOTEQUAL against AREF=1,
    // but must still write color while the depth test remains active.
    state.ctx[0].test = 0x5101Bu;
    state.ctx[0].zbuf = (1ull << 32) | 0x0A0000E7u;
    draw_triangle(state, 4u | 8u, 0, 255, 0, 1, true);
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    const std::uint32_t depth_test_pixel = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    if (depth_test_pixel != 0x0200FF00u) {
        std::fprintf(stderr, "GS FB_ONLY masked-depth mismatch: got %08X expected 0200FF00\n", depth_test_pixel);
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    // A failing depth comparison must still suppress the framebuffer write.
    state.ctx[0].test = 0x50000u;
    state.ctx[0].zbuf = 0x0A0000E7u;
    ps2::gs::Vertex depth_vertices[] = {
        make_vertex(10, 10, 255, 0, 0, 128),
        make_vertex(100, 10, 255, 0, 0, 128),
        make_vertex(10, 100, 255, 0, 0, 128),
    };
    for (auto &vertex : depth_vertices) vertex.z = 32768;
    if (!ps2::gs::native_gl::submit(state, depth_vertices, 3)) return 1;
    state.ctx[0].test = 0x5101Bu;
    state.ctx[0].zbuf |= 1ull << 32;
    for (auto &vertex : depth_vertices) {
        vertex.z = 0;
        vertex.r = 0;
        vertex.g = 255;
        vertex.a = 1;
    }
    if (!ps2::gs::native_gl::submit(state, depth_vertices, 3)) return 1;
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    const std::uint32_t rejected_depth_pixel = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    if (rejected_depth_pixel != 0xFF0000FFu) {
        std::fprintf(stderr, "GS FB_ONLY depth rejection mismatch: got %08X expected FF0000FF\n", rejected_depth_pixel);
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    state.ctx[0].test = 0;
    state.ctx[0].zbuf = 0;

    // Retail packets also mask the complete destination alpha byte. Verify
    // that the RGB write is accepted, alpha is preserved, and a subsequent
    // unmasked draw can write alpha again.
    clear_target();
    draw_triangle(state, 4u | 8u, 0, 0, 255, 64, true);
    state.ctx[0].frame |= 0xFF000000ull << 32;
    draw_triangle(state, 4u | 8u, 0, 255, 0, 0, true);
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    const std::uint32_t masked = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    if (masked != 0x8000FF00u) {
        std::fprintf(stderr, "GS alpha-byte mask mismatch: got %08X expected 8000FF00\n", masked);
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    state.ctx[0].frame &= 0xFFFFFFFFull;
    draw_triangle(state, 4u | 8u, 255, 0, 0, 128, true);
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    const std::uint32_t unmasked = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    if (unmasked != 0xFF0000FFu) {
        std::fprintf(stderr, "GL color mask was not restored: got %08X expected FF0000FF\n", unmasked);
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    state.ctx[0].frame |= 0x01000000ull << 32;
    const std::uint32_t previous_fbp = ps2::gs::native_gl::active_frame_fbp();
    state.ctx[0].frame = (state.ctx[0].frame & ~0x1FFull) | 0x5Cu;
    const ps2::gs::Vertex partial_mask_vertices[] = {
        make_vertex(10, 10, 0, 255, 0, 128),
        make_vertex(100, 10, 0, 255, 0, 128),
        make_vertex(10, 100, 0, 255, 0, 128),
    };
    if (ps2::gs::native_gl::submit(state, partial_mask_vertices, 3)) {
        std::fprintf(stderr, "partial-bit framebuffer mask was incorrectly accepted\n");
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    if (ps2::gs::native_gl::active_frame_fbp() != previous_fbp) {
        std::fprintf(stderr, "rejected primitive changed the active framebuffer\n");
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    state.ctx[0].frame = (state.ctx[0].frame & ~0x1FFull) | 0x5Bu;
    state.ctx[0].frame &= 0xFFFFFFFFull;

    // Captured retail tuple ALPHA=0xA8, FIX=0x80 computes source RGB exactly.
    // Compare against the reference with a different destination color.
    state.ctx[0].alpha = (0x80ull << 32) | 0xA8u;
    draw_triangle(state, 4u | 8u, 0, 0, 255, 128, false);
    draw_triangle(state, 4u | 8u | (1u << 6), 48, 96, 192, 64, false);
    const std::uint32_t fixed_ref = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    draw_triangle(state, 4u | 8u, 0, 0, 255, 128, true);
    draw_triangle(state, 4u | 8u | (1u << 6), 48, 96, 192, 64, true);
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
    const std::uint32_t fixed_gl = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    for (int channel = 0; channel < 4; ++channel) {
        const int reference = (fixed_ref >> (channel * 8)) & 0xFFu;
        const int actual = (fixed_gl >> (channel * 8)) & 0xFFu;
        const int expected = channel == 3 ? std::min(255, 2 * reference) : reference;
        if (std::abs(actual - expected) > 1) {
            std::fprintf(stderr, "GS ALPHA=0xA8 FIX=0x80 mismatch: reference %08X GL %08X\n", fixed_ref, fixed_gl);
            ps2::gs::native_gl::shutdown();
            return 1;
        }
    }
    state.ctx[0].alpha = (0x40ull << 32) | 0xA8u;
    if (ps2::gs::native_gl::submit(state, partial_mask_vertices, 3)) {
        std::fprintf(stderr, "unsupported GS ALPHA=0xA8 FIX=0x40 was incorrectly accepted\n");
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    state.ctx[0].alpha = 0x44u;

    if (std::getenv("MKA_TEST_COMBINER_PARITY")) {
        for (std::uint64_t tfx = 0; tfx < 4; ++tfx) {
            for (std::uint64_t tcc = 0; tcc < 2; ++tcc) {
                const std::uint32_t texture_base = 0x500u + static_cast<std::uint32_t>((tfx * 2 + tcc) * 0x20u);
                ps2::gs::write_pixel(ps2::gs::PSMCT32, texture_base, 1u, 0, 0, 0x405080A0u);
                state.ctx[0].tex0 = texture_base | (1ull << 14) | (tcc << 34) | (tfx << 35);
                clear_target();
                draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 8), 96, 64, 32, 64, false);
                const auto reference = ps2::gs::read_pixel(ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
                clear_target();
                draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 8), 96, 64, 32, 64, true);
                ps2::gs::native_gl::sync_surface_to_cpu(0x5Bu);
                const auto actual = ps2::gs::read_pixel(ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
                for (int ch = 0; ch < 4; ++ch) {
                    const int expected_channel = ch == 3 ? std::min(255u, ((reference >> 24) & 255u) * 2u)
                                                         : ((reference >> (ch * 8)) & 255u);
                    if (std::abs(static_cast<int>((actual >> (ch * 8)) & 255u) - expected_channel) > 2) {
                        std::fprintf(stderr, "combiner mismatch tfx=%llu tcc=%llu ref=%08X GL=%08X\n",
                                     static_cast<unsigned long long>(tfx), static_cast<unsigned long long>(tcc), reference, actual);
                        return 1;
                    }
                }
            }
        }
    }
    clear_target();
    const std::uint32_t expected = run_sequence(state, false);
    clear_target();
    const std::uint32_t actual = run_sequence(state, true);
    ps2::gs::native_gl::shutdown();

    bool pass = true;
    for (int ch = 0; ch < 4; ++ch) {
        const int shift = ch * 8;
        const int ref_value = static_cast<int>((expected >> shift) & 0xFFu);
        const int gl_value = static_cast<int>((actual >> shift) & 0xFFu);
        const int target = ch == 3 ? std::min(255, 2 * ref_value) : ref_value;
        if (std::abs(gl_value - target) > 1) pass = false;
    }
    if (!pass) {
        std::fprintf(stderr,
                     "GS ALPHA=0x44 sequence mismatch: reference %08X native GL %08X "
                     "(alpha compared under the x2 storage convention)\n",
                     expected, actual);
        return 2;
    }
    std::fprintf(stderr,
                 "GS ALPHA=0x44 PSMCT32 source-over matches the reference: "
                 "reference %08X native GL %08X\n",
                 expected, actual);
    return 0;
}
