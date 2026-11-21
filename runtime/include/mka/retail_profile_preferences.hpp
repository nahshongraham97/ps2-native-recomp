#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace mka::retail {

// Game preference values. Five indexed values have actual LWC1/SWC1 consumers;
// the remaining source bitwords keep exact values without guessed meanings.
// This type has native layout, not an88-byte mapped record or guest pointer.
struct RetailProfilePreferenceValues {
    std::array<float, 5> levels{};
    std::uint32_t word_14{}, word_18{}, word_1c{}, word_20{}, word_24{};
    std::uint32_t word_28{}, word_2c{}, word_30{}, word_34{}, word_38{};
    std::uint32_t word_3c{}, word_40{}, word_44{}, word_48{}, word_4c{};
    std::uint32_t word_50{}, word_54{};
};

RetailProfilePreferenceValues retail_profile_preference_defaults_from_2955d0() noexcept;

// Only the external retail save-image prefix has this fixed encoded size.
// No runtime caller can address a value by a guest offset through this API.
using RetailProfilePreferenceImage = std::array<std::uint8_t, 0x58>;
RetailProfilePreferenceImage encode_retail_profile_preferences(
    const RetailProfilePreferenceValues&) noexcept;
RetailProfilePreferenceValues decode_retail_profile_preferences(
    std::span<const std::uint8_t, 0x58>) noexcept;

} // namespace mka::retail
