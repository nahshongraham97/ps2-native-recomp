// A/B harness: the reference software rasterizer (raster::draw) against the
// native OpenGL path (native_gl::submit) on identical GS state and vertices.
//
// The isolation run showed the pre-native-GL renderer (gpu/gpu.cpp) draws the
// front-end text correctly while the native GL path does not. raster.cpp is
// the in-tree reference implementation of the GS sampling rules, so drawing
// the same sprite through both and diffing the pixels isolates the
// difference to the GL path instead of to shared state handling.

#include "gs/gs.hpp"
#include "gs/native_gl_backend.hpp"
#include "gs/raster.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::uint32_t kFrameBase = 0x5Bu;   // FRAME.FBP, 8192-byte pages
constexpr std::uint32_t kFrameWidth = 10u;     // FRAME.FBW, 64-pixel units
constexpr std::uint32_t kTextureBase = 0x300u; // TBP0, 256-byte units
constexpr std::uint32_t kTextureWidth = 4u;    // texels
constexpr std::uint32_t kSpriteX = 40u, kSpriteY = 40u, kSpriteSize = 8u;

ps2::gs::Vertex vertex(std::uint32_t x, std::uint32_t y,
                       std::uint32_t u, std::uint32_t v) {
    ps2::gs::Vertex vertex{};
    vertex.x = x * 16u;
    vertex.y = y * 16u;
    vertex.z = 0;
    vertex.r = 255;
    vertex.g = 255;
    vertex.b = 255;
    vertex.a = 255;
    vertex.q = 1.0f;
    vertex.u = u * 16u;
    vertex.v = v * 16u;
    return vertex;
}

} // namespace

int main() {
    _putenv_s("PS2_NATIVE_GL", "1");
    _putenv_s("PS2_GS", "native-gl");
    ps2::gs::init();

    // A four-by-four texture whose texels are all distinct, so any wrong
    // texel, wrong scale or wrong filter shows up in the diff.
    for (std::uint32_t y = 0; y < kTextureWidth; ++y) {
        for (std::uint32_t x = 0; x < kTextureWidth; ++x) {
            const std::uint32_t value = 0xFF000000u | (y << 16) | (x << 8) | (0x40u + x * 0x10u + y);
            ps2::gs::write_pixel(ps2::gs::PSMCT32, kTextureBase, 1u, x, y, value);
        }
    }

    ps2::gs::State state{};
    state.prmodecont = 1;
    state.colclamp = 1;
    state.ctx[0].frame = (static_cast<std::uint64_t>(kFrameWidth) << 16) | kFrameBase;
    state.ctx[0].scissor = (static_cast<std::uint64_t>(639u) << 16) |
                            (static_cast<std::uint64_t>(447u) << 48);
    // TBP0, TBW=1, PSM=CT32, TW=2, TH=2, TCC=1, TFX=DECAL (texture colour only).
    state.ctx[0].tex0 = kTextureBase | (1ull << 14) | (2ull << 26) | (2ull << 30) |
                        (1ull << 34) | (1ull << 35);
    // MMIN=1 (linear), MMAG=1 (linear).
    state.ctx[0].tex1 = (1ull << 6) | (1ull << 5);
    // WMS=CLAMP, WMT=CLAMP.
    state.ctx[0].clamp = 1u | (1u << 2);
    state.ctx[0].test = 0;
    state.ctx[0].zbuf = 0;
    state.ctx[0].alpha = 0;

    // The decode reads the texture state, which includes the primitive's TME
    // bit, so set the primitive up before decoding.
    state.prim = 6u | (1u << 4) | (1u << 8);

    // What the decoder actually produces for that texture, against what was
    // written. A mismatch here is a decode bug, not a sampling bug.
    {
        std::vector<std::uint32_t> decoded;
        std::uint32_t dw = 0, dh = 0;
        if (!ps2::gs::raster::decode_texture_rgba(state, decoded, dw, dh)) {
            std::fprintf(stderr, "decode_texture_rgba failed\n");
            return 3;
        }
        std::fprintf(stderr, "decoded texture %ux%u:\n", dw, dh);
        for (std::uint32_t y = 0; y < dh; ++y) {
            std::fprintf(stderr, "  ");
            for (std::uint32_t x = 0; x < dw; ++x)
                std::fprintf(stderr, " %08X", decoded[y * dw + x]);
            std::fprintf(stderr, "\n");
        }
    }

    // Sprite (primitive type 6) with texture mapping and UV texel coordinates.
    state.prim = 6u | (1u << 4) | (1u << 8);
    const ps2::gs::Vertex vertices[2] = {
        vertex(kSpriteX, kSpriteY, 0u, 0u),
        vertex(kSpriteX + kSpriteSize, kSpriteY + kSpriteSize, kTextureWidth, kTextureWidth),
    };

    // Reference: the software rasterizer writes straight into GS memory.
    ps2::gs::raster::draw(state, vertices, 2);
    std::uint32_t reference[kSpriteSize * kSpriteSize] = {};
    for (std::uint32_t y = 0; y < kSpriteSize; ++y)
        for (std::uint32_t x = 0; x < kSpriteSize; ++x)
            reference[y * kSpriteSize + x] = ps2::gs::read_pixel(
                ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                kSpriteX + x, kSpriteY + y);

    // Clear the frame buffer so the GL readback cannot be confused with the
    // reference result.
    for (std::uint32_t y = 0; y < kSpriteSize + 2u; ++y)
        for (std::uint32_t x = 0; x < kSpriteSize + 2u; ++x)
            ps2::gs::write_pixel(ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                                  kSpriteX + x - 1u, kSpriteY + y - 1u, 0u);

    if (!ps2::gs::native_gl::submit(state, vertices, 2)) {
        std::fprintf(stderr, "native GL rejected the sprite\n");
        ps2::gs::native_gl::shutdown();
        return 1;
    }
    ps2::gs::native_gl::flush();

    std::uint32_t mismatches = 0;
    for (std::uint32_t y = 0; y < kSpriteSize; ++y) {
        for (std::uint32_t x = 0; x < kSpriteSize; ++x) {
            const std::uint32_t got = ps2::gs::read_pixel(
                ps2::gs::PSMCT32, kFrameBase * 32u, kFrameWidth,
                kSpriteX + x, kSpriteY + y);
            const std::uint32_t want = reference[y * kSpriteSize + x];
            if (got == want) continue;
            if (mismatches < 24) {
                std::fprintf(stderr, "sprite pixel (%u,%u): reference %08X native-gl %08X\n",
                             x, y, want, got);
            }
            ++mismatches;
        }
    }
    ps2::gs::native_gl::shutdown();
    if (mismatches) {
        std::fprintf(stderr, "%u of %u sprite pixels differ between the software rasterizer and native GL\n",
                     mismatches, kSpriteSize * kSpriteSize);
        return 2;
    }
    std::fprintf(stderr, "sprite pixels agree between the software rasterizer and native GL\n");
    return 0;
}
