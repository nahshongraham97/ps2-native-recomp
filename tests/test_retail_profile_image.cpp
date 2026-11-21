#include "mka/retail_profile_image.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace {

void require(bool yes, const char* label) {
    if (!yes) {
        std::cerr << "FAIL retail profile image: " << label << '\n';
        std::exit(1);
    }
}

std::uint32_t footer(std::span<const std::uint8_t> bytes) {
    const auto i = bytes.size() - 4;
    return std::uint32_t{bytes[i]} |
        (std::uint32_t{bytes[i + 1]} << 8) |
        (std::uint32_t{bytes[i + 2]} << 16) |
        (std::uint32_t{bytes[i + 3]} << 24);
}

} // namespace

int main() {
    using namespace mka::retail;

    // Independently calculated from the original 2229E0 LBU/OR/ANDI/ADDU
    // words: mask starts at 0x80, halves to 0x01 and restarts at 0x80.
    const std::array<std::uint8_t, 8> zeros{};
    const std::array<std::uint8_t, 9> nine_zeros{};
    const std::array<std::uint8_t, 16> ascending{
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const std::array<std::uint8_t, 4> mixed{0x80, 1, 0x7f, 0};
    const std::array<std::uint8_t, 8> ones{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    require(retail_profile_checksum_2229e0({}) == 0, "empty length");
    require(retail_profile_checksum_2229e0(zeros) == 0xff, "eight mask cycle");
    require(retail_profile_checksum_2229e0(nine_zeros) == 0x17f,
            "ninth mask restart");
    require(retail_profile_checksum_2229e0(ascending) == 0x260,
            "ascending sixteen bytes");
    require(retail_profile_checksum_2229e0(mixed) == 0x150,
            "OR rather than addition or XOR");
    require(retail_profile_checksum_2229e0(ones) == 0x7f8,
            "all bits already set");

    std::vector<std::uint8_t> prefix(retail_profile_image_prefix_size);
    std::array<std::vector<std::uint8_t>, retail_profile_child_count> children;
    for (auto& child : children) child.resize(retail_profile_child_image_size);
    std::array<std::uint8_t, retail_profile_image_tail_size> tail{};
    RetailProfileImageEncodedParts parts{};
    parts.prefix = prefix;
    for (std::size_t i = 0; i < children.size(); ++i)
        parts.children[i] = children[i];
    parts.tail_before_footer = tail;
    RetailProfileImageFailure failure;

    auto zero_image = NativeRetailProfileImage::encode_card_write_222da0(
        parts, RetailProfileImageFooterMode::ordinary_checksum, failure);
    require(zero_image && failure.kind == RetailProfileImageError::none,
            "ordinary all-zero encoded image");
    require(zero_image->encoded_bytes().size() == retail_profile_image_size &&
            footer(zero_image->encoded_bytes()) == 0x003bffb5,
            "original full-image zero vector and little-endian footer");
    auto zero_read = NativeRetailProfileImage::decode_card_read_222a40(
        zero_image->encoded_bytes(), failure);
    require(zero_read && failure.kind == RetailProfileImageError::none,
            "ordinary card read validates");
    require(zero_read->encoded_bytes().data() != zero_image->encoded_bytes().data(),
            "decoded image owns its file bytes");

    // Fill the body with absolute file offsets modulo 256. The expected
    // 0x010D1C6A checksum is a fixed vector derived from the original words,
    // independent of the codec's encoder and decoder.
    std::size_t position = 0;
    for (auto& byte : prefix) byte = static_cast<std::uint8_t>(position++);
    for (auto& child : children)
        for (auto& byte : child) byte = static_cast<std::uint8_t>(position++);
    for (auto& byte : tail) byte = static_cast<std::uint8_t>(position++);
    require(position == retail_profile_image_size - 4, "external layout arithmetic");
    auto patterned = NativeRetailProfileImage::encode_card_write_222da0(
        parts, RetailProfileImageFooterMode::ordinary_checksum, failure);
    require(patterned && footer(patterned->encoded_bytes()) == 0x010d1c6a,
            "ascending full-image source vector");
    auto body = patterned->encoded_bytes();
    for (std::size_t i = 0; i < body.size() - 4; ++i)
        require(body[i] == static_cast<std::uint8_t>(i), "every input byte retained");
    auto views = patterned->encoded_parts();
    require(views.prefix.size() == 0x58 && views.prefix.front() == 0 &&
            views.prefix.back() == 0x57 &&
            views.children.front().size() == 0x3c30 &&
            views.children.front().front() == 0x58 &&
            views.children.back().back() ==
                static_cast<std::uint8_t>(retail_profile_image_size - 9) &&
            views.tail_before_footer.size() == 4 &&
            views.tail_before_footer.back() ==
                static_cast<std::uint8_t>(retail_profile_image_size - 5),
            "validated encoded spans preserve boundaries");
    auto patterned_read = NativeRetailProfileImage::decode_card_read_222a40(
        patterned->encoded_bytes(), failure);
    require(patterned_read && patterned_read->encoded_bytes().size() == body.size(),
            "patterned card read");
    for (std::size_t i = 0; i < body.size(); ++i)
        require(patterned_read->encoded_bytes()[i] == body[i],
                "decode preserves full encoded byte image");

    auto temporary = NativeRetailProfileImage::encode_card_write_222da0(
        parts, RetailProfileImageFooterMode::source_mode_one_zero, failure);
    require(temporary && temporary->footer_mode() ==
            RetailProfileImageFooterMode::source_mode_one_zero &&
            footer(temporary->encoded_bytes()) == 0,
            "source write-mode one zero footer");
    require(!NativeRetailProfileImage::decode_card_read_222a40(
                temporary->encoded_bytes(), failure) &&
            failure.kind == RetailProfileImageError::checksum_mismatch,
            "source ordinary read refuses mode-one zero footer");

    auto damaged = std::vector<std::uint8_t>(body.begin(), body.end());
    damaged[retail_profile_image_prefix_size + 0x29] ^= 1;
    require(!NativeRetailProfileImage::decode_card_read_222a40(damaged, failure) &&
            failure.kind == RetailProfileImageError::checksum_mismatch,
            "corrupted child byte rejected");
    damaged.assign(body.begin(), body.end());
    damaged.back() ^= 1;
    require(!NativeRetailProfileImage::decode_card_read_222a40(damaged, failure) &&
            failure.kind == RetailProfileImageError::checksum_mismatch,
            "corrupted footer rejected");
    require(!NativeRetailProfileImage::decode_card_read_222a40(
                body.first(body.size() - 1), failure) &&
            failure.kind == RetailProfileImageError::invalid_image_size,
            "truncated card image rejected before checksum");
    damaged.assign(body.begin(), body.end());
    damaged.push_back(0);
    require(!NativeRetailProfileImage::decode_card_read_222a40(damaged, failure) &&
            failure.kind == RetailProfileImageError::invalid_image_size,
            "overlong card image rejected");

    auto invalid = parts;
    invalid.prefix = parts.prefix.first(parts.prefix.size() - 1);
    require(!NativeRetailProfileImage::encode_card_write_222da0(
                invalid, RetailProfileImageFooterMode::ordinary_checksum,
                failure) && failure.kind == RetailProfileImageError::invalid_prefix_size,
            "invalid prefix refused");
    invalid = parts;
    invalid.children[3] = parts.children[3].first(
        parts.children[3].size() - 1);
    require(!NativeRetailProfileImage::encode_card_write_222da0(
                invalid, RetailProfileImageFooterMode::ordinary_checksum,
                failure) && failure.kind == RetailProfileImageError::invalid_child_size &&
            failure.child_index == 3, "invalid child refused by index");
    invalid = parts;
    invalid.tail_before_footer = parts.tail_before_footer.first(3);
    require(!NativeRetailProfileImage::encode_card_write_222da0(
                invalid, RetailProfileImageFooterMode::ordinary_checksum,
                failure) && failure.kind == RetailProfileImageError::invalid_tail_size,
            "invalid tail refused");
    require(!NativeRetailProfileImage::encode_card_write_222da0(
                parts, static_cast<RetailProfileImageFooterMode>(7), failure) &&
            failure.kind == RetailProfileImageError::invalid_footer_mode,
            "unknown footer mode refused");
    require(NativeRetailProfileImage::decode_card_read_222a40(
                patterned->encoded_bytes(), failure) &&
            failure.kind == RetailProfileImageError::none,
            "successful retry clears stale failure");

    std::cout << "PASS native retail profile card image codec; typed profile fields and saved-game acceptance open\n";
}
