#pragma once
#include "mka/font_layout.hpp"
namespace mka::text {
enum class FontVerticalScale { one, fifteen_sixteenths };
struct FontGeometryConfig {
    // Separate mandatory identities: original screen globals, draw environment,
    // and actual decoded atlas. No equality, region or viewport default.
    std::uint64_t logical_screen_width, logical_screen_height;
    std::uint64_t draw_canvas_width, draw_canvas_height;
    std::uint64_t atlas_width, atlas_height;
    FontVerticalScale vertical_scale;
};
enum class FontGeometryError { none, invalid_extent, unsupported_scale,
    invalid_placement, nonfinite_metrics, host_overflow };
struct FontGeometryFailure { FontGeometryError kind=FontGeometryError::none; std::size_t source_offset=0; };
const char* font_geometry_error_name(FontGeometryError) noexcept;
struct FontGlyphGeometry {
    std::size_t source_offset;
    std::uint8_t source_character;
    std::int64_t x0,y0,x1,y1;
    // Signed integer sixteenths of an actual atlas texel, unchanged quantization.
    // No wrapping/clamping, normalized UV, half-pixel correction or sampling.
    std::int64_t u0_sixteenths,v0_sixteenths,u1_sixteenths,v1_sixteenths;
};
std::optional<FontGlyphGeometry> geometry_font_glyph(const FontGlyphPlacement&,
    const FontGeometryConfig&,FontGeometryFailure&) noexcept;
} // namespace mka::text
