#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace mka {
enum class FontAtlasError { none, truncated, malformed_wrapper, malformed_chunk,
    unsupported_variant, invalid_transfer, invalid_extent, allocation_failure };
const char* font_atlas_error_name(FontAtlasError) noexcept;
struct FontAtlasFailure { FontAtlasError code = FontAtlasError::none; std::string detail; };

// Immutable through this API. Source bytes and raw alpha are retained unchanged.
// This is data decoding only: no font selection, filtering, gain or composition.
class FontAtlas {
public:
    FontAtlas(const FontAtlas&) = default;
    FontAtlas(FontAtlas&&) noexcept = default;
    FontAtlas& operator=(FontAtlas other) noexcept { swap(other); return *this; }
    void swap(FontAtlas& other) noexcept {
        source_.swap(other.source_);indices_.swap(other.indices_);rgba_.swap(other.rgba_);
        std::swap(width_,other.width_);std::swap(height_,other.height_);
    }
    static std::optional<FontAtlas> decode(std::vector<std::uint8_t> serialized,
                                         FontAtlasFailure&);
    std::uint32_t width() const noexcept { return width_; }
    std::uint32_t height() const noexcept { return height_; }
    const std::vector<std::uint8_t>& serialized() const noexcept { return source_; }
    const std::vector<std::uint8_t>& indices() const noexcept { return indices_; }
    const std::vector<std::uint8_t>& rgba_raw_alpha() const noexcept { return rgba_; }
private:
    FontAtlas() = default;
    std::vector<std::uint8_t> source_, indices_, rgba_;
    std::uint32_t width_{},height_{};
};
} // namespace mka
