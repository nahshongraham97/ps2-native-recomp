#include "mka/retail_profile_preferences.hpp"

#include <bit>
#include <limits>

namespace mka::retail {
static_assert(sizeof(float) == sizeof(std::uint32_t) &&
    std::numeric_limits<float>::is_iec559);

RetailProfilePreferenceValues retail_profile_preference_defaults_from_2955d0() noexcept {
    RetailProfilePreferenceValues out;
    out.levels.fill(std::bit_cast<float>(std::uint32_t{0x3f400000}));
    out.word_14 = 0x3f800000;
    out.word_18 = 2;
    out.word_1c = 2;
    out.word_20 = 2;
    out.word_24 = 90;
    out.word_28 = 3;
    out.word_2c = 1;
    out.word_3c = 50;
    out.word_40 = 50;
    out.word_44 = 50;
    out.word_48 = 50;
    out.word_4c = 50;
    out.word_50 = 85;
    out.word_54 = 0xffffffffu;
    return out;
}

RetailProfilePreferenceImage encode_retail_profile_preferences(
    const RetailProfilePreferenceValues& values) noexcept {
    RetailProfilePreferenceImage encoded{};
    std::size_t cursor = 0;
    const auto emit = [&](std::uint32_t word) {
        for (unsigned byte = 0; byte != 4; ++byte)
            encoded[cursor++] = static_cast<std::uint8_t>(word >> (byte * 8));
    };
    for (const auto level : values.levels) emit(std::bit_cast<std::uint32_t>(level));
    for (const auto word : std::array<std::uint32_t,17>{values.word_14, values.word_18, values.word_1c, values.word_20,
            values.word_24, values.word_28, values.word_2c, values.word_30, values.word_34,
            values.word_38, values.word_3c, values.word_40, values.word_44, values.word_48,
            values.word_4c, values.word_50, values.word_54}) emit(word);
    return encoded;
}

RetailProfilePreferenceValues decode_retail_profile_preferences(
    std::span<const std::uint8_t, 0x58> encoded) noexcept {
    std::size_t cursor = 0;
    const auto read = [&]() {
        std::uint32_t word = 0;
        for (unsigned byte = 0; byte != 4; ++byte)
            word |= std::uint32_t{encoded[cursor++]} << (byte * 8);
        return word;
    };
    RetailProfilePreferenceValues values;
    for (auto& level : values.levels) level = std::bit_cast<float>(read());
    for (auto* word : std::array<std::uint32_t*,17>{&values.word_14, &values.word_18, &values.word_1c, &values.word_20,
            &values.word_24, &values.word_28, &values.word_2c, &values.word_30, &values.word_34,
            &values.word_38, &values.word_3c, &values.word_40, &values.word_44, &values.word_48,
            &values.word_4c, &values.word_50, &values.word_54}) *word = read();
    return values;
}
} // namespace mka::retail
