#pragma once
#include "mka/movie_clock_filter.hpp"
#include "mka/movie_timecode.hpp"
#include <cstdint>
#include <optional>
namespace mka::video {
enum class MovieEligibilityNumericError { none,invalid_rational,invalid_frequency,overflow };
struct MovieEligibilityNumericFailure { MovieEligibilityNumericError kind=MovieEligibilityNumericError::none; };
const char* movie_eligibility_numeric_error_name(MovieEligibilityNumericError) noexcept;
struct MovieEligibilityNumericResult {
    std::int64_t adjusted_clock_numerator=0;
    double target_units=0,clock_units=0,early_boundary=0,late_boundary=0;
    bool direct_eligible=false,early=false,late=false;
};
// Ordered business quantization only. No availability/origin/adaptive policy.
std::optional<MovieEligibilityNumericResult> evaluate_movie_eligibility_numeric(
    MovieClockRational clock,MovieTimecodeRational target,std::int64_t frequency,
    std::int64_t margin,std::int64_t tolerance,MovieEligibilityNumericFailure&) noexcept;
}
