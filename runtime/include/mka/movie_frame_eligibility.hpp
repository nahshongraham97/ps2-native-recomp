#pragma once
#include "mka/movie_eligibility_numeric.hpp"
#include <memory>
namespace mka::video {
enum class MovieFrameEligibilityMode { direct, adaptive };
enum class MovieFrameEligibilityError { none, invalid_owner, invalid_config, numeric, overflow, allocation };
struct MovieFrameEligibilityFailure {
    MovieFrameEligibilityError kind=MovieFrameEligibilityError::none;
    MovieEligibilityNumericFailure numeric;
};
struct MovieFrameEligibilityConfig {
    std::int64_t frequency,step,margin,tolerance;
    MovieFrameEligibilityMode mode;
    MovieFrameEligibilityConfig(std::int64_t f,std::int64_t s,std::int64_t m,std::int64_t t,MovieFrameEligibilityMode branch)
        :frequency(f),step(s),margin(m),tolerance(t),mode(branch){}
};
struct MovieFrameEligibilityState {
    std::uint64_t repeated_late=100;
    double previous_late=-1.0,previous_middle=-1.0;
    bool previous_middle_result=false;
    bool operator==(const MovieFrameEligibilityState&) const = default;
};
enum class MovieFrameEligibilityBranch { direct, early, late_duplicate, late_new, middle };
struct MovieFrameEligibilityResult {
    bool eligible;
    MovieFrameEligibilityBranch branch;
    MovieEligibilityNumericResult metrics;
    MovieFrameEligibilityState state;
};
// Ordinary available-clock business decision only. Callers supply already
// adjusted target and its associated actual sequence rate metadata.
class NativeMovieFrameEligibility {
public:
    static std::unique_ptr<NativeMovieFrameEligibility> create(MovieFrameEligibilityConfig,MovieFrameEligibilityFailure&) noexcept;
    NativeMovieFrameEligibility(NativeMovieFrameEligibility&&) noexcept;
    NativeMovieFrameEligibility& operator=(NativeMovieFrameEligibility&&) noexcept;
    std::optional<MovieFrameEligibilityState> snapshot(MovieFrameEligibilityFailure&) const noexcept;
    std::optional<MovieFrameEligibilityResult> evaluate(MovieClockRational,MovieTimecodeRational,std::int64_t actual_rate_code,MovieFrameEligibilityFailure&) noexcept;
private:
    explicit NativeMovieFrameEligibility(MovieFrameEligibilityConfig c):config_(c){}
    MovieFrameEligibilityConfig config_;
    MovieFrameEligibilityState state_;
    bool valid_=true;
};
}
