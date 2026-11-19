#include "mka/movie_gpu.hpp"
#include <limits>

namespace mka::video {
namespace {
bool valid_time(const MovieTimestamp& t){return !t.present||(t.numerator>0&&t.denominator>0);}
// Compare positive rational numbers without overflowing cross products.
int compare(std::uint64_t a,std::uint64_t b,std::uint64_t c,std::uint64_t d){
    bool inverted=false;
    for(;;){const auto x=a/b,y=c/d;
        if(x!=y)return ((x<y)?-1:1)*(inverted?-1:1);
        const auto r=a%b,s=c%d;
        if(!r||!s)return ((r!=0)-(s!=0))*(inverted?-1:1);
        a=b;b=r;c=d;d=s;inverted=!inverted;
    }
}
// Exact rounded multiplier*numerator/denominator, without a widened integer
// requirement or an overflowing intermediate. Fractional quotient <=multiplier.
std::optional<std::uint64_t> rounded(std::uint32_t multiplier,std::uint64_t numerator,std::uint64_t denominator){
    const auto whole=numerator/denominator,fraction=numerator%denominator;
    if(whole>UINT64_MAX/multiplier)return std::nullopt;
    std::uint64_t q=0,r=0;
    for(int bit=31;bit>=0;--bit){
        q*=2;
        if(r>=denominator-r){r-=denominator-r;++q;}else r*=2;
        if((multiplier>>bit)&1){if(r>=denominator-fraction){r-=denominator-fraction;++q;}else r+=fraction;}
    }
    if(r>=denominator/2+denominator%2)++q;
    if(whole*multiplier>UINT64_MAX-q)return std::nullopt;
    return whole*multiplier+q;
}
}
MovieGpuError validate_movie_upload(const MovieRgbaFrame& f)noexcept{
    if(!f.width||!f.height||f.height>SIZE_MAX/std::size_t(f.width)||std::size_t(f.width)*f.height>SIZE_MAX/4)return MovieGpuError::invalid_frame;
    if(f.rgba.size()!=std::size_t(f.width)*f.height*4||
       (f.conversion_role!=MovieColorRole::helper_default&&f.conversion_role!=MovieColorRole::fresh_preset21)||
       !valid_time(f.source_pts)||!valid_time(f.timestamp))return MovieGpuError::invalid_frame;
    return MovieGpuError::none;
}
MovieGpuError validate_movie_present(const NativeMoviePresentPlan& p)noexcept{
    if(!p.frame||!p.frame->identity()||!p.frame->width()||!p.frame->height()||p.sample.x||p.sample.y||
       p.sample.width!=p.frame->width()||p.sample.height!=p.frame->height())return MovieGpuError::invalid_plan;
    if((p.filter!=MovieFilter::nearest&&p.filter!=MovieFilter::linear)||p.alpha!=MovieAlpha::replace_rgba8)return MovieGpuError::unsupported_effect;
    return MovieGpuError::none;
}
std::optional<MovieRect> movie_destination(const NativeMoviePresentPlan& p,const MovieHostLayout& l)noexcept{
    if(validate_movie_present(p)!=MovieGpuError::none||!l.destination||!l.window_width||!l.window_height||l.policy!=MovieWindowPolicy::centered_fit)return {};
    std::uint64_t n=0,d=0;
    if(l.aspect==MovieAspectMode::source_sample_aspect){
        const auto& m=p.frame->metadata();if(m.sample_aspect_numerator<=0||m.sample_aspect_denominator<=0||l.display_aspect_numerator||l.display_aspect_denominator)return {};
        n=std::uint64_t(p.frame->width())*unsigned(m.sample_aspect_numerator);d=std::uint64_t(p.frame->height())*unsigned(m.sample_aspect_denominator);
    }else if(l.aspect==MovieAspectMode::explicit_display_aspect){
        n=l.display_aspect_numerator;d=l.display_aspect_denominator;if(!n||!d)return {};
    }else return {};
    std::uint64_t w=l.window_width,h=l.window_height;
    if(compare(l.window_width,l.window_height,n,d)<=0){const auto value=rounded(l.window_width,d,n);if(!value)return {};h=*value?*value:1;}
    else{const auto value=rounded(l.window_height,n,d);if(!value)return {};w=*value?*value:1;}
    if(w>l.window_width||h>l.window_height)return {};
    return MovieRect{std::uint32_t((l.window_width-w)/2),std::uint32_t((l.window_height-h)/2),std::uint32_t(w),std::uint32_t(h)};
}
} // namespace mka::video
