#include "mka/movie_clock_filter.hpp"
#include <algorithm>
#include <limits>
#include <utility>
namespace mka::video {
namespace {
using Wide=__int128;
bool mode_valid(MovieClockFilterMode m) noexcept { return m==MovieClockFilterMode::mode0 || m==MovieClockFilterMode::mode1; }
bool config_valid(const MovieClockFilterConfig& c) noexcept {
    return c.history_size>0 && c.quantum.denominator>0 && c.quantum.numerator>0 &&
        c.large_error.denominator>0 && c.large_error.numerator>=0 &&
        c.first_offset.denominator>0 && c.restart_offset.denominator>0;
}
bool fail(MovieClockFilterFailure& f,MovieClockFilterError e) noexcept { f.kind=e;return false; }
bool store(Wide v,std::int64_t& out,MovieClockFilterFailure& f) noexcept {
    if(v<std::numeric_limits<std::int64_t>::min() || v>std::numeric_limits<std::int64_t>::max()) return fail(f,MovieClockFilterError::overflow);
    out=static_cast<std::int64_t>(v);return true;
}
bool count(std::uint64_t& v,Wide increment,MovieClockFilterFailure& f) noexcept {
    if(increment<0 || increment>static_cast<Wide>(std::numeric_limits<std::uint64_t>::max()-v)) return fail(f,MovieClockFilterError::overflow);
    v+=static_cast<std::uint64_t>(increment);return true;
}
Wide magnitude(Wide v) noexcept { return v<0?-v:v; }
Wide scaled(std::int64_t den,MovieClockRational r) noexcept { return (static_cast<Wide>(den)*r.numerator)/r.denominator; }
Wide extrapolate(const MovieClockFilterState& s,MovieClockRational a,MovieClockRational t) noexcept {
    if(!s.anchor)return 0;
    return static_cast<Wide>(s.anchor->audio)+(static_cast<Wide>(a.denominator)*(static_cast<Wide>(t.numerator)-s.anchor->timer))/t.denominator;
}
bool history_error(MovieClockFilterFailure& f,MovieClockHistoryFailure h) noexcept {
    f.history=h;return fail(f,h.kind==MovieClockHistoryError::allocation?MovieClockFilterError::allocation:h.kind==MovieClockHistoryError::overflow?MovieClockFilterError::overflow:MovieClockFilterError::history);
}
}
const char* movie_clock_filter_error_name(MovieClockFilterError e) noexcept {
    switch(e) {
    case MovieClockFilterError::none:return "none";
    case MovieClockFilterError::invalid_owner:return "invalid_owner";
    case MovieClockFilterError::invalid_config:return "invalid_config";
    case MovieClockFilterError::invalid_rational:return "invalid_rational";
    case MovieClockFilterError::unsupported_mode:return "unsupported_mode";
    case MovieClockFilterError::allocation:return "allocation";
    case MovieClockFilterError::overflow:return "overflow";
    case MovieClockFilterError::history:return "history";
    case MovieClockFilterError::math:return "math";
    }return "unknown_error";
}
NativeMovieClockFilter::NativeMovieClockFilter(const MovieClockFilterConfig& c,NativeMovieClockHistory&& h) noexcept:config_(c),history_(std::move(h)) {}
NativeMovieClockFilter::NativeMovieClockFilter(NativeMovieClockFilter&& other) noexcept:config_(other.config_),state_(other.state_),history_(std::move(other.history_)) {}
NativeMovieClockFilter& NativeMovieClockFilter::operator=(NativeMovieClockFilter&& other) noexcept {
    if(this!=&other) { config_=other.config_;state_=other.state_;history_=std::move(other.history_); }return *this;
}
std::optional<NativeMovieClockFilter> NativeMovieClockFilter::create(const MovieClockFilterConfig& c,MovieClockFilterFailure& f) noexcept {
    f={};if(!mode_valid(c.mode)) { fail(f,MovieClockFilterError::unsupported_mode);return std::nullopt; }
    if(!config_valid(c)) { fail(f,MovieClockFilterError::invalid_config);return std::nullopt; }
    MovieClockHistoryFailure hf;auto h=NativeMovieClockHistory::create(c.history_size,hf);
    if(!h) { history_error(f,hf);return std::nullopt; }
    return NativeMovieClockFilter(c,std::move(*h));
}
bool NativeMovieClockFilter::set_enabled(bool v,MovieClockFilterFailure& f) noexcept { f={};if(!valid())return fail(f,MovieClockFilterError::invalid_owner);config_.enabled=v;return true; }
bool NativeMovieClockFilter::set_paused(bool v,MovieClockFilterFailure& f) noexcept { f={};if(!valid())return fail(f,MovieClockFilterError::invalid_owner);config_.paused=v;return true; }
bool NativeMovieClockFilter::set_mode(MovieClockFilterMode m,MovieClockFilterFailure& f) noexcept {
    f={};if(!valid())return fail(f,MovieClockFilterError::invalid_owner);if(!mode_valid(m))return fail(f,MovieClockFilterError::unsupported_mode);config_.mode=m;return true;
}
bool NativeMovieClockFilter::clear_history(MovieClockFilterFailure& f) noexcept {
    f={};if(!valid())return fail(f,MovieClockFilterError::invalid_owner);
    auto resets=state_.statistics.resets;if(!count(resets,1,f))return false;
    MovieClockHistoryFailure hf;if(!history_.clear(hf))return history_error(f,hf);
    state_.statistics.resets=resets;return true;
}
bool NativeMovieClockFilter::add_previous_output(MovieClockRational r,bool& changed,MovieClockFilterFailure& f) noexcept {
    f={};changed=false;if(!valid())return fail(f,MovieClockFilterError::invalid_owner);
    if(r.denominator<=0)return fail(f,MovieClockFilterError::invalid_rational);
    if(config_.mode!=MovieClockFilterMode::mode0)return true;
    std::int64_t next;if(!store(static_cast<Wide>(state_.previous_output.numerator)+scaled(state_.previous_output.denominator,r),next,f))return false;
    state_.previous_output.numerator=next;changed=true;return true;
}
std::optional<MovieClockFilterUpdate> NativeMovieClockFilter::update(MovieClockRational a,MovieClockRational t,MovieClockFilterFailure& f) noexcept {
    f={};if(!valid()) { fail(f,MovieClockFilterError::invalid_owner);return std::nullopt; }
    if(a.denominator<=0 || t.denominator<=0) { fail(f,MovieClockFilterError::invalid_rational);return std::nullopt; }
    if(!mode_valid(config_.mode)) { fail(f,MovieClockFilterError::unsupported_mode);return std::nullopt; }
    if(!config_valid(config_)) { fail(f,MovieClockFilterError::invalid_config);return std::nullopt; }
    MovieClockFilterUpdate result{a,a,{}};
    if(t.denominator==1 || !config_.enabled) { result.observations.bypass=true;return result; }
    MovieClockHistoryFailure hf;auto staged=history_.clone(hf);if(!staged) { history_error(f,hf);return std::nullopt; }
    auto s=state_;auto& obs=result.observations;
    const auto clear=[&]() { if(!count(s.statistics.resets,1,f))return false;if(!staged->clear(hf))return history_error(f,hf);return true; };
    const auto oldfloor=s.audio_floor;a.numerator=std::max(a.numerator,oldfloor);
    if(config_.paused)s.active=false;
    else if(!s.active) {
        if(a.numerator>oldfloor) {
            s.active=true;const bool first=!s.anchor;
            const auto offset=first?config_.first_offset:config_.restart_offset;
            std::int64_t anchor;if(!store(static_cast<Wide>(a.numerator)+scaled(a.denominator,offset),anchor,f))return std::nullopt;
            s.anchor=MovieClockFilterAnchor{t.numerator,anchor};
            if(!clear())return std::nullopt;
            obs.first_anchor=first;obs.restart_anchor=!first;
        } else if(config_.mode==MovieClockFilterMode::mode0)s.active=true;
    }
    s.audio_floor=std::max(a.numerator,oldfloor);
    const Wide e=extrapolate(s,a,t);
    if(!s.active) {
        if(static_cast<Wide>(s.audio_floor)<e) {
            s.anchor=MovieClockFilterAnchor{t.numerator,config_.mode==MovieClockFilterMode::mode0?s.previous_output.numerator:s.audio_floor};
            if(!count(s.statistics.inactive_reanchors,1,f))return std::nullopt;obs.inactive_reanchor=true;
        }
    } else if(config_.mode==MovieClockFilterMode::mode1) {
        const Wide difference=static_cast<Wide>(a.numerator)-e;
        const Wide bound=scaled(a.denominator,config_.large_error);
        if(magnitude(difference)>bound) {
            s.anchor=MovieClockFilterAnchor{t.numerator,a.numerator};
            if(!clear() || !count(s.statistics.large_resets,1,f))return std::nullopt;obs.large_reset=true;
        } else {
            std::int64_t residual;if(!store(difference,residual,f))return std::nullopt;
            auto mean=staged->insert(residual,hf);if(!mean) { history_error(f,hf);return std::nullopt; }
            s.mean_before=s.mean_after=*mean;obs.inserted=true;
            const Wide quantum=scaled(a.denominator,config_.quantum);
            if(quantum<=0) { fail(f,MovieClockFilterError::invalid_config);return std::nullopt; }
            if(magnitude(static_cast<Wide>(*mean))>quantum) {
                const Wide q=(2*static_cast<Wide>(*mean))/quantum;
                const Wide k=*mean>quantum?q-1:q+1;
                if(!count(k>0?s.statistics.positive_adjustments:s.statistics.negative_adjustments,k>0?k:-k,f))return std::nullopt;
                const Wide correction=(k*quantum)/2;
                std::int64_t stored_correction,anchor;
                if(!store(correction,stored_correction,f) || !store(e+correction,anchor,f))return std::nullopt;
                s.anchor=MovieClockFilterAnchor{t.numerator,anchor};
                mean=staged->recenter(stored_correction,hf);if(!mean) { history_error(f,hf);return std::nullopt; }
                s.mean_after=*mean;obs.corrected=true;obs.correction=stored_correction;
            }
        }
    }
    Wide output=extrapolate(s,a,t);MovieClockRational out{0,a.denominator};
    if(output<s.previous_output.numerator) { out=s.previous_output;obs.output_clamped=true; }
    else if(!store(output,out.numerator,f))return std::nullopt;
    std::int64_t residual;if(!store(static_cast<Wide>(a.numerator)-out.numerator,residual,f))return std::nullopt;
    auto& minimum=s.active?s.statistics.active_min:s.statistics.inactive_min;
    auto& maximum=s.active?s.statistics.active_max:s.statistics.inactive_max;
    minimum=std::min(minimum,residual);maximum=std::max(maximum,residual);
    s.previous_audio=a;s.previous_timer=t;s.previous_output=out;
    result.clamped_audio=a;result.output=out;
    history_=std::move(*staged);state_=s;return result;
}
}
