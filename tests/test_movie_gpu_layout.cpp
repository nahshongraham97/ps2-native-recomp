#include "mka/movie_gpu.hpp"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"movie layout check %d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace {
struct Frame final:mka::video::MovieGpuFrame {
    unsigned w=1,h=1;mka::video::MovieGpuMetadata info;
    std::uint64_t identity()const noexcept override{return 1;}
    unsigned width()const noexcept override{return w;}unsigned height()const noexcept override{return h;}
    const mka::video::MovieGpuMetadata& metadata()const noexcept override{return info;}
};
}
int main(){using namespace mka::video;
    auto frame=std::make_shared<Frame>();NativeMoviePresentPlan p{frame,{0,0,1,1},MovieFilter::nearest,MovieAlpha::replace_rgba8};
    MovieHostLayout l{1,1920,1080,MovieAspectMode::source_sample_aspect,0,0,MovieWindowPolicy::centered_fit};
    CHECK(!movie_destination(p,l));frame->info.sample_aspect_numerator=frame->info.sample_aspect_denominator=1;
    auto r=movie_destination(p,l);CHECK(r&&r->width==1080&&r->height==1080&&r->x==420&&r->y==0);
    l.aspect=MovieAspectMode::explicit_display_aspect;l.display_aspect_numerator=4;l.display_aspect_denominator=3;
    r=movie_destination(p,l);CHECK(r&&r->width==1440&&r->height==1080&&r->x==240);
    l.aspect=MovieAspectMode::source_sample_aspect;CHECK(!movie_destination(p,l)); // conflicting policy rejects
    l.display_aspect_numerator=l.display_aspect_denominator=0;
    // Independent widened-integer oracle stresses the production fraction
    // comparator and long-division rounding without copying their algorithms.
    std::uint64_t state=0x5d7ac9e18ull;
    const auto next=[&](){state^=state<<13;state^=state>>7;state^=state<<17;return unsigned(state)|1u;};
    for(unsigned i=0;i<10000;++i){frame->w=next();frame->h=next();p.sample.width=frame->w;p.sample.height=frame->h;
        frame->info.sample_aspect_numerator=int(next()&0x7fffffffu)|1;frame->info.sample_aspect_denominator=int(next()&0x7fffffffu)|1;
        l.window_width=next();l.window_height=next();
        using Wide=unsigned __int128;
        const Wide n=Wide(frame->w)*unsigned(frame->info.sample_aspect_numerator),d=Wide(frame->h)*unsigned(frame->info.sample_aspect_denominator);
        Wide w=l.window_width,h=l.window_height;
        if(Wide(l.window_width)*d<=Wide(l.window_height)*n){h=(Wide(l.window_width)*d+n/2)/n;if(!h)h=1;}
        else{w=(Wide(l.window_height)*n+d/2)/d;if(!w)w=1;}
        r=movie_destination(p,l);CHECK(r&&r->width==w&&r->height==h&&r->x==(l.window_width-unsigned(w))/2&&r->y==(l.window_height-unsigned(h))/2);
    }
    p.alpha=MovieAlpha::unspecified;CHECK(validate_movie_present(p)==MovieGpuError::unsupported_effect);
    p.alpha=MovieAlpha::replace_rgba8;p.sample.x=1;CHECK(validate_movie_present(p)==MovieGpuError::invalid_plan);
    MovieRgbaFrame invalid;CHECK(validate_movie_upload(invalid)==MovieGpuError::invalid_frame);
    invalid.width=UINT32_MAX;invalid.height=UINT32_MAX;invalid.conversion_role=MovieColorRole::helper_default;CHECK(validate_movie_upload(invalid)==MovieGpuError::invalid_frame);
    std::puts("PASS movie native layout:10000 full-range SAR/window cases vs independent128-bit oracle and explicit-policy failures");return 0;
}
