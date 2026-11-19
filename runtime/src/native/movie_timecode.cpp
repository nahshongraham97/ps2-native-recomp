#include "mka/movie_timecode.hpp"
#include <array>
#include <limits>
#include <cmath>
#include <cfenv>
#ifdef __FAST_MATH__
#error Native movie position timecode requires strict floating-point semantics
#endif
namespace mka::video {
namespace {
// Exact signed-magnitude accumulation. Seven signed64 *smallconstant terms
// fit within96bits, so128 magnitude bits suffice without signed UB or an
// intermediate64 overflow rejecting a representable cancellation result.
struct Wide {std::uint64_t hi=0,lo=0;bool negative=false;};
int compare(const Wide& a,const Wide& b) noexcept {return a.hi!=b.hi?(a.hi<b.hi?-1:1):a.lo!=b.lo?(a.lo<b.lo?-1:1):0;}
Wide magnitude_product(std::int64_t x,std::uint32_t factor) noexcept {
    const auto m=x<0?std::uint64_t(-(x+1))+1:std::uint64_t(x);
    const std::uint64_t low=(m&0xffffffffu)*factor,high=(m>>32)*factor;
    Wide w;w.lo=low+(high<<32);w.hi=(high>>32)+(w.lo<low);w.negative=x<0&&(w.hi||w.lo);return w;
}
void add(Wide& a,Wide b) noexcept {
    if(a.negative==b.negative){const auto old=a.lo;a.lo+=b.lo;a.hi+=b.hi+(a.lo<old);}
    else {
        const auto order=compare(a,b);
        if(order<0){const auto old=b.lo;b.lo-=a.lo;b.hi-=a.hi+(old<a.lo);a=b;}
        else {const auto old=a.lo;a.lo-=b.lo;a.hi-=b.hi+(old<b.lo);}
    }
    if(!a.hi&&!a.lo)a.negative=false;
}
}
const char* movie_timecode_error_name(MovieTimecodeError e) noexcept {
    switch(e){case MovieTimecodeError::none:return "none";case MovieTimecodeError::missing_gop:return "missing_gop";case MovieTimecodeError::missing_timestamp:return "missing_timestamp";case MovieTimecodeError::invalid_temporal_reference:return "invalid_temporal_reference";case MovieTimecodeError::unsupported_fp_environment:return "unsupported_fp_environment";case MovieTimecodeError::unsupported_rate:return "unsupported_rate";case MovieTimecodeError::overflow:return "overflow";}return "unknown_movie_timecode_error";
}
std::optional<MovieTimecodeRational> convert_movie_timecode(const MovieTimecodeDescriptor& d,MovieTimecodeFailure& f) noexcept {
    f={};
    constexpr std::array<std::uint32_t,9> rates{1,23976,24000,25000,29970,30000,50000,59940,60000};
    if(d.frame_rate_code<1||d.frame_rate_code>8){f.kind=MovieTimecodeError::unsupported_rate;return {};}
    const auto code=d.frame_rate_code;const auto unit=rates[code];
    // Original fractional-rate handlers use integer nominal frame counts in
    // the numerator; the unmodified source table supplies the denominator.
    std::uint32_t nominal=unit;
    if(code==1)nominal=24000;else if(code==4)nominal=30000;else if(code==7)nominal=60000;
    // Original813A80 also drops TWO labels/minute at ratecode7; it is
    // not the conventional four-label60fps normalization.
    const std::uint32_t drop=d.drop_frame&&(code==1||code==4||code==7)?2:0;
    const std::uint32_t hour_factor=nominal*3600-drop*54*1000;
    const std::uint32_t minute_factor=nominal*60-drop*1000;
    Wide sum;
    add(sum,magnitude_product(d.hours,hour_factor));
    add(sum,magnitude_product(d.minutes,minute_factor));
    // C++ signed /10 truncates towardzero, matching original MIPS DIV even
    // for native negative inputs; no wraparound arithmetic is reproduced.
    add(sum,magnitude_product(d.minutes/10,drop*1000));
    add(sum,magnitude_product(d.seconds,nominal));
    add(sum,magnitude_product(d.pictures,1000));
    add(sum,magnitude_product(d.temporal_reference,1000));
    add(sum,magnitude_product(d.half_frame_adjustment,500));
    const auto limit=sum.negative?(std::uint64_t(1)<<63):std::uint64_t(std::numeric_limits<std::int64_t>::max());
    if(sum.hi||sum.lo>limit){f.kind=MovieTimecodeError::overflow;return {};}
    std::int64_t result;
    if(sum.negative)result=sum.lo==(std::uint64_t(1)<<63)?std::numeric_limits<std::int64_t>::min():-static_cast<std::int64_t>(sum.lo);
    else result=static_cast<std::int64_t>(sum.lo);
    return MovieTimecodeRational{result,unit};
}
std::optional<MovieTimecodeDescriptor> convert_movie_position_timecode(
    std::optional<std::uint64_t> timestamp22500,std::uint8_t code,bool drop,
    std::uint16_t temporal,MovieTimecodeFailure& f) noexcept {
    f={};
    if(!timestamp22500){f.kind=MovieTimecodeError::missing_timestamp;return {};}
    constexpr std::array<std::uint32_t,9> rates{1,23976,24000,25000,29970,30000,50000,59940,60000};
    constexpr std::array<std::uint32_t,9> nominal{0,24,24,25,30,30,50,60,60};
    if(code<1||code>8){f.kind=MovieTimecodeError::unsupported_rate;return {};}
    if(temporal>1023){f.kind=MovieTimecodeError::invalid_temporal_reference;return {};}
    if(std::fegetround()!=FE_TONEAREST){f.kind=MovieTimecodeError::unsupported_fp_environment;return {};}
    // Separate binary64 materializations preserve7F6DD0's software-double
    // conversion/multiply/divide/add/floor ordering. No fused expression or
    // replacement with an integer rational/long-double approximation.
    volatile double stamp=static_cast<double>(*timestamp22500);
    volatile double factor=static_cast<double>(2u*rates[code]);
    volatile double product=stamp*factor;
    volatile double quotient=product/22500000.0;
    volatile double biased=quotient+0.5;
    const double rounded=std::floor(biased);
    if(!std::isfinite(rounded)||rounded<0.0||rounded>=0x1p63){f.kind=MovieTimecodeError::overflow;return {};}
    const auto q=static_cast<std::uint64_t>(rounded);
    std::uint64_t k=q/2; k=k<temporal?0:k-temporal;
    MovieTimecodeDescriptor out;out.frame_rate_code=code;out.drop_frame=drop;
    out.temporal_reference=temporal;out.half_frame_adjustment=static_cast<std::int64_t>(q&1);
    std::uint64_t hours=0,minutes=0,seconds=0,pictures=0;
    if(drop&&(code==4||code==7)) {
        // Literal original849510/849530 inverse tables. In particular their
        // hour and ten-minute divisors are not standard SMPTE substitutions.
        constexpr std::array<std::uint32_t,8> table30{107891,17981,1800,1798,28,30,10,2};
        constexpr std::array<std::uint32_t,8> table60{215783,35963,3600,3596,56,60,10,4};
        const auto& t=code==4?table30:table60;
        hours=k/t[0];const auto rem=k%t[0];const auto block=rem/t[1];auto r=rem%t[1];
        std::uint64_t minute=0;
        if(r<t[2]){seconds=r/t[5];pictures=r%t[5];}
        else {r-=t[2];minute=1+r/t[3];r%=t[3];
            if(r<t[4]){seconds=0;pictures=r+t[7];}
            else {r-=t[4];seconds=1+r/t[5];pictures=r%t[5];}}
        minutes=block*t[6]+minute;
    }else {
        pictures=k%nominal[code];const auto whole_seconds=k/nominal[code];
        seconds=whole_seconds%60;minutes=(whole_seconds/60)%60;hours=whole_seconds/3600;
    }
    const auto limit=std::uint64_t(std::numeric_limits<std::int64_t>::max());
    if(hours>limit||minutes>limit||seconds>limit||pictures>limit){f.kind=MovieTimecodeError::overflow;return {};}
    out.hours=static_cast<std::int64_t>(hours);out.minutes=static_cast<std::int64_t>(minutes);
    out.seconds=static_cast<std::int64_t>(seconds);out.pictures=static_cast<std::int64_t>(pictures);return out;
}
std::optional<MovieTimecodeRational> convert_movie_picture_timecode(const Mpeg2PictureTiming& p,MovieTimecodeFailure& f) noexcept {
    f={};if(!p.gop){f.kind=MovieTimecodeError::missing_gop;return {};}
    const auto& g=*p.gop;
    return convert_movie_timecode({p.sequence.frame_rate_code,g.drop_frame,g.hours,g.minutes,g.seconds,g.pictures,p.temporal_reference,0},f);
}
}
