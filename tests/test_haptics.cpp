#include "mka/haptics.hpp"
#include <atomic>
#include <cstdio>
#include <thread>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"haptics check %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace mka::input;
    const ActuatorBytes normal{{0,1,255,255,255,255}},reverse{{1,0,255,255,255,255}};
    const ActuatorBytes on{{1,73,111,222,33,44}};
    auto d=decode_actuators(normal,on);CHECK(d&&d.request.small&&d.request.large==73);
    d=decode_actuators(reverse,{{129,1,0,0,0,0}});CHECK(d&&d.request.small&&d.request.large==129);
    CHECK(decode_actuators(normal,{{2,0,0,0,0,0}}).error==HapticError::unsupported_small_value);
    CHECK(decode_actuators({{0,0,255,255,255,255}},{}).error==HapticError::unsupported_alignment);
    CHECK(decode_actuators({{2,1,255,255,255,255}},{}).error==HapticError::unsupported_alignment);
    CHECK(decode_actuators({{255,255,255,255,255,255}},on));
    std::array<std::uint8_t,24> wire{{7,0,0,0,0x78,0x56,0x34,0x12,9,0,0,0,1,73,111,222,33,44}};
    auto rpc=decode_set_actdir_rpc(wire.data(),wire.size());
    CHECK(rpc&&rpc.request.player==0x12345678&&rpc.request.slot==9&&rpc.request.values==on);
    CHECK(decode_set_actdir_rpc(wire.data(),17).error==HapticError::invalid_request);
    CHECK(decode_set_actdir_rpc(nullptr,24).error==HapticError::invalid_request);
    wire[0]=8;CHECK(decode_set_actdir_rpc(wire.data(),24).error==HapticError::unsupported_command);
    Haptics service;
    CHECK(!service.sample(100000));CHECK(service.submit(100000,on)==HapticError::missing_alignment);
    CHECK(service.set_alignment(100000,normal)==HapticError::none);
    CHECK(service.submit(100000,on)==HapticError::none);
    const auto retained=service.sample(100000);CHECK(retained&&retained->request.small&&retained->request.large==73);
    CHECK(service.acknowledge(100000,retained->sequence,true)==HapticError::none);
    CHECK(service.sample(100000)->feedback==HapticFeedback::applied);
    CHECK(service.stop(100000)==HapticError::none);
    const auto stopped=service.sample(100000);CHECK(stopped&&stopped->sequence>retained->sequence&&!stopped->request.small&&!stopped->request.large);
    CHECK(service.acknowledge(100000,retained->sequence,true)==HapticError::stale_feedback);
    CHECK(service.acknowledge(100000,stopped->sequence,false)==HapticError::none);
    CHECK(service.sample(100000)->feedback==HapticFeedback::failed);
    CHECK(retained->request.large==73&&retained->feedback==HapticFeedback::pending); // owned copy
    CHECK(service.set_alignment(100000,reverse)==HapticError::none);
    CHECK(service.sample(100000)->alignment==reverse&&!service.sample(100000)->request.large);
    const auto before=service.sample(100000);
    CHECK(service.set_alignment(100000,{{0,0,255,255,255,255}})==HapticError::unsupported_alignment);
    CHECK(service.sample(100000)->sequence==before->sequence);
    for(unsigned player=0;player<128;++player)CHECK(service.set_alignment(player,normal)==HapticError::none);
    CHECK(service.stop_all()==HapticError::none);
    std::atomic<bool> done{false},valid{true};
    std::thread writer([&]{for(unsigned i=0;i<2000;++i){if(service.submit(0,(i&1)?ActuatorBytes{{1,73,0,0,0,0}}:ActuatorBytes{})!=HapticError::none)valid=false;}done=true;});
    while(!done){const auto s=service.sample(0);if(!s || (s->request.small?(s->request.large!=73):(s->request.large!=0)))valid=false;}
    writer.join();CHECK(valid);
    std::puts("native haptic decoding, ownership, sequence feedback, reset and atomic samples PASS");
}
