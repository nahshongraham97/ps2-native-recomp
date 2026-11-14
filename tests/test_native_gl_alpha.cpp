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
    state.ctx[0].test = 0;
    state.ctx[0].zbuf = 0;

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
