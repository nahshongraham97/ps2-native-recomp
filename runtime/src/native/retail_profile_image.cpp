#include "mka/retail_profile_image.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {

void fail(RetailProfileImageFailure& failure, RetailProfileImageError kind,
          const char* operation, std::size_t child_index = 0) noexcept {
    failure.kind = kind;
    failure.operation = operation;
    failure.child_index = child_index;
}

std::uint32_t read_le32(std::span<const std::uint8_t> bytes,
                        std::size_t offset) noexcept {
    return std::uint32_t{bytes[offset]} |
        (std::uint32_t{bytes[offset + 1]} << 8) |
        (std::uint32_t{bytes[offset + 2]} << 16) |
        (std::uint32_t{bytes[offset + 3]} << 24);
}

void write_le32(std::span<std::uint8_t> bytes, std::size_t offset,
                std::uint32_t value) noexcept {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

} // namespace

std::uint32_t retail_profile_checksum_2229e0(
    std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t sum = 0;
    std::uint8_t mask = 0x80;
    for (const auto byte : bytes) {
        // LBU, OR, ANDI 0xFF, then 32-bit ADDU at 222A14.
        sum += static_cast<std::uint8_t>(byte | mask);
        mask = static_cast<std::uint8_t>(mask >> 1);
        if (mask == 0) mask = 0x80;
    }
    return sum;
}

NativeRetailProfileImage::NativeRetailProfileImage(
    std::vector<std::uint8_t>&& bytes,
    RetailProfileImageFooterMode mode) noexcept
    : bytes_(std::move(bytes)), mode_(mode) {}

std::optional<NativeRetailProfileImage>
NativeRetailProfileImage::decode_card_read_222a40(
    std::span<const std::uint8_t> encoded,
    RetailProfileImageFailure& failure) noexcept {
    failure = {};
    if (encoded.size() != retail_profile_image_size) {
        fail(failure, RetailProfileImageError::invalid_image_size,
             "222a40_image_size");
        return std::nullopt;
    }
    const auto expected = retail_profile_checksum_2229e0(
        encoded.first(retail_profile_image_size - retail_profile_image_footer_size));
    const auto stored = read_le32(encoded,
        retail_profile_image_size - retail_profile_image_footer_size);
    if (stored != expected) {
        fail(failure, RetailProfileImageError::checksum_mismatch,
             "222a40_checksum");
        return std::nullopt;
    }
    try {
        std::vector<std::uint8_t> owned(encoded.begin(), encoded.end());
        NativeRetailProfileImage image(std::move(owned),
            RetailProfileImageFooterMode::ordinary_checksum);
        return std::move(image);
    } catch (const std::bad_alloc&) {
        fail(failure, RetailProfileImageError::allocation, "222a40_copy");
    } catch (const std::length_error&) {
        fail(failure, RetailProfileImageError::allocation, "222a40_copy");
    } catch (...) {
        fail(failure, RetailProfileImageError::native_exception, "222a40_copy");
    }
    return std::nullopt;
}

std::optional<NativeRetailProfileImage>
NativeRetailProfileImage::encode_card_write_222da0(
    const RetailProfileImageEncodedParts& parts,
    RetailProfileImageFooterMode mode,
    RetailProfileImageFailure& failure) noexcept {
    failure = {};
    if (parts.prefix.size() != retail_profile_image_prefix_size) {
        fail(failure, RetailProfileImageError::invalid_prefix_size,
             "222da0_prefix_size");
        return std::nullopt;
    }
    for (std::size_t i = 0; i < parts.children.size(); ++i) {
        if (parts.children[i].size() != retail_profile_child_image_size) {
            fail(failure, RetailProfileImageError::invalid_child_size,
                 "222da0_child_size", i);
            return std::nullopt;
        }
    }
    if (parts.tail_before_footer.size() != retail_profile_image_tail_size) {
        fail(failure, RetailProfileImageError::invalid_tail_size,
             "222da0_tail_size");
        return std::nullopt;
    }
    if (mode != RetailProfileImageFooterMode::ordinary_checksum &&
        mode != RetailProfileImageFooterMode::source_mode_one_zero) {
        fail(failure, RetailProfileImageError::invalid_footer_mode,
             "222da0_footer_mode");
        return std::nullopt;
    }
    try {
        std::vector<std::uint8_t> bytes(retail_profile_image_size);
        auto out = bytes.begin();
        out = std::copy(parts.prefix.begin(), parts.prefix.end(), out);
        for (const auto child : parts.children)
            out = std::copy(child.begin(), child.end(), out);
        out = std::copy(parts.tail_before_footer.begin(),
            parts.tail_before_footer.end(), out);
        const auto footer = mode == RetailProfileImageFooterMode::ordinary_checksum
            ? retail_profile_checksum_2229e0(std::span<const std::uint8_t>(
                bytes.data(), retail_profile_image_size - retail_profile_image_footer_size))
            : 0u;
        write_le32(bytes,
            retail_profile_image_size - retail_profile_image_footer_size, footer);
        NativeRetailProfileImage image(std::move(bytes), mode);
        return std::move(image);
    } catch (const std::bad_alloc&) {
        fail(failure, RetailProfileImageError::allocation, "222da0_encode");
    } catch (const std::length_error&) {
        fail(failure, RetailProfileImageError::allocation, "222da0_encode");
    } catch (...) {
        fail(failure, RetailProfileImageError::native_exception, "222da0_encode");
    }
    return std::nullopt;
}

std::span<const std::uint8_t> NativeRetailProfileImage::encoded_bytes()
    const & noexcept {
    return bytes_;
}

RetailProfileImageEncodedParts NativeRetailProfileImage::encoded_parts()
    const & noexcept {
    RetailProfileImageEncodedParts parts{};
    if (bytes_.size() != retail_profile_image_size) return parts;
    auto view = std::span<const std::uint8_t>(bytes_);
    parts.prefix = view.first(retail_profile_image_prefix_size);
    std::size_t offset = retail_profile_image_prefix_size;
    for (auto& child : parts.children) {
        child = view.subspan(offset, retail_profile_child_image_size);
        offset += retail_profile_child_image_size;
    }
    parts.tail_before_footer = view.subspan(offset, retail_profile_image_tail_size);
    return parts;
}

} // namespace mka::retail
