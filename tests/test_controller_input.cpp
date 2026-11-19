#include "mka/controller_input.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define CHECK(x) do { if(!(x)) {std::fprintf(stderr,"input check failed at %d: %s\n",__LINE__,#x);std::abort();} } while(0)
int main() {
    using namespace mka::input;
    ControllerInput input;
    CHECK(!input.sample(0));
    auto a=digital_state(0x4300,true,{{1,2,3,4}},73,129);
    CHECK(a.pressure[8]==73&&a.pressure[9]==129&&a.pressure[14]==255&&a.pressure[15]==0);
    for(unsigned i=0;i<a.pressure.size();++i)a.pressure[i]=static_cast<std::uint8_t>(i*17);
    CHECK(decode_pressure(encode_pressure(a.pressure))==a.pressure);
    CHECK(!decode_pressure("00")&&!decode_pressure("gg000000000000000000000000000000"));
    CHECK(input.publish(0,a));
    auto retained=input.sample(0);CHECK(retained&&retained->state.axes==a.axes);
    CHECK(input.publish(0,digital_state(0,false)));
    auto released=input.sample(0);CHECK(released&&!released->state.connected&&released->state.buttons==0);
    CHECK(released->sequence>retained->sequence&&retained->state.pressure==a.pressure);
    for(std::uint32_t player=1;player<=1025;++player)CHECK(input.publish(player,a));
    CHECK(input.sample(1025)&&!input.sample(1026));
    auto b=digital_state(0x8000,true,{{251,252,253,254}},0,0);
    std::atomic<bool> done{false};
    std::thread writer([&]{for(unsigned i=0;i<50000;++i)CHECK(input.publish(0,(i&1)?a:b));done.store(true);});
    while(!done.load()) {
        auto s=input.sample(0);CHECK(s);
        if(s->state.buttons==a.buttons)CHECK(s->state.axes==a.axes&&s->state.pressure==a.pressure);
        else if(s->state.buttons==b.buttons)CHECK(s->state.axes==b.axes&&s->state.pressure==b.pressure);
        else CHECK(s->state.buttons==0&&!s->state.connected);
    }
    writer.join();
    CHECK(input.sample(1)->state.axes==a.axes);
    std::puts("native controller ownership, pressure, release, capacity and atomic publication PASS");
}
