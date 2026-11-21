#pragma once

#include "mka/art_section_data.hpp"

namespace mka::content {
enum class IndexedSpriteError {
    none, truncated, unsupported_layout, malformed_transfer, host_capacity,
    allocation_failed, native_exception
};

struct IndexedSpriteFailure {
    IndexedSpriteError kind{IndexedSpriteError::none};
};

using StoredSpritePixel = std::array<std::byte, 4>;

class NativeIndexedSpriteData final {
public:
    std::size_t width() const noexcept { return width_; }
    std::size_t height() const noexcept { return height_; }
    // Row-major stored R, G, B, A bytes. Alpha is not normalized for display.
    std::span<const StoredSpritePixel> pixels() const & noexcept { return pixels_; }
    std::span<const StoredSpritePixel> pixels() const && = delete;
private:
    friend std::optional<NativeIndexedSpriteData> decode_indexed_sprite(
        const ArtSpriteData&, IndexedSpriteFailure&) noexcept;
    NativeIndexedSpriteData(std::size_t width, std::size_t height,
        std::vector<StoredSpritePixel> pixels) noexcept;
    std::size_t width_, height_;
    std::vector<StoredSpritePixel> pixels_;
};

// Verified 16x16 mode-2 indexed sprite stream only. This is format coverage,
// not a texture/resource cap or a renderer/device binding.
std::optional<NativeIndexedSpriteData> decode_indexed_sprite(
    const ArtSpriteData&, IndexedSpriteFailure&) noexcept;
} // namespace mka::content
