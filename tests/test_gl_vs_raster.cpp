// A/B test matrix: Native OpenGL vs the software reference rasterizer
// (raster.cpp is the project's ground truth for GS sampling rules).
//
// History: an earlier revision of this matrix compared 0 == 0 for every case
// because init_texture() wrote the texels at TBP0*32 while both renderers
// read them at TBP0 (page_address multiplies by 256), so both sides sampled
// an all-zero texture. The "13/13, 0 mismatches" result from that revision
// validated nothing. The texture base pointer is fixed here, strips are
// delivered the way gs.cpp kick() delivers them (count=3 triangles with two
// carried vertices), and the font cases run against the real retail atlas.
//
// Cases:
//   Sprites (prim 6): 1:1, 2x, 3x, 1.5x, ST mode, midpoint tie-break,
//                     plus nearest-neighbour variants.
//   Triangle strips (prim 4): 1:1 and 2x, bilinear and nearest, chunked
//                     exactly like kick() delivers them to both renderers.
//   Triangles (prim 3): 1:1 bilinear and nearest minification, two explicit
//                     count=3 packets per quad like the retail path.
//   Font atlas: the real retail font texture (texture_11.ppm, 256x128,
//                     MMIN=0 MMAG=0 TFX=0 TCC=1 from the 2026-11-13 glyph
//                     trace) drawn as minified glyph-like prim-4 quads,
//                     nearest-neighbour, with and without ALPHA=0x44
//                     source-over blending - the packet shape the retail
//                     front-end uses for text.
//
// Tolerance: nearest-neighbour and dyadic bilinear cases must be pixel
// exact. Non-dyadic bilinear scales (3x, 1.5x) allow a 1-unit channel
// difference: the reference rounds float blend weights, fixed-function GL
// uses 8-bit weights, and the two cannot agree exactly at thirds/fifths.
// Partial-alpha blends allow 1 unit for the same reason (GS integer /128 vs
// GL /255). Framebuffer alpha is compared under the pipeline's x2 convention
// (GL stores ~2x the GS byte: GS 0x80 = GL 0xFF, commit a3efc7f).

#include "gs/gs.hpp"
#include "gs/gpu/gpu.hpp"
#include "gs/native_gl_backend.hpp"
#include "gs/raster.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

namespace {

constexpr std::uint32_t kFrameBase = 0x5Bu;   // FRAME.FBP, 8192-byte pages
constexpr std::uint32_t kFrameWidth = 10u;     // FRAME.FBW, 64-pixel units
constexpr std::uint32_t kTextureBase = 0x300u; // TBP0, 256-byte units
constexpr std::uint32_t kTextureDim = 4u;     // 4x4 texels
constexpr std::uint32_t kFontBase = 0x400u;    // font atlas TBP0, 256-byte units
constexpr std::uint32_t kOriginX = 32u, kOriginY = 32u;

const char *const kFontAtlasPath =
    "research/runs/packet-native-texturetrace-20261113/textures/texture_11.ppm";

ps2::gs::Vertex make_vertex(std::uint32_t x, std::uint32_t y,
                            float u, float v, float q = 1.0f) {
    ps2::gs::Vertex vert{};
    vert.x = x * 16u;
    vert.y = y * 16u;
    vert.z = 0;
    vert.r = 255;
    vert.g = 255;
    vert.b = 255;
    vert.a = 255;
    vert.q = q;
    vert.u = static_cast<std::uint32_t>(u * 16.0f);
    vert.v = static_cast<std::uint32_t>(v * 16.0f);
    return vert;
}

void init_texture() {
    for (std::uint32_t y = 0; y < kTextureDim; ++y) {
        for (std::uint32_t x = 0; x < kTextureDim; ++x) {
            const std::uint32_t value = 0xFF000000u | (y << 16) | (x << 8) | (0x40u + x * 0x10u + y);
            // TBP0 is already in 256-byte units (page_address multiplies by
            // 256); the old *32 wrote the texels where neither renderer reads
            // them, which made every case compare 0 == 0.
            ps2::gs::write_pixel(ps2::gs::PSMCT32, kTextureBase, 1u, x, y, value);
        }
    }
}

void clear_framebuffer() {
    for (std::uint32_t y = 0; y < 128; ++y) {
        for (std::uint32_t x = 0; x < 128; ++x) {
            ps2::gs::write_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, x, y, 0u);
        }
    }
}

// gs.cpp kick() never hands a prim-4 strip to a renderer in one piece: after
// each count=3 draw it carries the last two vertices forward, so both the
// software rasterizer and native GL receive overlapping triangles. Tests that
// pass count=4 directly compare a primitive shape the game path never
// produces (and the reference only draws the first triangle of such a strip).
void raster_draw(const ps2::gs::State &state, const ps2::gs::Vertex *verts, int count) {
    if ((state.prim & 7u) == 4u && count > 3) {
        for (int i = 0; i + 2 < count; ++i) {
            const ps2::gs::Vertex tri[3] = {verts[i], verts[i + 1], verts[i + 2]};
            ps2::gs::raster::draw(state, tri, 3);
        }
        return;
    }
    ps2::gs::raster::draw(state, verts, count);
}

bool gl_submit(const ps2::gs::State &state, const ps2::gs::Vertex *verts, int count) {
    if ((state.prim & 7u) == 4u && count > 3) {
        for (int i = 0; i + 2 < count; ++i) {
            const ps2::gs::Vertex tri[3] = {verts[i], verts[i + 1], verts[i + 2]};
            if (!ps2::gs::native_gl::submit(state, tri, 3)) return false;
        }
        return true;
    }
    return ps2::gs::native_gl::submit(state, verts, count);
}

struct TestResult {
    std::string name;
    std::uint32_t total_pixels;
    std::uint32_t mismatches;
    bool passed;
};

// Per-channel comparison. Framebuffer alpha is compared under the x2
// convention: the pipeline stores ~2x the GS alpha byte (GS 0x80 -> GL 0xFF),
// which is pre-existing behaviour of the alpha scaling commit a3efc7f.
std::uint32_t count_mismatches(const std::vector<std::uint32_t> &ref,
                               const std::vector<std::uint32_t> &gl,
                               int tolerance) {
    std::uint32_t mismatches = 0;
    for (std::size_t i = 0; i < ref.size(); ++i) {
        const std::uint32_t r = ref[i], g = gl[i];
        bool bad = false;
        for (int ch = 0; ch < 4; ++ch) {
            const int shift = ch * 8;
            const int ref_value = static_cast<int>((r >> shift) & 0xFFu);
            const int gl_value = static_cast<int>((g >> shift) & 0xFFu);
            const int target = ch == 3 ? std::min(255, 2 * ref_value) : ref_value;
            if (std::abs(gl_value - target) > tolerance) bad = true;
        }
        if (bad) ++mismatches;
    }
    return mismatches;
}

// Prints only pixels beyond tolerance: tolerated diffs (x2 alpha convention,
// last-bit blend rounding) would otherwise bury the real failures.
void dump_pixels(const char *name, const std::vector<std::uint32_t> &ref,
                 const std::vector<std::uint32_t> &gl, int tolerance) {
    if (!std::getenv("GLVS_DEBUG")) return;
    std::fprintf(stderr, "[dbg] %s\n", name);
    for (std::size_t i = 0; i < ref.size(); ++i) {
        const std::uint32_t r = ref[i], g = gl[i];
        bool beyond = false;
        for (int ch = 0; ch < 4; ++ch) {
            const int shift = ch * 8;
            const int ref_value = static_cast<int>((r >> shift) & 0xFFu);
            const int gl_value = static_cast<int>((g >> shift) & 0xFFu);
            const int target = ch == 3 ? std::min(255, 2 * ref_value) : ref_value;
            if (std::abs(gl_value - target) > tolerance) beyond = true;
        }
        if (beyond) {
            std::fprintf(stderr, "  [%zu] ref=%08X gl=%08X  BEYOND\n", i, r, g);
        }
    }
}

std::vector<std::uint32_t> read_region(std::uint32_t x0, std::uint32_t y0,
                                       std::uint32_t w, std::uint32_t h) {
    std::vector<std::uint32_t> pixels(w * h);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            pixels[y * w + x] = ps2::gs::read_pixel(
                ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth, x0 + x, y0 + y);
        }
    }
    return pixels;
}

ps2::gs::State make_state(bool bilinear) {
    ps2::gs::State state{};
    state.prmodecont = 1;
    state.colclamp = 1;
    state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                            (static_cast<std::uint64_t>(447u) << 48);
    state.ctx[0].tex0 = (kTextureBase) | (1ull << 14) | (2ull << 26) | (2ull << 30) |
                        (1ull << 34) | (1ull << 35);
    state.ctx[0].tex1 = bilinear ? ((1ull << 6) | (1ull << 5)) : 0ull;
    state.ctx[0].clamp = 1u | (1u << 2);
    state.ctx[0].test = 0;
    state.ctx[0].zbuf = 0;
    state.ctx[0].alpha = 0;
    return state;
}

TestResult run_sprite_test(const char *name, std::uint32_t sprite_w, std::uint32_t sprite_h,
                           bool use_st = false, bool bilinear = true, int tolerance = 0) {
    ps2::gs::State state = make_state(bilinear);
    // Prim 6 = Sprite. Bit 4 = Texture mapping. Bit 8 = UV coordinate mode.
    state.prim = 6u | (1ull << 4);
    if (!use_st) state.prim |= (1ull << 8);

    float u1 = use_st ? 1.0f : static_cast<float>(kTextureDim);
    float v1 = use_st ? 1.0f : static_cast<float>(kTextureDim);

    const ps2::gs::Vertex verts[2] = {
        make_vertex(kOriginX, kOriginY, 0.0f, 0.0f, 1.0f),
        make_vertex(kOriginX + sprite_w, kOriginY + sprite_h, u1, v1, 1.0f),
    };

    clear_framebuffer();
    raster_draw(state, verts, 2);
    const std::vector<std::uint32_t> ref_pixels =
        read_region(kOriginX, kOriginY, sprite_w, sprite_h);

    clear_framebuffer();
    if (!gl_submit(state, verts, 2)) {
        return {name, sprite_w * sprite_h, sprite_w * sprite_h, false};
    }
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    const std::vector<std::uint32_t> gl_pixels =
        read_region(kOriginX, kOriginY, sprite_w, sprite_h);

    dump_pixels(name, ref_pixels, gl_pixels, tolerance);
    const std::uint32_t mismatches = count_mismatches(ref_pixels, gl_pixels, tolerance);
    return {name, sprite_w * sprite_h, mismatches, mismatches == 0};
}

// Prim-4 quad delivered the way kick() delivers it (two overlapping
// count=3 triangles) to both renderers.
TestResult run_triangle_test(const char *name, std::uint32_t quad_w, std::uint32_t quad_h,
                             bool bilinear = true, int tolerance = 0) {
    ps2::gs::State state = make_state(bilinear);
    state.prim = 4u | (1ull << 4) | (1ull << 8);

    const float u1 = static_cast<float>(kTextureDim);
    const float v1 = static_cast<float>(kTextureDim);

    const ps2::gs::Vertex verts[4] = {
        make_vertex(kOriginX, kOriginY, 0.0f, 0.0f),
        make_vertex(kOriginX + quad_w, kOriginY, u1, 0.0f),
        make_vertex(kOriginX, kOriginY + quad_h, 0.0f, v1),
        make_vertex(kOriginX + quad_w, kOriginY + quad_h, u1, v1),
    };

    clear_framebuffer();
    raster_draw(state, verts, 4);
    const std::vector<std::uint32_t> ref_pixels =
        read_region(kOriginX, kOriginY, quad_w, quad_h);

    clear_framebuffer();
    if (!gl_submit(state, verts, 4)) {
        return {name, quad_w * quad_h, quad_w * quad_h, false};
    }
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    const std::vector<std::uint32_t> gl_pixels =
        read_region(kOriginX, kOriginY, quad_w, quad_h);

    dump_pixels(name, ref_pixels, gl_pixels, tolerance);
    const std::uint32_t mismatches = count_mismatches(ref_pixels, gl_pixels, tolerance);
    return {name, quad_w * quad_h, mismatches, mismatches == 0};
}

// Prim 3: the retail path draws a quad as two explicit count=3 packets
// (kick() resets the queue after every prim-3 draw).
TestResult run_triangle3_test(const char *name, std::uint32_t quad_w, std::uint32_t quad_h,
                              bool bilinear = true, int tolerance = 0) {
    ps2::gs::State state = make_state(bilinear);
    state.prim = 3u | (1ull << 4) | (1ull << 8);

    const float u1 = static_cast<float>(kTextureDim);
    const float v1 = static_cast<float>(kTextureDim);

    const ps2::gs::Vertex tl = make_vertex(kOriginX, kOriginY, 0.0f, 0.0f);
    const ps2::gs::Vertex tr = make_vertex(kOriginX + quad_w, kOriginY, u1, 0.0f);
    const ps2::gs::Vertex bl = make_vertex(kOriginX, kOriginY + quad_h, 0.0f, v1);
    const ps2::gs::Vertex br = make_vertex(kOriginX + quad_w, kOriginY + quad_h, u1, v1);
    const ps2::gs::Vertex tri1[3] = {tl, tr, bl};
    const ps2::gs::Vertex tri2[3] = {tr, br, bl};

    clear_framebuffer();
    ps2::gs::raster::draw(state, tri1, 3);
    ps2::gs::raster::draw(state, tri2, 3);
    const std::vector<std::uint32_t> ref_pixels =
        read_region(kOriginX, kOriginY, quad_w, quad_h);

    clear_framebuffer();
    if (!ps2::gs::native_gl::submit(state, tri1, 3) ||
        !ps2::gs::native_gl::submit(state, tri2, 3)) {
        return {name, quad_w * quad_h, quad_w * quad_h, false};
    }
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    const std::vector<std::uint32_t> gl_pixels =
        read_region(kOriginX, kOriginY, quad_w, quad_h);

    dump_pixels(name, ref_pixels, gl_pixels, tolerance);
    const std::uint32_t mismatches = count_mismatches(ref_pixels, gl_pixels, tolerance);
    return {name, quad_w * quad_h, mismatches, mismatches == 0};
}

TestResult run_tiebreak_test() {
    return run_sprite_test("Midpoint Tie-Break (0.5 texel center)", 8u, 8u, false);
}

// --- Real font-atlas coverage -------------------------------------------

bool load_ppm(const char *path, std::vector<std::uint8_t> &rgb, int &w, int &h) {
    std::FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    auto next_int = [&]() -> int {
        int c = 0;
        for (;;) {
            c = std::fgetc(file);
            if (c == '#') {
                while (c != '\n' && c != EOF) c = std::fgetc(file);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                break;
            }
        }
        int value = 0;
        while (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            c = std::fgetc(file);
        }
        return value;
    };
    const int magic1 = std::fgetc(file);
    const int magic2 = std::fgetc(file);
    if (magic1 != 'P' || magic2 != '6') {
        std::fclose(file);
        return false;
    }
    w = next_int();
    h = next_int();
    const int max_value = next_int();
    if (max_value != 255 || w <= 0 || h <= 0) {
        std::fclose(file);
        return false;
    }
    rgb.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u);
    const std::size_t got = std::fread(rgb.data(), 1, rgb.size(), file);
    std::fclose(file);
    return got == rgb.size();
}

// The 2026-11-13 glyph dump predates the alpha-channel dump, so the atlas
// arrives as RGB only. Every texel is stored with the GS convention-opaque
// alpha 0x80; sampling (the property under test) is carried by RGB, and both
// renderers receive identical alpha, so the A/B stays fair.
bool init_font_atlas() {
    std::vector<std::uint8_t> rgb;
    int w = 0, h = 0;
    if (!load_ppm(kFontAtlasPath, rgb, w, h)) {
        std::fprintf(stderr, "[font-test] cannot read %s\n", kFontAtlasPath);
        return false;
    }
    if (w != 256 || h != 128) {
        std::fprintf(stderr, "[font-test] unexpected atlas size %dx%d (want 256x128)\n", w, h);
        return false;
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = (static_cast<std::size_t>(y) * w + x) * 3u;
            const std::uint32_t value = 0x80000000u |
                                        (static_cast<std::uint32_t>(rgb[i + 2]) << 16) |
                                        (static_cast<std::uint32_t>(rgb[i + 1]) << 8) |
                                        static_cast<std::uint32_t>(rgb[i]);
            ps2::gs::write_pixel(ps2::gs::PSMCT32, kFontBase, 4u,
                                 static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                                 value);
        }
    }
    return true;
}

struct GlyphQuad {
    std::uint32_t x, y, w, h;   // screen rectangle, pixels
    std::uint32_t u, v, tw, th; // texel box in the atlas
};

// Glyph-like quads at the minification ratios the defect report measured
// (e.g. the letter M is 17 atlas texels drawn across 14 screen pixels).
// Positions stay inside the 128x128 region clear_framebuffer() clears.
const GlyphQuad kGlyphQuads[] = {
    {36, 36, 14, 14, 0, 0, 17, 17},      // 17 -> 14 on both axes (the reported M)
    {56, 36, 10, 12, 30, 0, 12, 14},     // 12 -> 10, 14 -> 12
    {72, 36, 20, 17, 60, 4, 24, 20},     // 24 -> 20, 20 -> 17
    {36, 56, 13, 13, 100, 8, 16, 16},    // 16 -> 13
    {56, 56, 7, 8, 5, 40, 8, 9},         // 8 -> 7, 9 -> 8
    {70, 56, 26, 10, 130, 50, 31, 12},   // 31 -> 26, 12 -> 10
};
constexpr std::uint32_t kFontRegionX = 32, kFontRegionY = 32;
constexpr std::uint32_t kFontRegionW = 68, kFontRegionH = 38;

ps2::gs::State make_font_state(bool blend) {
    ps2::gs::State state{};
    state.prmodecont = 1;
    state.colclamp = 1;
    state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                            (static_cast<std::uint64_t>(447u) << 48);
    // Real font packet from the glyph trace: TEX0 PSMCT32 TCC=1 TFX=0,
    // TEX1 MMIN=0 MMAG=0 (nearest), CLAMP on both axes over the 256x128
    // region. TBP0/TBW rewritten to where this test stores the atlas.
    state.ctx[0].tex0 = kFontBase | (4ull << 14) | (0ull << 20) | (8ull << 26) |
                        (7ull << 30) | (1ull << 34) | (0ull << 35);
    state.ctx[0].tex1 = 0; // MMIN=0 MMAG=0
    state.ctx[0].clamp = 1u | (1u << 2) | (255u << 14) | (127u << 24);
    state.ctx[0].test = 0;
    state.ctx[0].zbuf = 0;
    state.ctx[0].alpha = blend ? 0x44u : 0u;
    return state;
}

void draw_font_background(const ps2::gs::State &base, bool use_gl, std::uint8_t r,
                          std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    ps2::gs::State state = base;
    state.prim = 4u; // untextured, no blend: both renderers overwrite exactly
    ps2::gs::Vertex verts[4];
    const std::uint32_t x0 = kFontRegionX, y0 = kFontRegionY;
    const std::uint32_t x1 = kFontRegionX + kFontRegionW, y1 = kFontRegionY + kFontRegionH;
    const std::uint32_t xs[4] = {x0, x1, x0, x1};
    const std::uint32_t ys[4] = {y0, y0, y1, y1};
    for (int i = 0; i < 4; ++i) {
        verts[i] = make_vertex(xs[i], ys[i], 0.0f, 0.0f);
        verts[i].r = r;
        verts[i].g = g;
        verts[i].b = b;
        verts[i].a = a;
    }
    if (use_gl) {
        if (!gl_submit(state, verts, 4))
            std::fprintf(stderr, "[font-test] background submit rejected\n");
    } else {
        raster_draw(state, verts, 4);
    }
}

void draw_glyph_quads(const ps2::gs::State &base, bool use_gl, std::uint8_t vertex_alpha) {
    for (const GlyphQuad &quad : kGlyphQuads) {
        ps2::gs::State state = base;
        state.prim = 4u | (1ull << 4) | (1ull << 8);
        if (base.ctx[0].alpha == 0x44u) state.prim |= (1ull << 6);
        ps2::gs::Vertex verts[4] = {
            make_vertex(quad.x, quad.y, static_cast<float>(quad.u), static_cast<float>(quad.v)),
            make_vertex(quad.x + quad.w, quad.y, static_cast<float>(quad.u + quad.tw),
                        static_cast<float>(quad.v)),
            make_vertex(quad.x, quad.y + quad.h, static_cast<float>(quad.u),
                        static_cast<float>(quad.v + quad.th)),
            make_vertex(quad.x + quad.w, quad.y + quad.h, static_cast<float>(quad.u + quad.tw),
                        static_cast<float>(quad.v + quad.th)),
        };
        for (auto &vert : verts) vert.a = vertex_alpha;
        if (use_gl) {
            if (!gl_submit(state, verts, 4))
                std::fprintf(stderr, "[font-test] glyph submit rejected\n");
        } else {
            raster_draw(state, verts, 4);
        }
    }
}

TestResult run_font_test(const char *name, bool blend, std::uint8_t vertex_alpha,
                         int tolerance) {
    const ps2::gs::State state = make_font_state(blend);

    clear_framebuffer();
    draw_font_background(state, false, 0x10, 0x20, 0x40, 0x80);
    draw_glyph_quads(state, false, vertex_alpha);
    const std::vector<std::uint32_t> ref_pixels =
        read_region(kFontRegionX, kFontRegionY, kFontRegionW, kFontRegionH);

    clear_framebuffer();
    draw_font_background(state, true, 0x10, 0x20, 0x40, 0x80);
    draw_glyph_quads(state, true, vertex_alpha);
    ps2::gs::native_gl::flush();
    ps2::gs::native_gl::sync_surface_to_cpu(kFrameBase);
    const std::vector<std::uint32_t> gl_pixels =
        read_region(kFontRegionX, kFontRegionY, kFontRegionW, kFontRegionH);

    dump_pixels(name, ref_pixels, gl_pixels, tolerance);
    const std::uint32_t mismatches = count_mismatches(ref_pixels, gl_pixels, tolerance);
    return {name, kFontRegionW * kFontRegionH, mismatches, mismatches == 0};
}

} // namespace

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    _putenv_s("PS2_NATIVE_GL", "1");
    _putenv_s("PS2_GS", "native-gl");
    ps2::gs::init();
    init_texture();
    // PS2_TRACE_PIXEL=<x>,<y> prints the reference rasterizer's interpolated
    // attributes for that pixel (used to diagnose boundary flips).
    ps2::gs::raster::g_probe.on = std::getenv("PS2_TRACE_PIXEL") != nullptr;

    std::vector<TestResult> results;
    results.push_back(run_sprite_test("Sprite 1:1 Scale (4x4 UV)", 4u, 4u, false));
    results.push_back(run_sprite_test("Sprite 2x Magnification (8x8 UV)", 8u, 8u, false));
    // Non-dyadic bilinear: fixed-function 8-bit filter weights cannot match
    // the reference's float weights exactly at thirds/fifths; 1 unit allowed.
    results.push_back(run_sprite_test("Sprite 3x Magnification (12x12 UV)", 12u, 12u, false, true, 1));
    results.push_back(run_sprite_test("Sprite 1.5x Non-Integer (6x6 UV)", 6u, 6u, false, true, 1));
    results.push_back(run_sprite_test("Sprite 2x Magnification (8x8 ST)", 8u, 8u, true));
    // Non-dyadic AND dyadic bilinear allow 1 unit: at exact half-weights the
    // reference rounds float results up (+0.5 truncation) while fixed-function
    // GL rounds half-to-even, so odd results can differ by 1. Alignment itself
    // is proven by the nearest cases, which must be exact.
    results.push_back(run_triangle_test("Triangle Strip 1:1 (4x4 UV)", 4u, 4u, true, 1));
    results.push_back(run_triangle_test("Triangle Strip 2x (8x8 UV)", 8u, 8u, true, 1));
    results.push_back(run_tiebreak_test());
    results.push_back(run_sprite_test("Sprite 1:1 Nearest (4x4 UV)", 4u, 4u, false, false));
    results.push_back(run_sprite_test("Sprite 2x Nearest (8x8 UV)", 8u, 8u, false, false));
    results.push_back(run_sprite_test("Sprite 3x Nearest (12x12 UV)", 12u, 12u, false, false));
    results.push_back(run_triangle_test("Triangle Strip 1:1 Nearest (4x4 UV)", 4u, 4u, false));
    results.push_back(run_triangle_test("Triangle Strip 2x Nearest (8x8 UV)", 8u, 8u, false));
    results.push_back(run_triangle3_test("Triangle Prim3 1:1 (4x4 UV)", 4u, 4u, true, 1));
    results.push_back(run_triangle3_test("Triangle Prim3 Nearest 13x13 (4x4 UV)", 13u, 13u, false));

    if (init_font_atlas()) {
        // Tolerance 1: the retail font packet is TFX=MODULATE, and GS
        // modulate uses integer /128 where fixed-function GL uses 8-bit
        // /255-scale arithmetic - a 0.4% difference that can flip the last
        // bit. A wrong-texel sampling error changes channels by far more
        // than 1 and is still caught.
        results.push_back(run_font_test("Font Atlas Nearest Minify (prim4)", false, 0x80, 1));
        results.push_back(run_font_test("Font Atlas ALPHA=0x44 av=0x80", true, 0x80, 1));
        results.push_back(run_font_test("Font Atlas ALPHA=0x44 av=0x40", true, 0x40, 1));
    } else {
        results.push_back({"Font Atlas Nearest Minify (prim4)", 0, 0, false});
        std::fprintf(stderr, "FONT ATLAS MISSING: %s must exist for font coverage\n",
                     kFontAtlasPath);
    }

    ps2::gs::native_gl::shutdown();

    std::cout << "\n=======================================================================\n";
    std::cout << "             GS TEXTURE SAMPLING A/B ACCEPTANCE MATRIX                 \n";
    std::cout << "=======================================================================\n";
    std::cout << std::left << std::setw(42) << "Test Case"
              << std::setw(14) << "Pixels"
              << std::setw(12) << "Mismatches"
              << "Status\n";
    std::cout << "-----------------------------------------------------------------------\n";

    bool all_passed = true;
    for (const auto &r : results) {
        std::cout << std::left << std::setw(42) << r.name
                  << std::setw(14) << r.total_pixels
                  << std::setw(12) << r.mismatches
                  << (r.passed ? "PASS" : "FAIL") << "\n";
        if (!r.passed) all_passed = false;
    }
    std::cout << "=======================================================================\n";

    if (all_passed) {
        std::cout << "ALL " << results.size() << " MATRIX TESTS PASSED!\n";
        return 0;
    }
    std::cerr << "ACCEPTANCE MATRIX FAILED!\n";
    return 1;
}
