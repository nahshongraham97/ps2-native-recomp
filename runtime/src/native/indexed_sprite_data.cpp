#include "mka/indexed_sprite_data.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::content {
namespace {
constexpr std::size_t asset_side = 16;
constexpr std::size_t index_bytes = asset_side * asset_side;
constexpr std::size_t palette_entries = 256;
constexpr std::size_t stored_pixel_bytes = 4;
constexpr std::size_t transfer_header_bytes = 80;
constexpr std::size_t alignment = 128;
constexpr std::size_t mode2_words_end = 164;
constexpr std::size_t mode2_leading_bytes = 0x100;

std::uint32_t le32(std::span<const std::byte> data, std::size_t offset) noexcept {
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(data[offset + i]) << (8 * i);
    return value;
}
bool supported_chunk(std::span<const std::byte> data, std::size_t offset,
                     std::uint32_t kind, std::uint32_t size) noexcept {
    const auto library = le32(data, offset + 8);
    const auto version = (library & 0xffff0000u)
        ? (((library >> 14) & 0x3ff00u) + 0x30000u) | ((library >> 16) & 0x3fu)
        : library << 8;
    return le32(data, offset) == kind && le32(data, offset + 4) == size &&
        version >= 0x34000u && version <= 0x36003u;
}
bool transfer_header(std::span<const std::byte> data, bool palette) noexcept {
    // Static serialized format records: check only this pair of known shapes.
    const auto side = palette ? 16u : 8u;
    const auto units = palette ? 64u : 16u;
    const std::array<std::uint32_t, 20> expected{
        3, 0x10000000, 14, 0,
        0, palette ? 0x00100030u : 0u, 0x51, 0,
        side, side, 0x52, 0,
        0, 0, 0x53, 0,
        units, 0x08000000, 0, 0};
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (le32(data, i * 4) != expected[i]) return false;
    return true;
}
std::size_t source_index(std::size_t x, std::size_t y) noexcept {
    const auto column_interleave = (((y >> 1) ^ (y >> 2)) & 1) * 4;
    return 64 * (y >> 2) + 32 * (y & 1) + 4 * ((x & 7) ^ column_interleave) +
        ((x & 8) >> 2) + ((y & 2) >> 1);
}
std::size_t palette_index(std::size_t index) noexcept {
    return (index & ~std::size_t{0x18}) | ((index & 8) << 1) | ((index & 16) >> 1);
}
} // namespace

NativeIndexedSpriteData::NativeIndexedSpriteData(std::size_t width, std::size_t height,
    std::vector<StoredSpritePixel> pixels) noexcept
    : width_(width), height_(height), pixels_(std::move(pixels)) {}

std::optional<NativeIndexedSpriteData> decode_indexed_sprite(
    const ArtSpriteData& sprite, IndexedSpriteFailure& failure) noexcept {
    failure = {};
    auto reject = [&failure](IndexedSpriteError kind) {
        failure.kind = kind;
        return std::optional<NativeIndexedSpriteData>{};
    };
    const auto encoded = sprite.encoded_bytes();
    if (encoded.size() < mode2_words_end) return reject(IndexedSpriteError::truncated);
    if (sprite.width() != static_cast<std::int32_t>(asset_side) ||
        sprite.height() != static_cast<std::int32_t>(asset_side) ||
        !supported_chunk(encoded, 0, 1, 8) || le32(encoded, 12) != 0x00325350 ||
        !supported_chunk(encoded, 20, 2, 4) || !supported_chunk(encoded, 36, 2, 4) ||
        !supported_chunk(encoded, 52, 1, 0x653) || !supported_chunk(encoded, 64, 1, 64) ||
        !supported_chunk(encoded, 140, 1, 0x5a0) ||
        le32(encoded, 76) != asset_side || le32(encoded, 80) != asset_side ||
        le32(encoded, 84) != 8 || le32(encoded, 88) != 0x00022504 ||
        le32(encoded, 92) != 0x11308000 || le32(encoded, 96) != 0x20000005 ||
        le32(encoded, 152) != 0x6c0 || le32(encoded, 156) != mode2_leading_bytes ||
        le32(encoded, 160) != 0x250)
        return reject(IndexedSpriteError::unsupported_layout);

    // Section-relative source cursor, never a native buffer address. The
    // type-1 lengths are not nesting bounds for the explicit mode-2 data.
    if (sprite.encoded_offset() > std::numeric_limits<std::size_t>::max() - mode2_words_end)
        return reject(IndexedSpriteError::host_capacity);
    const auto cursor = sprite.encoded_offset() + mode2_words_end;
    const auto padding = (alignment - cursor % alignment) % alignment;
    const auto mode2_offset = mode2_words_end + padding;
    const std::size_t mode2_size = le32(encoded, 152);
    if (mode2_offset > encoded.size() || mode2_size > encoded.size() - mode2_offset)
        return reject(IndexedSpriteError::truncated);
    const auto mode2 = encoded.subspan(mode2_offset, mode2_size);
    const auto first_header = mode2.subspan(mode2_leading_bytes, transfer_header_bytes);
    const auto indices = mode2.subspan(mode2_leading_bytes + transfer_header_bytes, index_bytes);
    const auto palette_header_offset = mode2_leading_bytes + transfer_header_bytes + index_bytes;
    const auto second_header = mode2.subspan(palette_header_offset, transfer_header_bytes);
    if (!transfer_header(first_header, false) || !transfer_header(second_header, true))
        return reject(IndexedSpriteError::malformed_transfer);
    const auto palette = mode2.subspan(palette_header_offset + transfer_header_bytes,
        palette_entries * stored_pixel_bytes);

    try {
        std::vector<StoredSpritePixel> pixels(index_bytes);
        for (std::size_t y = 0; y < asset_side; ++y)
            for (std::size_t x = 0; x < asset_side; ++x) {
                const auto index = std::to_integer<std::size_t>(indices[source_index(x, y)]);
                const auto stored = palette_index(index) * stored_pixel_bytes;
                std::copy_n(palette.begin() + stored, stored_pixel_bytes, pixels[y * asset_side + x].begin());
            }
        return NativeIndexedSpriteData(asset_side, asset_side, std::move(pixels));
    } catch (const std::length_error&) { return reject(IndexedSpriteError::host_capacity); }
      catch (const std::bad_alloc&) { return reject(IndexedSpriteError::allocation_failed); }
      catch (...) { return reject(IndexedSpriteError::native_exception); }
}
} // namespace mka::content
