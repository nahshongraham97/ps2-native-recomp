#include "mka/movie_descriptor_rewrite.hpp"
#include <cstdio>
#include <limits>
#include <initializer_list>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
using namespace mka::video;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
bool same(const MovieRewriteDescriptor& a,const MovieRewriteDescriptor& b){
    const auto& x=a.timecode;const auto& y=b.timecode;
    return std::tie(x.frame_rate_code,x.drop_frame,x.hours,x.minutes,x.seconds,x.pictures,x.temporal_reference,x.half_frame_adjustment,a.flag)==
        std::tie(y.frame_rate_code,y.drop_frame,y.hours,y.minutes,y.seconds,y.pictures,y.temporal_reference,y.half_frame_adjustment,b.flag);
}
bool same(const std::optional<MovieDescriptorCache>& a,const std::optional<MovieDescriptorCache>& b){return bool(a)==bool(b)&&(!a||(same(a->descriptor,b->descriptor)&&a->relative==b->relative));}
bool same(const MovieDescriptorRewriteState& a,const MovieDescriptorRewriteState& b){return a.enabled==b.enabled&&a.previous_gop_serial==b.previous_gop_serial&&a.first_baseline==b.first_baseline&&same(a.cache,b.cache)&&same(a.published,b.published);}
MovieRewriteDescriptor descriptor(std::int64_t frames=0,std::int64_t temporal=0,std::int64_t flag=0,std::int64_t half=0){return {{5,false,0,0,0,frames,temporal,half},flag};}
MovieDescriptorRewriteInput input(MovieRewriteDescriptor d,std::uint64_t serial=1){return {d,{},serial,false,5,false};}
MovieDescriptorRewriteState state(NativeMovieDescriptorRewrite& owner){MovieDescriptorRewriteFailure f;auto s=owner.snapshot(f);check(bool(s)&&f.kind==MovieDescriptorRewriteError::none,"owned state");return *s;}
MovieDescriptorRewriteResult update(NativeMovieDescriptorRewrite& owner,MovieDescriptorRewriteInput i){MovieDescriptorRewriteFailure f;auto r=owner.update(i,f);check(bool(r)&&f.kind==MovieDescriptorRewriteError::none,"successful typed update");return *r;}
}
int main(){try{
    static_assert(!std::is_copy_constructible_v<NativeMovieDescriptorRewrite>);
    static_assert(std::is_nothrow_move_constructible_v<NativeMovieDescriptorRewrite>);
    MovieDescriptorRewriteFailure failure;
    {NativeMovieDescriptorRewrite owner;auto s=state(owner);check(!s.enabled&&!s.previous_gop_serial&&!s.first_baseline&&!s.cache&&!s.published,"actual fresh flags/sentinels distinct");
     auto r=update(owner,input(descriptor(0,2,7,1)));check(r.branch==MovieDescriptorRewriteBranch::direct&&r.gop_changed&&!r.enabled_latched&&r.cache_updated,"first nonzero serial/no timestamp retains direct route");
     check(r.first_baseline==MovieTimecodeRational{500,30000}&&r.published.relative==MovieTimecodeRational{2000,30000}&&r.published.descriptor.timecode.temporal_reference==2&&r.published.descriptor.flag==7,"baseline zeroes temporary temporal only, retains half and full published raw fields");
     auto held=r;auto equal=update(owner,input(descriptor(0,2,99,1)));check(equal.cache_updated&&state(owner).cache->descriptor.flag==99,"equal numerator refreshes entire raw cache rather than keeping stale flag");
     auto lower=update(owner,input(descriptor(0,1,3,0)));check(!lower.cache_updated&&state(owner).cache->descriptor.flag==99&&same(held.published.descriptor,descriptor(0,2,7,1)),"smaller numerator leaves cache, retained result immutable");}
    {NativeMovieDescriptorRewrite owner;auto first=update(owner,input(descriptor(9,2,7,1),0));check(first.enabled_latched&&first.branch==MovieDescriptorRewriteBranch::zero_origin&&first.published.descriptor.timecode.pictures==0,"serial0 automatically enables and zeros only origin words without cache");
     auto second=update(owner,input(descriptor(5,4,11,3),0));const auto& d=second.published.descriptor;
     check(second.branch==MovieDescriptorRewriteBranch::cached&&d.timecode.pictures==0&&d.timecode.temporal_reference==4&&d.timecode.half_frame_adjustment==3&&d.flag==11&&second.published.relative.numerator==5000,"six-word cache copy retains CURRENT temporal/half/flag");
     auto third=update(owner,input(descriptor(8,6,-5),1));check(third.branch==MovieDescriptorRewriteBranch::incremented&&third.published.descriptor.timecode.pictures==12&&third.published.descriptor.timecode.temporal_reference==6&&third.published.descriptor.timecode.half_frame_adjustment==0&&third.published.descriptor.flag==-5,"increment uses PRIOR flag/half/TR but keeps CURRENT flag/TR");}
    {NativeMovieDescriptorRewrite owner;update(owner,input(descriptor(0,0,-2,1),0));auto r=update(owner,input(descriptor(9,7,99),1));check(r.published.descriptor.timecode.pictures==1&&r.published.descriptor.timecode.half_frame_adjustment==-1&&r.published.descriptor.timecode.temporal_reference==7&&r.published.descriptor.flag==99,"negative odd half sum divides toward zero, not floor");}
    {NativeMovieDescriptorRewrite owner;auto i=input(descriptor(0,0,42));i.current.timecode.hours=9;i.timestamp22500=225000; i.current.timecode.temporal_reference=3;
     auto r=update(owner,i);const auto& d=r.published.descriptor;
     check(r.branch==MovieDescriptorRewriteBranch::position&&r.enabled_latched&&d.timecode.hours==0&&d.timecode.minutes==0&&d.timecode.seconds==9&&d.timecode.pictures==27&&d.timecode.temporal_reference==3&&d.flag==42&&r.first_baseline==MovieTimecodeRational{297000,30000}&&r.published.relative==MovieTimecodeRational{3000,30000},"actual quarterPTS conversion precedes baseline, preserving separate flag");
     auto before=state(owner);i.current.timecode.temporal_reference=1024;i.gop_serial=2;check(!owner.update(i,failure)&&failure.kind==MovieDescriptorRewriteError::invalid_temporal&&same(before,state(owner)),"unsupported position temporal rejects before state mutation");}
    {NativeMovieDescriptorRewrite owner;auto i=input(descriptor());i.current.timecode.seconds=10;update(owner,i);
     i.gop_serial=2;i.current.timecode.seconds=11;auto inside=update(owner,i);check(!state(owner).enabled&&inside.branch==MovieDescriptorRewriteBranch::direct,"strictly inside five-second prior interval does not enable");
     i.gop_serial=3;i.current.timecode.seconds=16;auto edge=update(owner,i);check(edge.enabled_latched&&edge.branch==MovieDescriptorRewriteBranch::incremented&&edge.published.descriptor.timecode.seconds==11&&edge.published.descriptor.timecode.pictures==1,"exact comparison margin enables then increments prior cache, not current future GOP");}
    {NativeMovieDescriptorRewrite owner;auto i=input(descriptor());i.current.timecode.seconds=10;update(owner,i);i.gop_serial=2;i.current.timecode.seconds=9;auto r=update(owner,i);check(r.enabled_latched&&r.branch==MovieDescriptorRewriteBranch::incremented,"nonincreasing converted source descriptor enables automatic rewrite");}
    {NativeMovieDescriptorRewrite owner;auto i=input(descriptor());i.repeat=true;i.preserve_direct_without_cache=true;i.current.timecode.seconds=4;auto r=update(owner,i);check(r.enabled_latched&&r.branch==MovieDescriptorRewriteBranch::direct&&r.published.descriptor.timecode.seconds==4,"repeat enables; explicit no-cache preserve predicate retains direct origin");}
    {NativeMovieDescriptorRewrite owner;auto first=update(owner,input(descriptor(0,-2)));check(!first.cache_updated&&!state(owner).cache&&first.published.relative.numerator==-2000,"inactive cache comparison seed is minus one, not zero or automatic first cache");
     check(owner.set_rewrite_enabled(true,failure),"explicit source config mutation");auto i=input(descriptor(5,-3));i.preserve_direct_without_cache=true;auto r=update(owner,i);check(r.branch==MovieDescriptorRewriteBranch::direct&&r.published.relative.numerator==2000&&r.cache_updated&&state(owner).cache&&state(owner).cache->relative.numerator==2000,"preserved direct five-minus-three publishes positive2000 and MUST refresh inactive cache");}
    {NativeMovieDescriptorRewrite owner;update(owner,input(descriptor(0,-2)));check(owner.set_rewrite_enabled(true,failure),"separate negative inactive-cache enable");auto i=input(descriptor(0,-3));i.preserve_direct_without_cache=true;auto r=update(owner,i);check(r.branch==MovieDescriptorRewriteBranch::direct&&r.published.relative.numerator==-3000&&!r.cache_updated&&!state(owner).cache,"preserved direct negative3000 remains below inactive seed minus one; cache stays inactive");}
    {NativeMovieDescriptorRewrite owner;auto r=update(owner,input(descriptor(0,-1,0,2)));check(r.published.relative.numerator==-1000,"negative published relative descriptor is explicit numeric value");}
    for(unsigned code=1;code<=8;++code){
        constexpr std::int64_t nominal[]={0,24,24,25,30,30,50,60,60};NativeMovieDescriptorRewrite owner;
        auto i=input(descriptor(nominal[code]-1));i.current.timecode.frame_rate_code=static_cast<std::uint8_t>(code);update(owner,i);
        check(owner.set_rewrite_enabled(true,failure),"explicit increment fixture enable");i.gop_serial=2;i.current.timecode.pictures=0;
        auto r=update(owner,i);check(r.published.descriptor.timecode.seconds==1&&r.published.descriptor.timecode.pictures==0,"original literal nominal table rollover for every admitted rate");
    }
    for(std::int64_t minute:{0,9}){NativeMovieDescriptorRewrite owner;auto i=input(descriptor(29));i.current.timecode.frame_rate_code=4;i.current.timecode.drop_frame=true;i.current.timecode.minutes=minute;i.current.timecode.seconds=59;update(owner,i);check(owner.set_rewrite_enabled(true,failure),"drop fixture explicit enable");i.gop_serial=2;
     auto r=update(owner,i);check(r.published.descriptor.timecode.minutes==minute+1&&r.published.descriptor.timecode.seconds==0&&r.published.descriptor.timecode.pictures==(minute==0?2:0),"source two-label clamp except tenth minute");}
    {NativeMovieDescriptorRewrite owner;auto i=input(descriptor(29));i.current.timecode.minutes=59;i.current.timecode.seconds=59;update(owner,i);check(owner.set_rewrite_enabled(true,failure),"hour fixture enable");i.gop_serial=2;auto r=update(owner,i);check(r.published.descriptor.timecode.hours==1&&r.published.descriptor.timecode.minutes==0&&r.published.descriptor.timecode.seconds==0&&r.published.descriptor.timecode.pictures==0,"carry reaches hour exactly");}
    {NativeMovieDescriptorRewrite owner;update(owner,input(descriptor()));auto before=state(owner);auto i=input(descriptor());i.current.timecode.frame_rate_code=0;check(!owner.update(i,failure)&&failure.kind==MovieDescriptorRewriteError::unsupported_rate&&same(before,state(owner)),"nominal divisor zero rejected atomically");i.current.timecode.frame_rate_code=9;check(!owner.update(i,failure)&&failure.kind==MovieDescriptorRewriteError::unsupported_rate&&same(before,state(owner)),"unknown rate rejected atomically");
     i.current.timecode.frame_rate_code=5;i.current.timecode.hours=INT64_MAX;check(!owner.update(i,failure)&&failure.kind==MovieDescriptorRewriteError::timecode&&failure.timecode.kind==MovieTimecodeError::overflow&&same(before,state(owner)),"conversion overflow preserves all owner fields");}
    {NativeMovieDescriptorRewrite owner;update(owner,input(descriptor(0,0,INT64_MAX),0));check(owner.set_rewrite_enabled(true,failure),"large flag fixture");auto before=state(owner);auto i=input(descriptor(),1);check(!owner.update(i,failure)&&same(before,state(owner)),"large native flag uses wide arithmetic then reports final conversion overflow atomically");}
    {NativeMovieDescriptorRewrite owner;auto held=update(owner,input(descriptor(0,2,7,1)));auto before=state(owner);NativeMovieDescriptorRewrite moved(std::move(owner));check(!owner.snapshot(failure)&&failure.kind==MovieDescriptorRewriteError::invalid_owner&&!owner.update(input(descriptor()),failure)&&!owner.set_rewrite_enabled(true,failure),"all moved owner operations reject");moved=std::move(moved);check(same(before,state(moved)),"self move retains complete owner");NativeMovieDescriptorRewrite assigned;assigned=std::move(moved);check(same(before,state(assigned))&&!moved.snapshot(failure)&&held.published.descriptor.flag==7,"move assignment and immutable returned lifetime");}
    check(std::puts("PASS native descriptor rewrite cache and baseline literals; no retail admission or clock selection")>=0&&std::fflush(stdout)==0,"checked footer");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native descriptor rewrite %s\n",e.what());return 1;}}
