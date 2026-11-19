#include "mka/movie_timecode.hpp"
#include <array>
#include <limits>
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
    switch(e){case MovieTimecodeError::none:return "none";case MovieTimecodeError::missing_gop:return "missing_gop";case MovieTimecodeError::unsupported_rate:return "unsupported_rate";case MovieTimecodeError::overflow:return "overflow";}return "unknown_movie_timecode_error";
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
std::optional<MovieTimecodeRational> convert_movie_picture_timecode(const Mpeg2PictureTiming& p,MovieTimecodeFailure& f) noexcept {
    f={};if(!p.gop){f.kind=MovieTimecodeError::missing_gop;return {};}
    const auto& g=*p.gop;
    return convert_movie_timecode({p.sequence.frame_rate_code,g.drop_frame,g.hours,g.minutes,g.seconds,g.pictures,p.temporal_reference,0},f);
}
}
