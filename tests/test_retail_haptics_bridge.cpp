#include "mka/retail_haptics_bridge.hpp"
#include <cstdio>
#include <limits>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"retail haptics check %d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace {
std::array<std::uint8_t,32> packet(std::uint32_t sequence){
    std::array<std::uint8_t,32> p{};
    for(unsigned i=0;i<4;++i)p[i]=static_cast<std::uint8_t>(sequence>>(i*8));
    p[4]=1;p[8]=6;p[12]=1;p[13]=73;
    for(unsigned i=18;i<32;++i)p[i]=static_cast<std::uint8_t>(i+91);
    return p;
}
}
int main(){
    using namespace mka::input;
    Haptics service;
    const ActuatorBytes normal{{0,1,255,255,255,255}};
    CHECK(service.set_alignment(7,normal)==HapticError::none);
    CHECK(service.set_alignment(70000,normal)==HapticError::none);
    auto raw=packet(0xffffffffu);
    auto decoded=decode_retail_actuator_packet(raw.data(),raw.size());
    CHECK(decoded&&decoded.packet.sequence==0xffffffffu&&decoded.packet.task==1&&decoded.packet.length==6);
    CHECK(decoded.packet.values[0]==1&&decoded.packet.values[1]==73&&decoded.packet.raw==raw);
    CHECK(decode_retail_actuator_packet(nullptr,32).error==RetailHapticError::malformed_packet);
    CHECK(decode_retail_actuator_packet(raw.data(),31).error==RetailHapticError::malformed_packet);
    CHECK(decode_retail_actuator_packet(raw.data(),33).error==RetailHapticError::malformed_packet);
    raw[4]=7;CHECK(decode_retail_actuator_packet(raw.data(),32).error==RetailHapticError::unsupported_task);
    raw=packet(1);raw[8]=5;CHECK(decode_retail_actuator_packet(raw.data(),32).error==RetailHapticError::unsupported_length);
    raw=packet(1);
    HapticEndpoint retired;
    {
        RetailHapticsBridge bridge(service);
        CHECK(bridge.bind(1,999).error==RetailHapticError::missing_alignment);
        const auto a=bridge.bind(0x1234567800000001ull,7);CHECK(a);retired=a.endpoint;
        CHECK(bridge.bind(a.endpoint.identity,70000).error==RetailHapticError::endpoint_already_bound);
        CHECK(bridge.bind(2,7).error==RetailHapticError::player_already_bound);
        const auto b=bridge.bind(0xabcdef0000000001ull,70000);CHECK(b); // same low32, different full identity
        CHECK(!bridge.sample(a.endpoint));
        CHECK(bridge.deliver(a.endpoint,raw.data(),32));
        CHECK(service.sample(7)->request.small&&service.sample(7)->request.large==73);
        CHECK(!service.sample(70000)->request.large);
        const auto retained=bridge.sample(a.endpoint);CHECK(retained&&retained->player==7&&retained->packet.raw==raw);
        raw[13]=129;CHECK(bridge.deliver(b.endpoint,raw.data(),32));
        CHECK(service.sample(70000)->request.large==129&&retained->packet.values[1]==73);
        const auto before=service.sample(7);
        raw[4]=9;CHECK(bridge.deliver(a.endpoint,raw.data(),32).error==RetailHapticError::unsupported_task);
        CHECK(service.sample(7)->sequence==before->sequence&&bridge.sample(a.endpoint)->packet.values[1]==73);
        raw=packet(2);raw[12]=2;
        const auto invalid=bridge.deliver(a.endpoint,raw.data(),32);
        CHECK(invalid.error==RetailHapticError::publication_failed&&invalid.publication_error==HapticError::unsupported_small_value);
        CHECK(service.sample(7)->sequence==before->sequence);
        // Original counter increments before submission; gaps and uint32 wrap
        // are data, never an inferred failure or ordering/deduplication rule.
        for(auto sequence:{100u,std::numeric_limits<std::uint32_t>::max(),0u,0u}){
            raw=packet(sequence);CHECK(bridge.deliver(a.endpoint,raw.data(),32));
            CHECK(bridge.sample(a.endpoint)->packet.sequence==sequence);
        }
        CHECK(service.sample(7)->feedback==HapticFeedback::pending); // never fabricated host application
        CHECK(bridge.close(a.endpoint));
        CHECK(!service.sample(7)->request.small&&!service.sample(7)->request.large);
        CHECK(bridge.deliver(a.endpoint,raw.data(),32).error==RetailHapticError::unbound_or_stale_endpoint);
        const auto reopened=bridge.bind(a.endpoint.identity,7);CHECK(reopened&&reopened.endpoint.generation!=a.endpoint.generation);
        CHECK(bridge.deliver(a.endpoint,raw.data(),32).error==RetailHapticError::unbound_or_stale_endpoint);
        CHECK(bridge.deliver(reopened.endpoint,raw.data(),32));
    }
    CHECK(!service.sample(7)->request.small&&!service.sample(7)->request.large&&!service.sample(70000)->request.large);
    {
        RetailHapticsBridge next(service);const auto current=next.bind(retired.identity,7);CHECK(current);
        CHECK(current.endpoint.generation!=retired.generation);
        CHECK(next.deliver(retired,raw.data(),32).error==RetailHapticError::unbound_or_stale_endpoint);
    }
    std::puts("retail32-byte haptic data, full endpoint identity, publication, reopen lifetime and stop PASS");
}
