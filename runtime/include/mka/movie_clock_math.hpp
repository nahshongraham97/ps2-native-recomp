#pragma once
#include <cstdint>
#include <optional>
namespace mka::video {
enum class MovieClockMathError { none, zero_divisor, invalid_denominator, overflow };
struct MovieClockMathFailure { MovieClockMathError kind=MovieClockMathError::none; };
const char* movie_clock_math_error_name(MovieClockMathError) noexcept;
// Ordinary checked integer math. No anchor sentinels, clock or timing policy.
std::optional<std::int64_t> scale_movie_clock(std::int64_t a,std::int64_t b,
    std::int64_t divisor,MovieClockMathFailure&) noexcept;
std::optional<std::int64_t> extrapolate_movie_clock(std::int64_t anchor_audio,
    std::int64_t audio_den,std::int64_t timer_now,std::int64_t anchor_timer,
    std::int64_t timer_den,MovieClockMathFailure&) noexcept;
}
