#include "mka/movie_clock_history.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <utility>
#include <vector>
using namespace mka::video;
static void check(bool ok,const char* message) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static void trace(const char* name,const char* op,std::optional<std::int64_t> value,
                  const NativeMovieClockHistory& h,std::optional<std::int64_t> result,
                  const MovieClockHistoryFailure& f) {
    std::printf("{\"case\":\"%s\",\"window_size\":%zu,\"op\":\"%s\",\"value\":",name,h.window().size(),op);
    if(value) std::printf("%lld",static_cast<long long>(*value)); else std::printf("null");
    std::printf(",\"ok\":%s,\"error\":\"%s\",\"mean\":",result?"true":"false",movie_clock_history_error_name(f.kind));
    if(result) std::printf("%lld",static_cast<long long>(*result)); else std::printf("null");
    std::printf(",\"cursor\":%zu,\"window\":[",h.cursor());
    bool first=true; for(auto v:h.window()) { std::printf("%s%lld",first?"":",",static_cast<long long>(v)); first=false; }
    std::printf("]}\n");
}
int main() {
    MovieClockHistoryFailure f;
    check(!NativeMovieClockHistory::create(0,f)&&f.kind==MovieClockHistoryError::invalid_window,"zero window");
    check(!NativeMovieClockHistory::create(std::numeric_limits<std::size_t>::max(),f)&&f.kind==MovieClockHistoryError::storage_size,"unrepresentable storage");
    for(std::size_t n:{1u,10u,61u,256u}) {
        auto owned=NativeMovieClockHistory::create(n,f); check(owned&&f.kind==MovieClockHistoryError::none,"create");
        auto& h=*owned; const char* name=n==1?"n1":n==10?"n10":n==61?"n61":"n256";
        auto a=h.mean(f); check(a&&*a==0,"zero filled mean");trace(name,"create",{},h,a,f);
        a=h.insert(-9,f);check(a&&*a==(-9/static_cast<std::int64_t>(n)),"negative warmup truncates zero");trace(name,"insert",-9,h,a,f);
        for(std::size_t i=1;i<n+3;++i) {const auto v=static_cast<std::int64_t>(i)*3-17;a=h.insert(v,f);check(a&&f.kind==MovieClockHistoryError::none,"insert trace");trace(name,"insert",v,h,a,f);}
        check(h.cursor()==3%n,"ring cursor");
        if(n==10) { const std::vector<std::int64_t> expected{13,16,19,-8,-5,-2,1,4,7,10};check(std::vector<std::int64_t>(h.window().begin(),h.window().end())==expected,"literal physical ring order");check(a&&*a==5,"literal ring mean"); }
        const auto cursor=h.cursor();a=h.recenter(7,f);check(a&&h.cursor()==cursor,"recenter preserves cursor");trace(name,"recenter",7,h,a,f);
        check(h.clear(f),"clear");a=h.mean(f);check(a&&*a==0&&h.cursor()==0,"clear only kernel");trace(name,"clear",{},h,a,f);
    }
    auto o=NativeMovieClockHistory::create(2,f);check(o.has_value(),"extreme create");auto& h=*o;
    trace("extremes","create",{},h,h.mean(f),f);
    constexpr auto hi=std::numeric_limits<std::int64_t>::max();constexpr auto lo=std::numeric_limits<std::int64_t>::min();
    auto a=h.insert(hi,f);check(a&&*a==hi/2,"wide first");trace("extremes","insert",hi,h,a,f);
    a=h.insert(hi,f);check(a&&*a==hi,"sum beyond int64 positive");trace("extremes","insert",hi,h,a,f);
    auto prior=std::vector<std::int64_t>(h.window().begin(),h.window().end());auto cursor=h.cursor();
    a=h.recenter(-1,f);check(!a&&f.kind==MovieClockHistoryError::overflow&&h.cursor()==cursor&&std::vector<std::int64_t>(h.window().begin(),h.window().end())==prior,"overflow transaction");trace("extremes","recenter",-1,h,a,f);
    a=h.insert(lo,f);check(a&&*a==0,"mixed extremes negative half truncates zero");trace("extremes","insert",lo,h,a,f);
    a=h.insert(lo,f);check(a&&*a==lo,"sum beyond int64 negative");trace("extremes","insert",lo,h,a,f);
    a=h.recenter(1,f);check(!a&&f.kind==MovieClockHistoryError::overflow,"negative overflow");trace("extremes","recenter",1,h,a,f);
    check(h.clear(f),"clear extremes");a=h.mean(f);trace("extremes","clear",{},h,a,f);
    a=h.insert(0,f);trace("extremes","insert",0,h,a,f);a=h.insert(lo,f);trace("extremes","insert",lo,h,a,f);prior.assign(h.window().begin(),h.window().end());cursor=h.cursor();
    a=h.recenter(1,f);check(!a&&h.cursor()==cursor&&std::vector<std::int64_t>(h.window().begin(),h.window().end())==prior,"later-slot overflow leaves earlier slot unchanged");trace("extremes","recenter",1,h,a,f);
    NativeMovieClockHistory moved(std::move(h));check(moved.valid()&&!h.valid(),"move construction owner");
    a=h.insert(2,f);check(!a&&f.kind==MovieClockHistoryError::invalid_owner,"moved insert");trace("moved_from","insert",2,h,a,f);
    check(!h.clear(f)&&f.kind==MovieClockHistoryError::invalid_owner,"moved clear");
    check(!h.recenter(0,f)&&f.kind==MovieClockHistoryError::invalid_owner,"moved recenter");
    check(!h.mean(f)&&f.kind==MovieClockHistoryError::invalid_owner,"moved mean");
    auto destination=NativeMovieClockHistory::create(1,f);check(destination.has_value(),"move destination");*destination=std::move(moved);
    check(destination->valid()&&!moved.valid()&&destination->window().size()==2,"move assignment");
    auto* self=&*destination;*destination=std::move(*self);check(destination->valid()&&destination->window().size()==2,"self move");
    a=destination->mean(f);check(a&&*a==lo/2,"moved values preserved");trace("move_destination","mean",{},*destination,a,f);
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"trace output");
    std::fprintf(stderr,"PASS: native history kernel literals and operation trace; full clock/player untested\n");
}
