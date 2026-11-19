#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace mka::text {
enum class FontMetricsError {
    none, moved_metrics, truncated_header, partial_glyph_record,
    nonfinite_value, missing_glyph, allocation_failed, host_capacity
};
struct FontMetricsFailure {
    FontMetricsError kind = FontMetricsError::none;
    std::size_t offset = 0; // owned serialized bytes, not an address
};
const char* font_metrics_error_name(FontMetricsError) noexcept;
struct FontGlyphMetrics {
    float width, height, bearing_x, bearing_y;
    float u0, u1, v0, v1, advance;
};
struct FontMetricsStorage;
// Serialized 3E4970 metrics: label[32], five LE float32 header fields,
// then nine LE float32 values per glyph. Count comes from the supplied bytes.
// Source character32 is record0; a larger native index does not imply Unicode
// transcoding. No font/atlas lookup, default glyph, layout or device state.
class NativeFontMetrics {
public:
    NativeFontMetrics(const NativeFontMetrics&) = default;
    NativeFontMetrics& operator=(const NativeFontMetrics&) = default;
    NativeFontMetrics(NativeFontMetrics&&) noexcept = default;
    NativeFontMetrics& operator=(NativeFontMetrics&&) noexcept = default;
    static std::optional<NativeFontMetrics> parse(std::vector<std::uint8_t> owned_bytes,
                                                 FontMetricsFailure&);
    std::span<const std::uint8_t> raw_bytes() const noexcept;
    std::span<const std::uint8_t> label_bytes() const noexcept;
    // Preserve unnamed header fields without inventing their semantics.
    std::span<const float> header_values() const noexcept;
    std::size_t glyph_count() const noexcept;
    std::optional<FontGlyphMetrics> glyph(std::uint64_t source_character,
                                          FontMetricsFailure&) const noexcept;
    // Original space uses header+0x30, not record0.advance. Other supported
    // characters use the glyph's horizontal advance; missing ones reject.
    std::optional<float> advance(std::uint64_t source_character,
                                 FontMetricsFailure&) const noexcept;
private:
    explicit NativeFontMetrics(std::shared_ptr<const FontMetricsStorage> s)
        : storage_(std::move(s)) {}
    std::shared_ptr<const FontMetricsStorage> storage_;
};
} // namespace mka::text
