#include "mka/movie_eligibility_numeric.hpp"
#include <bit>
#include <limits>
namespace mka::video {
namespace {
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559 &&
    std::numeric_limits<double>::radix==2 && std::numeric_limits<double>::digits==53 &&
    std::numeric_limits<double>::max_exponent==1024 && std::numeric_limits<double>::min_exponent==-1021);
// Private normalized eligibility units. No hardware encoding/exponent policy.
struct Units { bool negative=false;int exponent=0;std::uint32_t significant=0; };
Units integer_units(std::int64_t n) noexcept {
    if(!n)return {};
    const bool negative=n<0;
    const auto magnitude=negative?std::uint64_t{0}-static_cast<std::uint64_t>(n):static_cast<std::uint64_t>(n);
    const int exponent=63-std::countl_zero(magnitude);
    const auto significant=exponent>23?magnitude>>(exponent-23):magnitude<<(23-exponent);
    return {negative,exponent,static_cast<std::uint32_t>(significant)};
}
Units scaled_numerator(std::int64_t n) noexcept {
    auto value=integer_units(n);if(!value.significant)return value;
    // Exact literal10000 is the SECOND operand, significant9C4000. Its
    // low bit1 is zero; source operand-dependent decrement is identically0.
    const std::uint64_t product=static_cast<std::uint64_t>(value.significant)*0x9C4000u;
    value.exponent+=13;
    if(product>>47) { value.significant=static_cast<std::uint32_t>(product>>24);++value.exponent; }
    else value.significant=static_cast<std::uint32_t>(product>>23);
    return value;
}
Units measured_units(std::int64_t numerator,std::int64_t denominator) noexcept {
    auto a=scaled_numerator(numerator);const auto b=integer_units(denominator);
    if(!a.significant)return a; // denominator is validated positive.
    const std::uint64_t dividend=static_cast<std::uint64_t>(a.significant)<<25;
    auto quotient=dividend/b.significant;bool sticky=dividend%b.significant!=0;
    int exponent=a.exponent-b.exponent;int drop=2;
    if(!(quotient>>25)) {drop=1;--exponent;}
    sticky=sticky || (quotient&((std::uint64_t{1}<<(drop-1))-1))!=0;
    const bool half=(quotient>>(drop-1))&1u;quotient>>=drop;
    if(half&&(sticky||(quotient&1u)))++quotient;
    if(quotient>>24) {quotient>>=1;++exponent;}
    return {a.negative,exponent,static_cast<std::uint32_t>(quotient)};
}
Units tolerance_boundary(Units clock,Units tolerance,bool subtract) noexcept {
    if(subtract)tolerance.negative=!tolerance.negative;
    if(!tolerance.significant)return clock.significant?clock:Units{clock.negative&&tolerance.negative,0,0};
    if(!clock.significant)return tolerance;
    if(tolerance.exponent>clock.exponent ||
       (tolerance.exponent==clock.exponent&&tolerance.significant>clock.significant)) {
        const auto temp=clock;clock=tolerance;tolerance=temp;
    }
    const int gap=clock.exponent-tolerance.exponent;
    std::uint32_t large=clock.significant<<1;
    const std::uint32_t small=gap>25?0u:(tolerance.significant<<1)>>gap;
    if(clock.negative!=tolerance.negative) {
        large-=small;if(!large)return {};
        const int shift=std::countl_zero(large)-7;large<<=shift;clock.exponent-=shift;
    } else {
        large+=small;if(large&0x2000000u) {large>>=1;++clock.exponent;}
    }
    clock.significant=large>>1;return clock;
}
bool materialize(Units units,double& result) noexcept {
    const auto sign=units.negative?std::uint64_t{1}<<63:0;
    if(!units.significant) { result=std::bit_cast<double>(sign);return true; }
    // Validated native64 business inputs prove exponents[-74,77]. This
    // check protects ordinary native representation, never saturates/flushes.
    if(units.exponent< -1022 || units.exponent>1023)return false;
    const auto exponent=static_cast<std::uint64_t>(units.exponent+1023)<<52;
    const auto fraction=static_cast<std::uint64_t>(units.significant&0x7FFFFFu)<<29;
    result=std::bit_cast<double>(sign|exponent|fraction);return true;
}
}
const char* movie_eligibility_numeric_error_name(MovieEligibilityNumericError e) noexcept {
    switch(e) {
    case MovieEligibilityNumericError::none:return "none";
    case MovieEligibilityNumericError::invalid_rational:return "invalid_rational";
    case MovieEligibilityNumericError::invalid_frequency:return "invalid_frequency";
    case MovieEligibilityNumericError::overflow:return "overflow";
    }return "unknown_error";
}
std::optional<MovieEligibilityNumericResult> evaluate_movie_eligibility_numeric(
    MovieClockRational clock,MovieTimecodeRational target,std::int64_t frequency,
    std::int64_t margin,std::int64_t tolerance,MovieEligibilityNumericFailure& failure) noexcept {
    failure={};
    if(clock.denominator<=0 || target.denominator<=0) { failure.kind=MovieEligibilityNumericError::invalid_rational;return std::nullopt; }
    if(frequency<=0) {failure.kind=MovieEligibilityNumericError::invalid_frequency;return std::nullopt;}
    const __int128 delta=(static_cast<__int128>(clock.denominator)*margin)/frequency;
    const __int128 adjusted=static_cast<__int128>(clock.numerator)+delta;
    if(adjusted<std::numeric_limits<std::int64_t>::min() || adjusted>std::numeric_limits<std::int64_t>::max()) {
        failure.kind=MovieEligibilityNumericError::overflow;return std::nullopt;
    }
    MovieEligibilityNumericResult result;result.adjusted_clock_numerator=static_cast<std::int64_t>(adjusted);
    const auto x=measured_units(target.numerator,target.denominator);
    const auto c=measured_units(result.adjusted_clock_numerator,clock.denominator);
    const auto t=integer_units(tolerance);
    if(!materialize(x,result.target_units)||!materialize(c,result.clock_units)||
       !materialize(tolerance_boundary(c,t,false),result.early_boundary)||
       !materialize(tolerance_boundary(c,t,true),result.late_boundary)) {
        failure.kind=MovieEligibilityNumericError::overflow;return std::nullopt;
    }
    result.direct_eligible=result.target_units<=result.clock_units;
    result.early=result.early_boundary<result.target_units;
    result.late=result.target_units<=result.late_boundary;
    return result;
}
}
