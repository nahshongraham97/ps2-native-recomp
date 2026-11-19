#include "mka/gpu_diagnostics.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace {
void check(bool ok,const char* message) { if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);} }
}
int main() {
    using namespace mka::graphics;
    GpuQueryCounter counter;
    const auto first=counter.session();
    check(counter.total_ms()==0,"unavailable/uncompleted timing is not fabricated");
    check(counter.completed(first,1,4000000) && counter.total_ms()==4,"first completed duration");
    check(counter.completed(first,2,6000000) && counter.total_ms()==10,"unequal frame durations accumulate rather than replace");
    check(!counter.completed(first,2,6000000) && !counter.completed(first,1,4000000) && counter.total_ms()==10,"repeat/out-of-order results never double count");
    const double window_begin=counter.total_ms();
    check(counter.completed(first,3,4000000) && counter.completed(first,4,4000000),"equal later durations completed");
    check(counter.total_ms()-window_begin==8,"equal frame durations yield positive window delta");
    counter.reset_session();
    check(counter.total_ms()==18 && !counter.completed(first,5,9000000),"context reset preserves total and discards stale query identity");
    check(counter.completed(counter.session(),1,2000000) && counter.total_ms()==20,"new context starts query serials but adds to process total");
    check(!counter.completed(counter.session(),0,1),"unassigned query serial rejected");
    // Pending work is represented by absence of completed(), not a guessed time.
    const auto observed=counter.total_ms();
    check(observed==counter.total_ms(),"reads are observational and cannot consume/count GL queries");
    unsigned reached=0,stopped=0,uploads=0;
    for(bool result:{false,true,false}) {
        std::vector<int> sequence;
        const bool actual=account_native_transfer([&]{sequence.push_back(1);++uploads;return result;},[&](bool success){sequence.push_back(2); if(success)++reached;else ++stopped;});
        check(actual==result && sequence==std::vector<int>({1,2}),"accounting occurs once after actual destination result");
    }
    check(uploads==3 && reached==1 && stopped==2,"failed destination operations never inflate reached count");
    std::puts("GPU diagnostics helper tests passed; no GL driver executed");
}
