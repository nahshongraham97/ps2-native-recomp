#include "mka/retail_haptics_bridge.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::input {
namespace {
std::atomic<std::uint64_t> generations{0};
std::optional<std::uint64_t> next_generation(){
    auto previous=generations.load(std::memory_order_relaxed);
    do {if(previous==std::numeric_limits<std::uint64_t>::max())return std::nullopt;}
    while(!generations.compare_exchange_weak(previous,previous+1,std::memory_order_relaxed));
    return previous+1;
}
}
RetailActuatorDecode decode_retail_actuator_packet(const std::uint8_t* data,std::size_t size){
    RetailActuatorDecode result;
    if(!data || size!=32){result.error=RetailHapticError::malformed_packet;return result;}
    const auto u32=[&](unsigned at){return std::uint32_t(data[at])|(std::uint32_t(data[at+1])<<8)|
        (std::uint32_t(data[at+2])<<16)|(std::uint32_t(data[at+3])<<24);};
    auto& packet=result.packet;packet.sequence=u32(0);packet.task=u32(4);packet.length=u32(8);
    for(unsigned i=0;i<32;++i)packet.raw[i]=data[i];
    for(unsigned i=0;i<6;++i)packet.values[i]=data[12+i];
    if(packet.task!=1)result.error=RetailHapticError::unsupported_task;
    else if(packet.length!=6)result.error=RetailHapticError::unsupported_length;
    return result;
}
RetailHapticsBridge::~RetailHapticsBridge(){
    if(!close_all()){
        std::fprintf(stderr,"[native-haptics-stop] cannot publish stop while retiring endpoint bindings\n");
        std::abort();
    }
}
HapticBindingResult RetailHapticsBridge::bind(std::uint64_t identity,std::uint32_t player){
    std::lock_guard<std::mutex> lock(mutex_);
    if(endpoints_.find(identity)!=endpoints_.end())return {{},RetailHapticError::endpoint_already_bound};
    for(const auto& entry:endpoints_)if(entry.second.player==player)return {{},RetailHapticError::player_already_bound};
    if(!service_.sample(player))return {{},RetailHapticError::missing_alignment};
    const auto generation=next_generation();if(!generation)return {{},RetailHapticError::generation_exhausted};
    try{endpoints_.emplace(identity,EndpointState{*generation,player,{}});return {{identity,*generation},RetailHapticError::none};}
    catch(const std::bad_alloc&){return {{},RetailHapticError::allocation_failure};}
    catch(const std::length_error&){return {{},RetailHapticError::allocation_failure};}
}
RetailHapticDelivery RetailHapticsBridge::deliver(HapticEndpoint endpoint,const std::uint8_t* data,std::size_t size){
    const auto decoded=decode_retail_actuator_packet(data,size);if(!decoded)return {decoded.error};
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=endpoints_.find(endpoint.identity);
    if(found==endpoints_.end() || found->second.generation!=endpoint.generation)return {RetailHapticError::unbound_or_stale_endpoint};
    const auto error=service_.submit(found->second.player,decoded.packet.values);
    if(error!=HapticError::none)return {RetailHapticError::publication_failed,error};
    found->second.last=decoded.packet;return {};
}
RetailHapticDelivery RetailHapticsBridge::close(HapticEndpoint endpoint){
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=endpoints_.find(endpoint.identity);
    if(found==endpoints_.end() || found->second.generation!=endpoint.generation)return {RetailHapticError::unbound_or_stale_endpoint};
    const auto error=service_.stop(found->second.player);
    if(error!=HapticError::none)return {RetailHapticError::publication_failed,error};
    endpoints_.erase(found);return {};
}
RetailHapticDelivery RetailHapticsBridge::close_all(){
    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& entry:endpoints_){const auto error=service_.stop(entry.second.player);
        if(error!=HapticError::none)return {RetailHapticError::publication_failed,error};}
    endpoints_.clear();return {};
}
std::optional<RetailHapticSnapshot> RetailHapticsBridge::sample(HapticEndpoint endpoint)const{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=endpoints_.find(endpoint.identity);
    if(found==endpoints_.end() || found->second.generation!=endpoint.generation || !found->second.last)return std::nullopt;
    return RetailHapticSnapshot{found->second.player,*found->second.last};
}
RetailHapticsBridge& retail_haptics_bridge(){static RetailHapticsBridge bridge(haptics());return bridge;}
} // namespace mka::input
