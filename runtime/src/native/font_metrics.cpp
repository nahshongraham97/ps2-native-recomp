#include "mka/font_metrics.hpp"
#include <bit>
#include <cmath>
#include <new>
#include <stdexcept>
#if defined(__FAST_MATH__)
#error "font_metrics requires finite source float32 validation; disable fast-math"
#endif

namespace mka::text {
struct FontMetricsStorage {
    std::vector<std::uint8_t> bytes;
    std::array<float, 5> header;
    std::vector<FontGlyphMetrics> glyphs;
};
namespace {
constexpr std::size_t header_bytes = 52, record_bytes = 36;
float le_float(const std::vector<std::uint8_t>& b, std::size_t at) {
    const auto word = std::uint32_t(b[at]) | (std::uint32_t(b[at+1]) << 8) |
                      (std::uint32_t(b[at+2]) << 16) | (std::uint32_t(b[at+3]) << 24);
    return std::bit_cast<float>(word);
}
}
const char* font_metrics_error_name(FontMetricsError e) noexcept {
    switch (e) {
#define MKA_FONT_ERROR(x) case FontMetricsError::x: return #x;
    MKA_FONT_ERROR(none) MKA_FONT_ERROR(moved_metrics) MKA_FONT_ERROR(truncated_header)
    MKA_FONT_ERROR(partial_glyph_record) MKA_FONT_ERROR(nonfinite_value)
    MKA_FONT_ERROR(missing_glyph) MKA_FONT_ERROR(allocation_failed) MKA_FONT_ERROR(host_capacity)
#undef MKA_FONT_ERROR
    }
    return "unknown_font_metrics_error";
}
std::optional<NativeFontMetrics> NativeFontMetrics::parse(std::vector<std::uint8_t> bytes,
                                                         FontMetricsFailure& f) {
    f = {};
    if (bytes.size() < header_bytes) { f = {FontMetricsError::truncated_header, bytes.size()}; return {}; }
    if ((bytes.size() - header_bytes) % record_bytes) {
        f = {FontMetricsError::partial_glyph_record, header_bytes +
             ((bytes.size() - header_bytes) / record_bytes) * record_bytes}; return {};
    }
    // Validate all source floats before publishing an owner. Header label is
    // opaque fixed-width data; no required NUL or guessed font-name match.
    for (std::size_t at = 32; at < bytes.size(); at += 4)
        if (!std::isfinite(le_float(bytes, at))) { f = {FontMetricsError::nonfinite_value, at}; return {}; }
    try {
        auto s = std::make_shared<FontMetricsStorage>(); s->bytes = std::move(bytes);
        for (std::size_t i=0; i<s->header.size(); ++i) s->header[i] = le_float(s->bytes, 32+i*4);
        const auto count = (s->bytes.size() - header_bytes) / record_bytes;
        s->glyphs.reserve(count);
        for (std::size_t i=0; i<count; ++i) {
            const auto at = header_bytes + i * record_bytes;
            s->glyphs.push_back({le_float(s->bytes, at), le_float(s->bytes, at+4),
                le_float(s->bytes, at+8), le_float(s->bytes, at+12),
                le_float(s->bytes, at+16), le_float(s->bytes, at+20),
                le_float(s->bytes, at+24), le_float(s->bytes, at+28), le_float(s->bytes, at+32)});
        }
        return NativeFontMetrics(std::move(s));
    } catch (const std::bad_alloc&) { f = {FontMetricsError::allocation_failed, 0}; }
      catch (const std::length_error&) { f = {FontMetricsError::host_capacity, 0}; }
    return {};
}
std::span<const std::uint8_t> NativeFontMetrics::raw_bytes() const noexcept {
    return storage_ ? std::span<const std::uint8_t>(storage_->bytes) : std::span<const std::uint8_t>{};
}
std::span<const std::uint8_t> NativeFontMetrics::label_bytes() const noexcept {
    return storage_ ? std::span<const std::uint8_t>(storage_->bytes).first(32) : std::span<const std::uint8_t>{};
}
std::span<const float> NativeFontMetrics::header_values() const noexcept {
    return storage_ ? std::span<const float>(storage_->header) : std::span<const float>{};
}
std::size_t NativeFontMetrics::glyph_count() const noexcept { return storage_ ? storage_->glyphs.size() : 0; }
std::optional<FontGlyphMetrics> NativeFontMetrics::glyph(std::uint64_t c, FontMetricsFailure& f) const noexcept {
    f = {};
    if (!storage_) { f = {FontMetricsError::moved_metrics, 0}; return {}; }
    if (c < 32 || c-32 >= storage_->glyphs.size()) { f = {FontMetricsError::missing_glyph, 0}; return {}; }
    return storage_->glyphs[static_cast<std::size_t>(c-32)];
}
std::optional<float> NativeFontMetrics::advance(std::uint64_t c, FontMetricsFailure& f) const noexcept {
    f = {};
    if (!storage_) { f = {FontMetricsError::moved_metrics, 0}; return {}; }
    if (c == 32) return storage_->header[4];
    const auto g = glyph(c, f); return g ? std::optional<float>(g->advance) : std::nullopt;
}
} // namespace mka::text
