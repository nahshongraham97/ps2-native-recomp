#pragma once
#include "mka/font_metrics.hpp"
#include <optional>
#include <span>
#include <vector>
namespace mka::text {
enum class FontLayoutError { none, unsupported_control, missing_glyph, invalid_metrics,
    nonprogress, host_overflow, allocation_failed };
const char* font_layout_error_name(FontLayoutError) noexcept;
struct FontLayoutFailure { FontLayoutError kind=FontLayoutError::none; std::size_t source_offset=0; };
struct FontLayoutConfig {
    std::uint64_t total_byte_budget, per_line_scan_budget;
    std::int64_t center, baseline, line_advance;
};
struct FontGlyphPlacement {
    std::size_t source_offset;
    std::uint8_t source_character;
    std::int64_t pen_x, baseline;
    FontGlyphMetrics metrics;
};
struct FontLayoutLine {
    std::size_t source_offset, selected_bytes, scanned_bytes;
    std::int64_t selected_width, pen_x, baseline;
    bool explicit_newline;
};
struct FontLayout {
    std::vector<FontGlyphPlacement> glyphs;
    std::vector<FontLayoutLine> lines;
    std::size_t consumed_bytes=0;
};
// Input is the counted source-byte text excluding any C-string terminator.
// No Unicode conversion, width wrap, space skipping, screen/region transform,
// UV transport or rendering. Results own all copied metrics/placement records.
std::optional<FontLayout> layout_font_bytes(const NativeFontMetrics&,
    std::span<const std::uint8_t>,const FontLayoutConfig&,FontLayoutFailure&);
} // namespace mka::text
