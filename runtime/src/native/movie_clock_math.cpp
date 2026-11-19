#include "mka/movie_clock_math.hpp"
#include <limits>
namespace mka::video {
const char* movie_clock_math_error_name(MovieClockMathError e) noexcept {
    switch(e) {
    case MovieClockMathError::none:return "none";
    case MovieClockMathError::zero_divisor:return "zero_divisor";
    case MovieClockMathError::invalid_denominator:return "invalid_denominator";
    case MovieClockMathError::overflow:return "overflow";
    } return "unknown_error";
}
namespace {
std::optional<std::int64_t> finish(__int128 v,MovieClockMathFailure& f) noexcept {
    if(v<std::numeric_limits<std::int64_t>::min() ||
       v>std::numeric_limits<std::int64_t>::max()) {
        f.kind=MovieClockMathError::overflow;return std::nullopt;
    }
    return static_cast<std::int64_t>(v);
}
}
std::optional<std::int64_t> scale_movie_clock(std::int64_t a,std::int64_t b,
    std::int64_t divisor,MovieClockMathFailure& f) noexcept {
    f={};
    if(!divisor) { f.kind=MovieClockMathError::zero_divisor;return std::nullopt; }
    // Two signed64 operands have product magnitude <=2^126, never INT128_MIN.
    return finish((static_cast<__int128>(a)*b)/divisor,f);
}
std::optional<std::int64_t> extrapolate_movie_clock(std::int64_t anchor_audio,
    std::int64_t audio_den,std::int64_t timer_now,std::int64_t anchor_timer,
    std::int64_t timer_den,MovieClockMathFailure& f) noexcept {
    f={};
    if(!timer_den) { f.kind=MovieClockMathError::zero_divisor;return std::nullopt; }
    if(audio_den<=0 || timer_den<0) {
        f.kind=MovieClockMathError::invalid_denominator;return std::nullopt;
    }
    // |delta|<=2^64-1, audio_den<=2^63-1. Product plus any signed64
    // anchor remains within signed128, even before division by positive den.
    const __int128 delta=static_cast<__int128>(timer_now)-anchor_timer;
    const __int128 scaled=(static_cast<__int128>(audio_den)*delta)/timer_den;
    return finish(static_cast<__int128>(anchor_audio)+scaled,f);
}
}
