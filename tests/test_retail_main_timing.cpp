#include "mka/retail_main_timing.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
namespace {
bool fail_allocation=false;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
void* operator new(std::size_t size){if(fail_allocation)throw std::bad_alloc();if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){try{
    RetailCrtFailure cf;auto crt=NativeRetailCrt::create(cf);check(bool(crt),"actual native CRT owner");
    NativeRetailMainTiming timing;RetailMainTimingFailure f;
    check(!timing.start(3,*crt,f)&&f.kind==RetailMainTimingError::crt&&f.crt.kind==RetailCrtError::not_finalized&&!timing.observe(3),
        "unpublished CRT cannot create stopwatch state");
    check(crt->finalize_pre_main(cf),"real CRT publication");
    check(!timing.stop(3,*crt,f)&&f.kind==RetailMainTimingError::not_started,"unknown stopwatch fails by name");
    const auto before=crt->now_nanoseconds(cf);
    check(timing.start(3,*crt,f),"main slot3 samples same real CRT");
    const auto after=crt->now_nanoseconds(cf);auto observation=timing.observe(3);
    check(before&&after&&observation&&observation->running&&observation->elapsed_microseconds==0&&
        observation->started_microseconds>=*before/1000&&observation->started_microseconds<=*after/1000,
        "native microsecond scale and shared epoch");
    auto other=NativeRetailCrt::create(cf);check(other&&other->finalize_pre_main(cf),"different real clock owner");
    const auto coarse_before=crt->now_nanoseconds(cf);
    const auto coarse=timing.coarse_milliseconds(*crt,f);
    const auto coarse_after=crt->now_nanoseconds(cf);
    check(coarse_before&&coarse&&coarse_after&&*coarse>=*coarse_before/1000000u&&
          *coarse<=*coarse_after/1000000u,"native coarse milliseconds share actual steady-clock epoch/scale");
    const auto coarse_again=timing.coarse_milliseconds(*crt,f);
    check(coarse_again&&*coarse_again>=*coarse,"coarse clock is live and monotonic");
    check(!timing.coarse_milliseconds(*other,f)&&f.kind==RetailMainTimingError::foreign_clock,
          "coarse clock cannot silently change retained clock identity");
    check(!timing.stop(3,*other,f)&&f.kind==RetailMainTimingError::foreign_clock&&timing.observe(3)->running,
        "foreign CRT cannot stop or mutate the live stopwatch");
    const auto elapsed=timing.stop(3,*crt,f);observation=timing.observe(3);
    check(elapsed&&observation&&!observation->running&&observation->elapsed_microseconds==*elapsed,
        "native elapsed result retained without original32bit capacity");
    const auto elapsed_again=timing.stop(3,*crt,f);
    check(elapsed_again&&*elapsed_again>=*elapsed,"source repeated stop resamples the retained start");
    fail_allocation=true;const bool admitted=timing.start(200,*crt,f);fail_allocation=false;
    check(!admitted&&f.kind==RetailMainTimingError::allocation&&!timing.observe(200),"failed real map allocation publishes no slot");
    for(unsigned slot=100;slot<180;++slot)check(timing.start(slot,*crt,f),"dynamic stopwatch ownership");
    check(timing.observe(179).has_value(),"no original slot count is a host memory limit");
    check(timing.initialize_parsed_setting(f)&&timing.parsed_setting()==84,"verified original text binary32 conversion");
    bool foreign=false;std::thread worker([&]{
        RetailMainTimingFailure tf;foreign=!timing.start(3,*crt,tf)&&tf.kind==RetailMainTimingError::crt&&
            tf.crt.kind==RetailCrtError::wrong_thread&&!timing.observe(3)&&!timing.parsed_setting();
        foreign=foreign&&!timing.coarse_milliseconds(*crt,tf)&&tf.kind==RetailMainTimingError::crt&&
            tf.crt.kind==RetailCrtError::wrong_thread;
    });worker.join();check(foreign,"foreign thread cannot mutate or observe main timing");
    check(crt->shutdown(cf),"real CRT shutdown");
    const auto retained=timing.observe(3);
    check(!timing.stop(3,*crt,f)&&f.crt.kind==RetailCrtError::closed&&
        timing.observe(3)->elapsed_microseconds==retained->elapsed_microseconds,"closed-clock failure retains exact prior state");
    std::puts("PASS native main shared-clock stopwatches and source parsed setting; no game/device");
    return 0;
}catch(const std::exception& e){fail_allocation=false;std::fprintf(stderr,"FAIL native main timing: %s\n",e.what());return 1;}}
