#include "mka/movie_service_clock.hpp"
#include <limits>
namespace mka::video {
namespace {
using Wide=__int128;
constexpr std::int64_t rates[]={1,23976,24000,25000,29970,30000,50000,59940,60000};
bool fail(MovieServiceClockFailure& f,MovieServiceClockError e) noexcept { f.kind=e;return false; }
bool store(Wide n,std::int64_t& out,MovieServiceClockFailure& f) noexcept {
    if(n<std::numeric_limits<std::int64_t>::min() || n>std::numeric_limits<std::int64_t>::max())return fail(f,MovieServiceClockError::overflow);
    out=static_cast<std::int64_t>(n);return true;
}
std::optional<std::int64_t> retained(std::int64_t n) noexcept { return n==-1?std::nullopt:std::optional<std::int64_t>{n}; }
bool rate_valid(std::int64_t code) noexcept { return code>=0 && code<=8; }
}
const char* movie_service_clock_error_name(MovieServiceClockError e) noexcept {
    switch(e) {
    case MovieServiceClockError::none:return "none";
    case MovieServiceClockError::invalid_owner:return "invalid_owner";
    case MovieServiceClockError::invalid_frequency:return "invalid_frequency";
    case MovieServiceClockError::invalid_rational:return "invalid_rational";
    case MovieServiceClockError::unsupported_mode:return "unsupported_mode";
    case MovieServiceClockError::unsupported_rate:return "unsupported_rate";
    case MovieServiceClockError::overflow:return "overflow";
    case MovieServiceClockError::counter_changed:return "counter_changed";
    }return "unknown_error";
}
std::optional<MovieServiceClockConfig> movie_service_source_config(MovieServiceSourceMode mode,MovieServiceClockFailure& f) noexcept {
    f={};switch(mode) {
    case MovieServiceSourceMode::mode0:return MovieServiceClockConfig{59940,1000};
    case MovieServiceSourceMode::mode1:return MovieServiceClockConfig{50000,1000};
    case MovieServiceSourceMode::mode2:return MovieServiceClockConfig{60000,1000};
    }fail(f,MovieServiceClockError::unsupported_mode);return std::nullopt;
}
std::optional<NativeMovieServiceClock> NativeMovieServiceClock::create(MovieServiceClockConfig c,MovieServiceClockFailure& f) noexcept {
    f={};if(c.frequency<=0) { fail(f,MovieServiceClockError::invalid_frequency);return std::nullopt; }
    return NativeMovieServiceClock(c);
}
NativeMovieServiceClock::NativeMovieServiceClock(NativeMovieServiceClock&& other) noexcept:state_(other.state_),valid_(other.valid_) { other.valid_=false; }
NativeMovieServiceClock& NativeMovieServiceClock::operator=(NativeMovieServiceClock&& other) noexcept {
    if(this!=&other) { state_=other.state_;valid_=other.valid_;other.valid_=false; }return *this;
}
std::optional<MovieServiceMaintenance> NativeMovieServiceClock::maintenance(MovieServiceMaintenanceGates g,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_) { fail(f,MovieServiceClockError::invalid_owner);return std::nullopt; }
    auto next=state_;MovieServiceMaintenance result{g.state_admits&&g.first_inhibit_clear&&g.second_inhibit_clear,state_.retained.has_value()&&g.retained_admits};
    if(result.counter_advanced&&!store(static_cast<Wide>(state_.counter)+state_.step,next.counter,f))return std::nullopt;
    if(result.retained_advanced) {
        std::int64_t v;if(!store(static_cast<Wide>(*state_.retained)+state_.step,v,f))return std::nullopt;next.retained=retained(v);
    }
    state_=next;return result;
}
std::optional<MovieServiceClockRead> NativeMovieServiceClock::read(bool admitted,MovieServiceClockFailure& f) const noexcept {
    f={};if(!valid_) { fail(f,MovieServiceClockError::invalid_owner);return std::nullopt; }
    MovieServiceClockRead result;if(admitted)result.value=MovieClockRational{state_.counter,state_.frequency};return result;
}
bool NativeMovieServiceClock::set_step(std::int64_t step,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieServiceClockError::invalid_owner);state_.step=step;return true;
}
bool NativeMovieServiceClock::assign_retained(std::int64_t value,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieServiceClockError::invalid_owner);state_.retained=retained(value);return true;
}
bool NativeMovieServiceClock::invalidate_retained(MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieServiceClockError::invalid_owner);state_.retained.reset();return true;
}
bool NativeMovieServiceClock::increment(MovieClockRational r,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieServiceClockError::invalid_owner);
    if(r.denominator<=0)return fail(f,MovieServiceClockError::invalid_rational);
    // F positive64 times signed64 is bounded by2^126. Adding signed64
    // after division stays within signed128; only true stored fields narrow.
    const Wide delta=(static_cast<Wide>(state_.frequency)*r.numerator)/r.denominator;
    std::int64_t counter,value;
    if(!store(static_cast<Wide>(state_.counter)+delta,counter,f) ||
       !store(static_cast<Wide>(state_.retained.value_or(-1))+delta,value,f))return false;
    state_.counter=counter;state_.retained=retained(value);return true;
}
bool NativeMovieServiceClock::increment_rate(std::int64_t code,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieServiceClockError::invalid_owner);
    if(!rate_valid(code))return fail(f,MovieServiceClockError::unsupported_rate);
    return increment(code==0?MovieClockRational{0,29970}:MovieClockRational{1000,rates[code]},f);
}
bool NativeMovieServiceClock::increment_ticks(std::int64_t expected,Wide delta,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieServiceClockError::invalid_owner);
    if(state_.counter!=expected)return fail(f,MovieServiceClockError::counter_changed);
    if(delta<0)return fail(f,MovieServiceClockError::invalid_rational);
    // These differences fit signed128. Bound before adding an unrestricted
    // positive128 input, then narrow only the two actual stored destinations.
    const Wide counter_room=Wide(INT64_MAX)-state_.counter;
    const Wide retained_room=Wide(INT64_MAX)-state_.retained.value_or(-1);
    if(delta>counter_room||delta>retained_room)return fail(f,MovieServiceClockError::overflow);
    const auto counter=std::int64_t(Wide(state_.counter)+delta);
    const auto value=std::int64_t(Wide(state_.retained.value_or(-1))+delta);
    state_.counter=counter;state_.retained=retained(value);return true;
}
std::optional<MovieServiceRateProjection> NativeMovieServiceClock::project_rate(std::int64_t code,MovieServiceClockFailure& f) const noexcept {
    f={};if(!valid_) { fail(f,MovieServiceClockError::invalid_owner);return std::nullopt; }
    if(!rate_valid(code)) { fail(f,MovieServiceClockError::unsupported_rate);return std::nullopt; }
    MovieServiceRateProjection result;if(!code)return result;
    std::int64_t value;if(!store((static_cast<Wide>(rates[code])*state_.step)/1000,value,f))return std::nullopt;
    result.value=value;return result;
}
std::optional<bool> NativeMovieServiceClock::eligible(MovieClockRational target,MovieServiceClockFailure& f) noexcept {
    f={};if(!valid_) { fail(f,MovieServiceClockError::invalid_owner);return std::nullopt; }
    if(target.denominator<=0) { fail(f,MovieServiceClockError::invalid_rational);return std::nullopt; }
    if(!state_.retained || *state_.retained<0) { state_.retained=0;return true; }
    return static_cast<Wide>(*state_.retained)*target.denominator>=static_cast<Wide>(target.numerator)*state_.frequency;
}
std::optional<bool> NativeMovieServiceClock::retained_less_than(std::int64_t threshold,MovieServiceClockFailure& f) const noexcept {
    f={};if(!valid_) { fail(f,MovieServiceClockError::invalid_owner);return std::nullopt; }
    return state_.retained.value_or(-1)<threshold;
}
}
