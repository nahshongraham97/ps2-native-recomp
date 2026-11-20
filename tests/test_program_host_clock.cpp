#include "mka/program_host_clock.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
void conversion(){
 ProgramHostClockFailure f;std::int64_t tick=100;unsigned calls=0;auto clock=NativeProgramHostClock::create_with_reader(3000,[&](auto&)->std::optional<std::int64_t>{++calls;return tick;},f);check(clock&&calls==1,"creation captures exactly one explicit origin");
 check(clock->read(f)->milliseconds==1,"zero maps to one");tick=102;check(clock->read(f)->milliseconds==1,"fraction below one ms floors before zero mapping");tick=103;check(clock->read(f)->milliseconds==1,"exact one ms");tick=108;check(clock->read(f)->milliseconds==2,"fractional ms floors");
 auto second_user=clock;tick=130;check(second_user->read(f)->milliseconds==10&&clock->read(f)->milliseconds==10,"multiple users share same origin and accepted counter");
}
void wrap_and_wide(){
 ProgramHostClockFailure f;std::int64_t tick=0;auto clock=NativeProgramHostClock::create_with_reader(1000,[&](auto&)->std::optional<std::int64_t>{return tick;},f);check(bool(clock),"word clock");tick=0xffffffffLL;check(clock->read(f)->milliseconds==0xffffffffu,"last nonzero word");tick=0x100000000LL;check(clock->read(f)->milliseconds==1,"wrapped zero remapped one");tick=0x100000002LL;check(clock->read(f)->milliseconds==2,"word wrap intentionally retained");
 tick=std::numeric_limits<std::int64_t>::min();auto wide=NativeProgramHostClock::create_with_reader(1000,[&](auto&)->std::optional<std::int64_t>{return tick;},f);check(bool(wide),"negative host origin accepted without subtraction overflow");tick=std::numeric_limits<std::int64_t>::max();check(wide->read(f)->milliseconds==0xffffffffu,"full signed64 endpoint delta computed wide");
 tick=0;auto scale=NativeProgramHostClock::create_with_reader(1,[&](auto&)->std::optional<std::int64_t>{return tick;},f);tick=std::numeric_limits<std::int64_t>::max();check(scale->read(f)->milliseconds==0xfffffc18u,"wide multiplication before intentional lowword conversion");
}
void failures(){
 ProgramHostClockFailure f;check(!NativeProgramHostClock::create_with_reader(0,[](auto&)->std::optional<std::int64_t>{return 0;},f)&&f.kind==ProgramHostClockError::invalid_frequency,"zero frequency rejected");check(!NativeProgramHostClock::create_with_reader(-1,{},f)&&f.kind==ProgramHostClockError::invalid_frequency,"negative frequency rejected");check(!NativeProgramHostClock::create_with_reader(1000,{},f)&&f.kind==ProgramHostClockError::invalid_source,"absent reader rejected");
 check(!NativeProgramHostClock::create_with_reader(1000,[](auto&)->std::optional<std::int64_t>{return {};},f)&&f.kind==ProgramHostClockError::query,"origin query failure named");check(!NativeProgramHostClock::create_with_reader(1000,[](auto&)->std::optional<std::int64_t>{throw std::runtime_error("origin");},f)&&f.kind==ProgramHostClockError::native_exception,"origin exception named");
 std::int64_t tick=10;unsigned mode=0;auto clock=NativeProgramHostClock::create_with_reader(1000,[&](ProgramHostClockFailure& failure)->std::optional<std::int64_t>{if(mode==1){failure={ProgramHostClockError::query,77};return {};}if(mode==2)throw std::runtime_error("read");if(mode==3){failure={ProgramHostClockError::query,88};return 9999;}return tick;},f);check(bool(clock),"failure test owner");tick=20;check(clock->read(f)->milliseconds==10,"accepted read");tick=19;check(!clock->read(f)&&f.kind==ProgramHostClockError::regressed,"counter regression named");mode=1;check(!clock->read(f)&&f.kind==ProgramHostClockError::query&&f.native_code==77,"reader diagnostic preserved");mode=2;check(!clock->read(f)&&f.kind==ProgramHostClockError::native_exception,"reader exception named");mode=3;check(!clock->read(f)&&f.kind==ProgramHostClockError::query&&f.native_code==88,"reader failure with value rejected");mode=0;tick=20;check(clock->read(f)->milliseconds==10&&f.kind==ProgramHostClockError::none&&f.native_code==0,"failures do not advance last accepted tick and next read clears failure");tick=19;check(!clock->read(f)&&f.kind==ProgramHostClockError::regressed,"failed lower tick did not lower acceptance boundary");
}
void recursive_reader(){
 ProgramHostClockFailure f;std::shared_ptr<NativeProgramHostClock> clock;std::int64_t tick=10;unsigned nested=0;
 clock=NativeProgramHostClock::create_with_reader(1000,[&](ProgramHostClockFailure&)->std::optional<std::int64_t>{if(clock){ProgramHostClockFailure inner;check(!clock->read(inner)&&inner.kind==ProgramHostClockError::reentry,"same-thread recursive reader rejected before mutex");++nested;}return tick;},f);check(bool(clock),"recursive reader captures one origin before owner publication");tick=15;check(clock->read(f)->milliseconds==5&&f.kind==ProgramHostClockError::none&&nested==1,"outer read survives nested rejection");tick=20;check(clock->read(f)->milliseconds==10&&nested==2,"RAII sampling guard clears after prior reader");
}
void concurrent(){
 ProgramHostClockFailure f;std::int64_t tick=0;std::atomic<unsigned> inside=0,calls=0;auto clock=NativeProgramHostClock::create_with_reader(1000,[&](auto&)->std::optional<std::int64_t>{check(inside.fetch_add(1)==0,"reader calls serialized across native threads");++calls;auto result=tick++;inside.fetch_sub(1);return result;},f);check(bool(clock),"concurrent owner");
 std::vector<std::thread> workers;std::atomic<unsigned> accepted=0;for(unsigned n=0;n<8;++n)workers.emplace_back([clock,&accepted]{for(unsigned i=0;i<100;++i){ProgramHostClockFailure failure;auto sample=clock->read(failure);check(sample&&sample->milliseconds>=1&&failure.kind==ProgramHostClockError::none,"concurrent shared owner reading");++accepted;}});for(auto& worker:workers)worker.join();check(accepted==800&&calls==801&&clock->read(f)->milliseconds==801,"one origin and all serialized reads share one native timeline");
}
}
int main(){conversion();wrap_and_wide();failures();recursive_reader();concurrent();std::puts("PASS_NATIVE_PROGRAM_HOST_CLOCK: shared explicit epoch/integer milliseconds/wrap/failure atomicity/reentry/concurrent readers; no original kernel epoch or physical timing fidelity claim");return std::fflush(stdout)||std::ferror(stdout)?1:0;}
