#pragma once

#include "gs.hpp"

#include <vector>

namespace ps2::gs::raster {

void reset();
// Pixel history for the draw trace (gs.cpp): with PS2_TRACE_PIXEL=<x>,<y>, every primitive of a traced frame that reaches
// that pixel of whatever buffer it draws to prints what happened there (software renderer only: PS2_GS=cpu).
struct Probe {
    bool on = false;
    std::uint64_t frame = 0, number = 0;
};
extern Probe g_probe;
void draw(const State &s, const Vertex *v, int count);
bool decode_texture_rgba(const State &s, std::vector<std::uint32_t> &pixels, std::uint32_t &width, std::uint32_t &height);

// The texture's base in VRAM (256-byte units) for the state, or 0xFFFFFFFF
// when the state has no texture.
std::uint32_t texture_base(const State &s);

// The parameters a decoded texture needs, so the native path can push a
// guest's write into the native GL texture the write belongs to.
struct TextureParams {
    std::uint32_t psm = 0, tbp = 0, tbw = 0;
    std::uint32_t cpsm = 0, csa = 0, ta0 = 0;
    bool aem = false;
    const std::uint32_t *clut = nullptr;
};

// Decodes a rectangle of a texture into RGBA, the same way
// decode_texture_rgba decodes the whole texture.
bool decode_rect_rgba(const TextureParams &p, std::uint32_t x, std::uint32_t y,
                      std::uint32_t w, std::uint32_t h, std::vector<std::uint32_t> &pixels);

}
