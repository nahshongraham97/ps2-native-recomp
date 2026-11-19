// Pure table admission/lookup fixture. No player, gain coefficient or device.
#include "mka/movie_gain.hpp"
#include <bit>
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>

using namespace mka::video;
namespace {
void check(bool value, const char* label) {
    if (!value) throw std::runtime_error(label);
}
void check_error(const std::optional<std::int32_t>& value,
                 const MovieGainFailure& failure, MovieGainError expected) {
    check(!value && failure.kind == expected && failure.operation[0],
          "unsupported request must return named failure without a value");
}
}
int main() try {
    MovieGainFailure failure;
    const auto zero = movie_gain_ui_base(0);
    check(std::bit_cast<std::uint32_t>(zero) == 0, "source slider zero exact bits");
    check(std::bit_cast<std::uint32_t>(movie_gain_default_base()) == 0x3f400000u,
          "source default literal0.75");
    // Literal source-reviewed sequence, independently admitted from the
    // offline polynomial/operand review; not read from the generated include.
    constexpr std::array<std::int32_t, 5> default_sequence{-24, -49, -85, -145, -960};
    const auto default_base = movie_gain_default_base();
    check(movie_gain_initial(default_base, failure) == default_sequence[0],
          "literal admitted default initial tenth-dB");
    for (std::uint64_t step = 1; step <= 4; ++step)
        check(movie_gain_fade({default_base, step, 4}, failure) == default_sequence[step],
              "literal admitted default fade sequence");
    check(std::bit_cast<std::uint32_t>(movie_gain_ui_base(std::numeric_limits<std::int64_t>::min())) == 0,
          "negative UI input clamps without overflow");
    check(std::bit_cast<std::uint32_t>(movie_gain_ui_base(std::numeric_limits<std::int64_t>::max())) ==
          std::bit_cast<std::uint32_t>(movie_gain_ui_base(100)), "large UI input clamps");
    check(movie_gain_initial(zero, failure) == -960 && failure.kind == MovieGainError::none,
          "literal original zero bypass initial");
    for (std::uint64_t step = 1; step <= 4; ++step)
        check(movie_gain_fade({zero, step, 4}, failure) == -960,
              "zero-base fade uses original zero bypass");
    for (int percent = 0; percent <= 100; ++percent) {
        const auto base = movie_gain_ui_base(percent);
        check(movie_gain_initial(base, failure).has_value(), "all source UI categories admitted");
        check(movie_gain_fade({base, 4, 4}, failure) == -960,
              "final source total4 step cancels to zero, not guessed intermediate gain");
    }
    struct LiteralCase {
        std::uint32_t bits;
        std::array<std::int32_t, 5> values;
    };
    // Offline literal-source results, plus the explicit native-wide final
    // cancellation policy. These are not copied from the interval include.
    constexpr LiteralCase cases[] = {
        {0x00000000u, {-960, -960, -960, -960, -960}},
        {0x00000001u, {-960, -960, -960, -960, -960}},
        {0x007fffffu, {-960, -960, -960, -960, -960}},
        {0x00800000u, {-960, -960, -960, -960, -960}},
        {0x3f000000u, {-60, -85, -120, -180, -960}},
        {0x3f400001u, {-24, -49, -85, -145, -960}},
        {0x3f800000u, {0, -24, -60, -120, -960}},
        {0x7f7fffffu, {0, 0, 0, 0, -960}},
    };
    for (const auto& literal : cases) {
        for (auto sign : {0u, 0x80000000u}) {
            const auto base = std::bit_cast<float>(literal.bits | sign);
            check(movie_gain_initial(base, failure) == literal.values[0] &&
                  failure.kind == MovieGainError::none, "finite signed literal initial response");
            for (std::uint64_t step = 1; step <= 4; ++step)
                check(movie_gain_fade({base, step, 4}, failure) == literal.values[step] &&
                      failure.kind == MovieGainError::none, "finite signed literal fade response");
        }
    }
    for (auto bits : {0x7f800000u, 0xff800000u, 0x7fc00001u, 0xffc00001u}) {
        const auto unknown = std::bit_cast<float>(bits);
        auto value = movie_gain_initial(unknown, failure);
        check_error(value, failure, MovieGainError::unsupported_base);
        value = movie_gain_fade({unknown, 1, 4}, failure);
        check_error(value, failure, MovieGainError::unsupported_base);
    }
    auto value = movie_gain_fade({zero, 1, 3}, failure);
    check_error(value, failure, MovieGainError::unsupported_fade_total);
    for (auto step : {0ull, 5ull, std::numeric_limits<unsigned long long>::max()}) {
        value = movie_gain_fade({zero, step, 4}, failure);
        check_error(value, failure, MovieGainError::unsupported_fade_step);
    }
    check(movie_gain_initial(zero, failure) == -960 && failure.kind == MovieGainError::none,
          "successful lookup clears previous failure");
    // Emit public API observations for the root's separate admitted-JSON
    // comparison. No table include or expected dataset is accessible here.
    for (unsigned category = 0; category <= 101; ++category) {
        const auto base = category == 101 ? movie_gain_default_base() : movie_gain_ui_base(category);
        const auto initial = movie_gain_initial(base, failure);
        check(initial.has_value(), "TSV initial lookup");
        std::array<std::int32_t, 4> fades{};
        for (std::uint64_t step = 1; step <= 4; ++step) {
            const auto value = movie_gain_fade({base, step, 4}, failure);
            check(value.has_value(), "TSV fade lookup");
            fades[step - 1] = *value;
        }
        std::printf("%u\t%08X\t%d\t%d\t%d\t%d\t%d\n", category,
                    static_cast<unsigned>(std::bit_cast<std::uint32_t>(base)),
                    static_cast<int>(*initial), static_cast<int>(fades[0]),
                    static_cast<int>(fades[1]), static_cast<int>(fades[2]),
                    static_cast<int>(fades[3]));
    }
    std::fputs("PASS finite native movie gain interval lookup and source UI values; native final mute extension, no playback/binary/hardware parity claim\n", stderr);
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL movie gain: %s\n", error.what()); return 1;
}
