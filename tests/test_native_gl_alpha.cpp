#include "gs/gs.hpp"
#include "gs/native_gl_backend.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

bool draw_triangle(ps2::gs::State &state, std::uint64_t prim,
                   std::uint8_t r, std::uint8_t g,
                   std::uint8_t b, std::uint8_t a) {
    state.prim = prim;
    const ps2::gs::Vertex vertices[] = {
        make_vertex(10, 10, r, g, b, a),
        make_vertex(100, 10, r, g, b, a),
        make_vertex(10, 100, r, g, b, a),
    };
    return ps2::gs::native_gl::submit(state, vertices, 3);
}

} // namespace

int main() {
    _putenv_s("PS2_NATIVE_GL", "1");
    _putenv_s("PS2_GS", "native-gl");

    ps2::gs::State state{};
    state.prmodecont = 1;
    state.colclamp = 1;
    state.ctx[0].frame = (static_cast<std::uint64_t>(10) << 16) | 0x5Bu;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(127) << 16) |
                            (static_cast<std::uint64_t>(127) << 48);
    state.ctx[0].alpha = 0x44u;

    // The captured retail TEST state uses AFAIL=FB_ONLY, ZTST=ALWAYS and
    // ZBUF.ZMSK=1. Even a zero-alpha fragment must still update color; the
    // alpha test cannot change framebuffer or depth output in this state.
    state.ctx[0].test = 0x3101Bu;
    state.ctx[0].zbuf = 1ull << 32;
    if (!draw_triangle(state, 4u | 8u, 255, 0, 0, 0)) {
        std::fprintf(stderr, "native GL rejected an inert GS alpha test\n");
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    ps2::gs::native_gl::flush();
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

    // Establish a blue destination, then exercise the captured packet's
    // PSMCT32 + TCC + TFX=MODULATE path with the exact GS source-over tuple.
    // TEXA=0x80 and vertex alpha=0x80 produce As=0x80; As/128 is one, so the
    // result must be the source color, not a 50% GL blend.
    if (!draw_triangle(state, 4u | 8u, 0, 0, 255, 255)) {
        std::fprintf(stderr, "native GL rejected the destination setup primitive\n");
        return 1;
    }
    constexpr std::uint32_t texture_base = 0x100u;
    ps2::gs::write_pixel(ps2::gs::PSMCT32, texture_base, 1u, 0u, 0u, 0x800000FFu);
    state.ctx[0].tex0 = texture_base | (1ull << 14) | (1ull << 34); // 1x1 CT32, TCC=1, TFX=MODULATE
    if (!draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 6) | (1u << 8), 255, 255, 255, 128)) {
        std::fprintf(stderr, "native GL rejected the GS ALPHA=0x44 source-over primitive\n");
        ps2::gs::native_gl::shutdown();
        return 2;
    }

    ps2::gs::native_gl::flush();
    const std::uint32_t pixel = ps2::gs::read_pixel(ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    // GS PSMCT32 stores R in the low byte and A in the high byte.
    constexpr std::uint32_t expected = 0x800000FFu;
    if (pixel != expected) {
        std::fprintf(stderr, "GS ALPHA=0x44 pixel mismatch: got %08X expected %08X\n", pixel, expected);
        ps2::gs::native_gl::shutdown();
        return 3;
    }
    std::fprintf(stderr, "GS ALPHA=0x44 PSMCT32 source-over pixel verified: %08X\n", pixel);

    // Reuse the same cached GS texture over a fresh destination. The first
    // blend changes the compositor's GL texture bindings; the second must
    // still sample the cached GS texture, not the compositor's prior source.
    if (!draw_triangle(state, 4u | 8u, 0, 0, 255, 255)) {
        std::fprintf(stderr, "native GL rejected the second destination setup primitive\n");
        ps2::gs::native_gl::shutdown();
        return 4;
    }
    constexpr std::uint32_t repeated_texture_base = texture_base + 0x100u;
    ps2::gs::write_pixel(ps2::gs::PSMCT32, repeated_texture_base, 1u, 0u, 0u, 0x200000FFu);
    state.ctx[0].tex0 = repeated_texture_base | (1ull << 14) | (1ull << 34);
    if (!draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 6) | (1u << 8), 255, 255, 255, 255) ||
        !draw_triangle(state, 4u | 8u | (1u << 4) | (1u << 6) | (1u << 8), 255, 255, 255, 255)) {
        std::fprintf(stderr, "native GL rejected a repeated cached-texture source-over primitive\n");
        ps2::gs::native_gl::shutdown();
        return 5;
    }
    ps2::gs::native_gl::flush();
    const std::uint32_t repeated_texture_pixel = ps2::gs::read_pixel(
        ps2::gs::PSMCT32, 0x5Bu * 32u, 10u, 20u, 20u);
    ps2::gs::native_gl::shutdown();
    // TEXA=0x20 modulated by vertex alpha 0xFF gives GS As=0x40 (factor 1/2).
    // Two source-over draws over opaque blue produce approximately
    // (191,0,64,112); permit one code value for framebuffer quantization.
    constexpr std::array<int, 4> expected_repeated_rgba{191, 0, 64, 112};
    const std::array<int, 4> actual_repeated_rgba{
        static_cast<int>(repeated_texture_pixel & 0xFFu),
        static_cast<int>((repeated_texture_pixel >> 8) & 0xFFu),
        static_cast<int>((repeated_texture_pixel >> 16) & 0xFFu),
        static_cast<int>((repeated_texture_pixel >> 24) & 0xFFu),
    };
    bool repeated_pixel_matches = true;
    for (std::size_t i = 0; i < expected_repeated_rgba.size(); ++i)
        repeated_pixel_matches &= std::abs(actual_repeated_rgba[i] - expected_repeated_rgba[i]) <= 1;
    if (!repeated_pixel_matches) {
        std::fprintf(stderr, "cached texture state mismatch: got %08X expected about (191,0,64,112)\n",
                     repeated_texture_pixel);
        return 6;
    }
    std::fprintf(stderr, "GS alpha compositor preserved cached texture bindings: %08X\n",
                 repeated_texture_pixel);
    return 0;
}
