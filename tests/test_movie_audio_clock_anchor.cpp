#include "mka/movie_audio_clock_anchor.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::video;
namespace {int fail_after=-1;void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}}
void* operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){try{
    auto id=std::make_shared<const MovieSourceObservationIdentity>();std::string key(80,'x');MovieAudioClockAnchorFailure f;
    mka::sound::AudioQueueStatus q{10,0,10,0,0,false};mka::sound::AudioClockReading c{0,10,123,0,false};
    auto nonzero=c;nonzero.position=1;check(!NativeMovieAudioClockAnchor::create(key,id,q,nonzero,f)&&f.kind==MovieAudioClockAnchorError::invalid_origin,"first observed nonzero cannot invent zero anchor");
    auto started=q;started.started=true;check(!NativeMovieAudioClockAnchor::create(key,id,started,c,f)&&f.kind==MovieAudioClockAnchorError::invalid_origin,"must anchor actual stopped empty stream");
    auto owner=NativeMovieAudioClockAnchor::create(key,id,q,c,f);check(bool(owner),"fresh actual stream coordinate0");auto zero=owner->observe(q,c,f);check(zero&&zero->elapsed==MovieClockRational{0,10}&&!zero->active&&zero->output_clock&&!zero->output_clock->precise,"S_FALSE valid imprecise zero, no precision fabrication");
    q.started=true;q.submitted_frames=10;q.padding_frames=7;q.writable_frames=3;c.position=3;c.qpc_100ns=777;c.precise=true;auto first=owner->observe(q,c,f);check(first&&first->elapsed==MovieClockRational{3,10}&&first->active&&first->anchor==zero->anchor&&first->output_clock->qpc_100ns==777,"first nonzero uses original zero, never subtracts initial query");
    q.started=false;c.position=4;auto stopped=owner->observe(q,c,f);check(stopped&&stopped->elapsed==MovieClockRational{4,10}&&!stopped->active,"actual stopped observation retains elapsed");c.qpc_100ns=9999;check(owner->observe(q,c,f)->elapsed==MovieClockRational{4,10},"QPC changes cannot advance device elapsed");q.started=true;c.position=5;check(owner->observe(q,c,f)->elapsed==MovieClockRational{5,10},"resume continues same device origin");
    auto bad=c;bad.position=4;check(!owner->observe(q,bad,f)&&f.kind==MovieAudioClockAnchorError::regression,"position regression rejected");bad=c;bad.frequency=11;check(!owner->observe(q,bad,f)&&f.kind==MovieAudioClockAnchorError::frequency,"frequency change rejected");auto foreign=q;foreign.reset_epoch=1;bad=c;bad.reset_epoch=1;check(!owner->observe(foreign,bad,f)&&f.kind==MovieAudioClockAnchorError::epoch,"unacknowledged reset cannot replace origin");
    q.started=false;auto last=owner->observe(q,c,f);q={10,0,10,0,1,false};c={0,10,12000,1,true};auto cancelled=owner->reset(q,c,f);check(cancelled&&cancelled->cancelled&&cancelled->anchor==last->anchor&&cancelled->output_clock->reset_epoch==0&&cancelled->elapsed==MovieClockRational{5,10},"cancelled old epoch remains separate actual facts");auto fresh=owner->observe(q,c,f);check(fresh&&fresh->anchor!=cancelled->anchor&&fresh->output_clock->reset_epoch==1&&fresh->elapsed==MovieClockRational{0,10},"confirmed reset creates distinct zero anchor");
    fail_after=0;auto rejected=owner->observe(q,c,f);fail_after=-1;check(!rejected&&f.kind==MovieAudioClockAnchorError::allocation,"owned sample allocation failure named");check(owner->observe(q,c,f)->anchor==fresh->anchor,"allocation cannot change anchor/state");
    MovieAudioClockAnchorFailure thread_failure;std::thread t([&]{owner->observe(q,c,thread_failure);});t.join();check(thread_failure.kind==MovieAudioClockAnchorError::wrong_thread,"output owner thread enforced");owner.reset();check(cancelled->source==id&&cancelled->source_key==key&&cancelled->output_clock->position==5,"owned cancelled sample survives producer teardown");
    q={10,0,10,0,0,false};c={0,UINT64_MAX,0,0,true};auto wide=NativeMovieAudioClockAnchor::create(key,id,q,c,f);check(bool(wide),"wide native frequency reducible zero");q.started=true;c.position=UINT64_MAX;auto exact=wide->observe(q,c,f);check(exact&&exact->elapsed==MovieClockRational{1,1}&&exact->output_clock->position==UINT64_MAX,"native wide exact p/f reduction without device cap");
    check(std::puts("PASS native output clock zero anchor/lifecycle/exact rational; no acoustic or retail clock claim")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL audio anchor %s\n",e.what());return 1;}}
