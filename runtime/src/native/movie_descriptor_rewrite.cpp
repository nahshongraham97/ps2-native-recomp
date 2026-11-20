#include "mka/movie_descriptor_rewrite.hpp"
#include <array>
#include <limits>
#include <utility>
#include <type_traits>
namespace mka::video {
namespace {
using Wide=__int128;
static_assert(std::is_nothrow_copy_assignable_v<MovieDescriptorRewriteState>);
static_assert(std::is_nothrow_move_assignable_v<MovieDescriptorRewriteState>);
static_assert(std::is_nothrow_copy_constructible_v<MovieDescriptorRewriteResult>);
constexpr std::array<std::int64_t,9> nominal{0,24,24,25,30,30,50,60,60};
bool fail(MovieDescriptorRewriteFailure& f,MovieDescriptorRewriteError e) noexcept {f.kind=e;return false;}
bool store(Wide n,std::int64_t& out,MovieDescriptorRewriteFailure& f) noexcept {
    if(n<std::numeric_limits<std::int64_t>::min()||n>std::numeric_limits<std::int64_t>::max())return fail(f,MovieDescriptorRewriteError::overflow);
    out=std::int64_t(n);return true;
}
std::optional<MovieTimecodeRational> convert(const MovieRewriteDescriptor& d,MovieDescriptorRewriteFailure& f) noexcept {
    auto r=convert_movie_timecode(d.timecode,f.timecode);
    if(!r)fail(f,MovieDescriptorRewriteError::timecode);
    return r;
}
void six_fields(MovieTimecodeDescriptor& into,const MovieTimecodeDescriptor& from) noexcept {
    into.frame_rate_code=from.frame_rate_code;into.drop_frame=from.drop_frame;
    into.hours=from.hours;into.minutes=from.minutes;into.seconds=from.seconds;into.pictures=from.pictures;
}
bool increment(const MovieRewriteDescriptor& prior,MovieRewriteDescriptor& into,MovieDescriptorRewriteFailure& f) noexcept {
    const auto& p=prior.timecode;const auto rate=p.frame_rate_code;
    if(rate<1||rate>8)return fail(f,MovieDescriptorRewriteError::unsupported_rate);
    const Wide halves=Wide(prior.flag)+p.half_frame_adjustment,q=halves/2;
    Wide frame=Wide(p.pictures)+p.temporal_reference+1+q;
    const Wide carry=frame/nominal[rate];frame%=nominal[rate];
    Wide seconds=Wide(p.seconds)+carry,minutes=Wide(p.minutes)+seconds/60;
    seconds%=60;const Wide hours=Wide(p.hours)+minutes/60;minutes%=60;
    if(p.drop_frame&&seconds==0&&minutes%10!=0&&frame<2)frame=2;
    auto& d=into.timecode;d.frame_rate_code=rate;d.drop_frame=p.drop_frame;
    return store(hours,d.hours,f)&&store(minutes,d.minutes,f)&&store(seconds,d.seconds,f)&&
        store(frame,d.pictures,f)&&store(halves-2*q,d.half_frame_adjustment,f);
}
}
NativeMovieDescriptorRewrite::NativeMovieDescriptorRewrite(NativeMovieDescriptorRewrite&& other) noexcept
    :state_(std::move(other.state_)),valid_(other.valid_){other.valid_=false;}
NativeMovieDescriptorRewrite& NativeMovieDescriptorRewrite::operator=(NativeMovieDescriptorRewrite&& other) noexcept {
    if(this!=&other){state_=std::move(other.state_);valid_=other.valid_;other.valid_=false;}return *this;
}
bool NativeMovieDescriptorRewrite::set_rewrite_enabled(bool value,MovieDescriptorRewriteFailure& f) noexcept {
    f={};if(!valid_)return fail(f,MovieDescriptorRewriteError::invalid_owner);state_.enabled=value;return true;
}
std::optional<MovieDescriptorRewriteState> NativeMovieDescriptorRewrite::snapshot(MovieDescriptorRewriteFailure& f) const noexcept {
    f={};if(!valid_){fail(f,MovieDescriptorRewriteError::invalid_owner);return {};}return state_;
}
std::optional<MovieDescriptorRewriteResult> NativeMovieDescriptorRewrite::update(const MovieDescriptorRewriteInput& input,MovieDescriptorRewriteFailure& f) noexcept {
    f={};if(!valid_){fail(f,MovieDescriptorRewriteError::invalid_owner);return {};}
    const auto rate=input.current.timecode.frame_rate_code;
    if(rate<1||rate>8){fail(f,MovieDescriptorRewriteError::unsupported_rate);return {};}
    auto next=state_;auto current=input.current;
    MovieDescriptorRewriteResult result;
    result.gop_changed=!next.previous_gop_serial||*next.previous_gop_serial!=input.gop_serial;
    if(!next.enabled){
        bool enable=bool(input.timestamp22500)||input.gop_serial==0||input.repeat;
        if(!enable&&result.gop_changed&&next.cache){
            auto now=convert(current,f);if(!now)return {};
            auto before=convert(next.cache->descriptor,f);if(!before)return {};
            const Wide limit=Wide(before->numerator)+Wide(input.comparison_margin)*before->denominator;
            enable=now->numerator<=before->numerator||Wide(now->numerator)>=limit;
        }
        if(enable){next.enabled=true;result.enabled_latched=true;}
    }
    if(next.enabled){
        if(result.gop_changed&&input.timestamp22500){
            const auto temporal=current.timecode.temporal_reference;
            if(temporal<0||temporal>1023){
                fail(f,MovieDescriptorRewriteError::invalid_temporal);return {};
            }
            auto converted=convert_movie_position_timecode(input.timestamp22500,rate,current.timecode.drop_frame,
                std::uint16_t(temporal),f.timecode);
            if(!converted){fail(f,MovieDescriptorRewriteError::timecode);return {};}
            current.timecode=*converted;result.branch=MovieDescriptorRewriteBranch::position;
        }else if(!next.cache){
            if(!input.preserve_direct_without_cache){
                auto& d=current.timecode;d.drop_frame=false;d.hours=d.minutes=d.seconds=d.pictures=0;
                result.branch=MovieDescriptorRewriteBranch::zero_origin;
            }
        }else if(!result.gop_changed){
            six_fields(current.timecode,next.cache->descriptor.timecode);result.branch=MovieDescriptorRewriteBranch::cached;
        }else{
            if(!increment(next.cache->descriptor,current,f))return {};
            result.branch=MovieDescriptorRewriteBranch::incremented;
        }
    }
    if(!next.first_baseline){
        auto first=current;first.timecode.temporal_reference=0;
        auto baseline=convert(first,f);if(!baseline)return {};next.first_baseline=*baseline;
    }
    auto absolute=convert(current,f);if(!absolute)return {};
    std::int64_t relative;
    if(!store(Wide(absolute->numerator)-next.first_baseline->numerator,relative,f))return {};
    result.published={current,{relative,absolute->denominator}};
    result.first_baseline=*next.first_baseline;
    if(relative>=(next.cache?next.cache->relative.numerator:-1)){
        next.cache=result.published;result.cache_updated=true;
    }
    next.published=result.published;next.previous_gop_serial=input.gop_serial;
    state_=std::move(next);return result;
}
}
