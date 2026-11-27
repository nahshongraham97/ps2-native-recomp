#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mka::retail {
enum class RetailSsfTextureError {
    none, truncated, malformed_wrapper, malformed_chunk, unsupported_version,
    unsupported_raster_profile, unsupported_transfer, invalid_extent,
    allocation, native_exception
};
struct RetailSsfTextureFailure {
    RetailSsfTextureError kind{RetailSsfTextureError::none};
    const char* operation{""};
};
struct RetailSsfTextureData {
    std::string name;
    std::uint32_t width{}, height{}, sampler_metadata{};
    // Original component values, including alpha, are retained unchanged.
    // A rendering/effect policy is a separate caller obligation.
    std::vector<std::uint8_t> indices, rgba_raw_alpha;
    std::size_t backing_offset{}, backing_bytes{}, pixel_header{}, palette_header{};
};
// Decodes the source3015E0/229180/229420 mode2 PAL8+CT32-transfer profile.
// payload_offset_in_section establishes serialized128-byte alignment. It is
// a file offset, never an original address or the address of the host buffer.
// Other raster/transfer/compression profiles refuse by name. No fake TXD root.
std::optional<RetailSsfTextureData> decode_retail_ssf_texture(
    std::span<const std::byte>, std::uint64_t payload_offset_in_section,
    RetailSsfTextureFailure&) noexcept;
} // namespace mka::retail
