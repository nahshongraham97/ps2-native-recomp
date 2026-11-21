#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace mka::retail {

// These sizes describe the external retail card image only. Native profile
// state must be lifted into typed owners before a Main/profile binding passes.
inline constexpr std::size_t retail_profile_image_size = 0x1e1e0;
inline constexpr std::size_t retail_profile_image_prefix_size = 0x58;
inline constexpr std::size_t retail_profile_child_count = 8;
inline constexpr std::size_t retail_profile_child_image_size = 0x3c30;
inline constexpr std::size_t retail_profile_image_tail_size = 4;
inline constexpr std::size_t retail_profile_image_footer_size = 4;
static_assert(retail_profile_image_prefix_size +
    retail_profile_child_count * retail_profile_child_image_size +
    retail_profile_image_tail_size + retail_profile_image_footer_size ==
    retail_profile_image_size);

enum class RetailProfileImageFooterMode {
    ordinary_checksum, source_mode_one_zero
};

enum class RetailProfileImageError {
    none, invalid_image_size, invalid_prefix_size, invalid_child_size,
    invalid_tail_size, invalid_footer_mode, checksum_mismatch, allocation,
    native_exception
};

struct RetailProfileImageFailure {
    RetailProfileImageError kind{RetailProfileImageError::none};
    const char* operation{""};
    std::size_t child_index{};
};

struct RetailProfileImageEncodedParts {
    std::span<const std::uint8_t> prefix;
    std::array<std::span<const std::uint8_t>, retail_profile_child_count> children;
    std::span<const std::uint8_t> tail_before_footer;
};

// Exact 2229E0 byte/mask sum; the 32-bit ADDU result wraps by definition.
std::uint32_t retail_profile_checksum_2229e0(
    std::span<const std::uint8_t>) noexcept;

// Owns encoded *file bytes*, not a runtime guest-layout record. Views returned
// from an lvalue are borrowed and expire when this owner moves or retires.
class NativeRetailProfileImage final {
public:
    NativeRetailProfileImage(const NativeRetailProfileImage&) = delete;
    NativeRetailProfileImage& operator=(const NativeRetailProfileImage&) = delete;
    NativeRetailProfileImage(NativeRetailProfileImage&&) noexcept = default;
    NativeRetailProfileImage& operator=(NativeRetailProfileImage&&) noexcept = default;

    // 222A40's ordinary card-read validation requires a matching checksum.
    // A mode-one zero-footer image is not accepted by this read operation.
    static std::optional<NativeRetailProfileImage> decode_card_read_222a40(
        std::span<const std::uint8_t>, RetailProfileImageFailure&) noexcept;

    // 222DA0's two write modes preserve every supplied encoded body byte.
    // Mode one sets the final four bytes to zero; ordinary mode writes the
    // 2229E0 checksum. The input spans are external encoded parts only.
    static std::optional<NativeRetailProfileImage> encode_card_write_222da0(
        const RetailProfileImageEncodedParts&,
        RetailProfileImageFooterMode,
        RetailProfileImageFailure&) noexcept;

    std::span<const std::uint8_t> encoded_bytes() const & noexcept;
    std::span<const std::uint8_t> encoded_bytes() const && = delete;
    RetailProfileImageEncodedParts encoded_parts() const & noexcept;
    RetailProfileImageEncodedParts encoded_parts() const && = delete;
    RetailProfileImageFooterMode footer_mode() const noexcept { return mode_; }

private:
    NativeRetailProfileImage(std::vector<std::uint8_t>&&,
        RetailProfileImageFooterMode) noexcept;
    std::vector<std::uint8_t> bytes_;
    RetailProfileImageFooterMode mode_;
};

} // namespace mka::retail
