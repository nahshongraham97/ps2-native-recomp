#include "mka/movie_clock_history.hpp"
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>
using namespace mka::video;
static void check(bool b,const char* message) { if(!b) { std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1); } }
static void emit(std::size_t n,const char* owner,const char* op,std::optional<std::int64_t> value,
                 const NativeMovieClockHistory& h,MovieClockHistoryFailure failure) {
    MovieClockHistoryFailure mf;auto mean=h.mean(mf);
    std::printf("{\"n\":%zu,\"owner\":\"%s\",\"operation\":\"%s\",\"value\":",n,owner,op);
    if(value) std::printf("%lld",static_cast<long long>(*value));else std::printf("null");
    std::printf(",\"error\":\"%s\",\"valid\":%s,\"mean\":",movie_clock_history_error_name(failure.kind),h.valid()?"true":"false");
    if(mean) std::printf("%lld",static_cast<long long>(*mean));else std::printf("null");
    std::printf(",\"cursor\":%zu,\"window\":[",h.cursor());bool first=true;
    for(auto v:h.window()) { std::printf("%s%lld",first?"":",",static_cast<long long>(v));first=false; }
    std::printf("]}\n");
}
int main() {
    // Fixed request plan: for each N, create; N+3 inserts (7*i-11);
    // clone; source insert101; clone recenter3; source clear; clone insert-77;
    // move source; reject clone on moved source; self-move clone; destroy source.
    for(std::size_t n:{1u,10u,61u,256u}) {
        MovieClockHistoryFailure f;auto source=NativeMovieClockHistory::create(n,f);check(source.has_value(),"create");emit(n,"source","create",{},*source,f);
        for(std::size_t i=0;i<n+3;++i) {auto v=static_cast<std::int64_t>(i)*7-11;check(source->insert(v,f).has_value(),"fill");emit(n,"source","insert",v,*source,f);}
        auto clone=source->clone(f);check(clone.has_value(),"clone");check(clone->cursor()==source->cursor()&&std::vector<std::int64_t>(clone->window().begin(),clone->window().end())==std::vector<std::int64_t>(source->window().begin(),source->window().end()),"equal physical state");check(clone->window().data()!=source->window().data(),"distinct storage");emit(n,"clone","clone",{},*clone,f);
        auto saved=std::vector<std::int64_t>(clone->window().begin(),clone->window().end());check(source->insert(101,f).has_value(),"source modification");check(std::vector<std::int64_t>(clone->window().begin(),clone->window().end())==saved,"source isolation");emit(n,"source","insert",101,*source,f);
        saved.assign(source->window().begin(),source->window().end());check(clone->recenter(3,f).has_value(),"clone modification");check(std::vector<std::int64_t>(source->window().begin(),source->window().end())==saved,"clone isolation");emit(n,"clone","recenter",3,*clone,f);
        check(source->clear(f),"source clear");emit(n,"source","clear",{},*source,f);check(clone->insert(-77,f).has_value(),"clone insert");emit(n,"clone","insert",-77,*clone,f);
        NativeMovieClockHistory moved(std::move(*source));emit(n,"moved","move_source",{},moved,{});
        check(!source->clone(f)&&f.kind==MovieClockHistoryError::invalid_owner,"moved clone rejects");emit(n,"source","clone_rejected",{},*source,f);
        const auto cursor=clone->cursor();saved.assign(clone->window().begin(),clone->window().end());auto* self=&*clone;*clone=std::move(*self);check(clone->cursor()==cursor&&std::vector<std::int64_t>(clone->window().begin(),clone->window().end())==saved,"self move retained");emit(n,"clone","self_move",{},*clone,{});
        source.reset();check(clone->mean(f).has_value(),"clone survives source destruction");emit(n,"clone","source_destroyed",{},*clone,f);
    }
    check(std::fflush(stdout)==0&&!std::ferror(stdout),"complete output");std::fprintf(stderr,"PASS: owned history clone isolation and trace; no clock/player\n");
}
