#include "mka/movie_audio_progress.hpp"
#include <cstdio>
#include <stdexcept>
using namespace mka::video;
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
    MovieAudioProgressFailure f;
    auto p=NativeMovieAudioProgress::create({"owned-source",100,48000,48000,0},f);require(bool(p),"create");
    require(p->accept("owned-source",48000,{100,7,7,0,{0,3,0}},f),"first chunk");
    require(p->accept("owned-source",48000,{100,7,7,3,{3,4,0}},f),"second chunk");
    require(!p->accept("foreign-source",48000,{107,1,1,0,{7,1,0}},f)&&f.kind==MovieAudioProgressError::foreign_source,"foreign receipt same coordinates rejected");
    require(!p->accept("owned-source",44100,{107,1,1,0,{7,1,0}},f)&&f.kind==MovieAudioProgressError::source_rate_mismatch,"actual source rate rejected");
    auto unchanged=p->snapshot(f);require(unchanged&&unchanged->submitted==7&&unchanged->retired==0&&unchanged->pending==7&&unchanged->source_boundary==100&&unchanged->epoch==0&&unchanged->source_identity=="owned-source"&&unchanged->source_rate==48000&&unchanged->paused&&!unchanged->cancelled,"foreign inputs preserve complete state");
    auto s=p->observe({8,2,6,7,0,false},f);require(s&&s->retired==5&&s->source_boundary==105&&s->pending==2,"literal partial retirement");
    std::puts("{\"case\":\"partial\",\"submitted\":7,\"padding\":2,\"retired\":5,\"source_boundary\":105}");
    require(!p->observe({8,3,5,7,0,false},f)&&f.kind==MovieAudioProgressError::regression,"regression");
    require(p->snapshot(f)->source_boundary==105,"failure atomic");
    require(!p->accept("owned-source",48000,{107,1,2,0,{7,1,0}},f)&&f.kind==MovieAudioProgressError::invalid_receipt,"resampled packet rejected");
    require(!p->accept("owned-source",48000,{108,1,1,0,{7,1,0}},f)&&f.kind==MovieAudioProgressError::noncontiguous,"source gap");
    require(!p->observe({8,0,8,7,1,false},f)&&f.kind==MovieAudioProgressError::epoch_mismatch,"foreign epoch");
    require(!p->observe({8,2,6,7,0,true},f)&&f.kind==MovieAudioProgressError::invalid_status,"paused started mismatch");
    auto old=p->reset({"owned-source",200,48000,48000,1},{8,0,8,0,1,false},f);
    require(old&&old->cancelled&&old->source_boundary==105&&old->pending==2,"reset cancels at observed boundary");
    require(p->snapshot(f)->source_boundary==200&&p->snapshot(f)->submitted==0,"reset new explicit origin");
    require(p->accept("owned-source",48000,{200,2,2,0,{0,2,1}},f),"new epoch");
    require(p->pause(false,f),"reported resume");
    auto end=p->observe({8,0,8,2,1,true},f);require(end&&end->source_boundary==202,"underrun bound");
    require(p->observe({8,0,8,2,1,true},f)->source_boundary==202,"no time extrapolation");
    require(p->pause(true,f),"reported stop");
    require(!p->reset({"owned-source",300,48000,48000,2},{8,1,7,0,2,false},f),"nonempty reset status");
    NativeMovieAudioProgress moved(std::move(*p));require(!p->snapshot(f)&&f.kind==MovieAudioProgressError::invalid_owner,"moved source");
    moved=std::move(moved);require(moved.snapshot(f)->source_boundary==202,"self move");p.reset();require(old->source_identity=="owned-source"&&old->source_boundary==105,"owned old snapshot");
    auto wide=NativeMovieAudioProgress::create({"wide",UINT64_MAX-1,1,1,0},f);require(wide->accept("wide",1,{UINT64_MAX-1,1,1,0,{0,1,0}},f),"native64 endpoint");
    require(!wide->accept("wide",1,{UINT64_MAX,1,1,0,{1,1,0}},f)&&f.kind==MovieAudioProgressError::overflow,"range overflow");
    require(!NativeMovieAudioProgress::create({"x",0,48000,44100,0},f)&&f.kind==MovieAudioProgressError::unsupported_rate,"rate mismatch");
    std::puts("PASS pure source retirement; no device/clock/player binding");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
