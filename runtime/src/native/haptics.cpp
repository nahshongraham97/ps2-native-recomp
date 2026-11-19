#include "mka/haptics.hpp"

#include <limits>
#include <new>
#include <stdexcept>

namespace mka::input {
HapticDecode decode_actuators(const ActuatorBytes& alignment,const ActuatorBytes& values) {
    HapticDecode result;
    bool small=false,large=false;
    for(std::size_t i=0;i<alignment.size();++i){
        switch(alignment[i]){
        case 255: break; // explicitly unused serialized position
        case 0:
            if(small){result.error=HapticError::unsupported_alignment;return result;}
            if(values[i]>1){result.error=HapticError::unsupported_small_value;return result;}
            small=true;result.request.small=values[i]!=0;break;
        case 1:
            if(large){result.error=HapticError::unsupported_alignment;return result;}
            large=true;result.request.large=values[i];break;
        default:result.error=HapticError::unsupported_alignment;return result;
        }
    }
    return result;
}
ActDirectDecode decode_set_actdir_rpc(const std::uint8_t* bytes,std::size_t size) {
    ActDirectDecode result;
    if(!bytes || size<18){result.error=HapticError::invalid_request;return result;}
    const auto u32=[&](std::size_t at){return std::uint32_t(bytes[at])|(std::uint32_t(bytes[at+1])<<8)|
        (std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);};
    if(u32(0)!=7){result.error=HapticError::unsupported_command;return result;}
    result.request.player=u32(4);result.request.slot=u32(8);
    for(std::size_t i=0;i<6;++i)result.request.values[i]=bytes[12+i];
    return result;
}
HapticError Haptics::set_alignment(std::uint32_t player,const ActuatorBytes& alignment) {
    const auto decoded=decode_actuators(alignment,{});
    if(!decoded)return decoded.error;
    std::lock_guard<std::mutex> lock(mutex_);
    if(sequence_==std::numeric_limits<std::uint64_t>::max())return HapticError::sequence_exhausted;
    try{
        auto& entry=players_.try_emplace(player).first->second;
        entry=HapticSample{};entry.alignment=alignment;entry.sequence=++sequence_;
        return HapticError::none;
    }catch(const std::bad_alloc&){return HapticError::allocation_failure;}
     catch(const std::length_error&){return HapticError::allocation_failure;}
}
HapticError Haptics::submit(std::uint32_t player,const ActuatorBytes& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=players_.find(player);
    if(found==players_.end())return HapticError::missing_alignment;
    const auto decoded=decode_actuators(found->second.alignment,values);
    if(!decoded)return decoded.error;
    if(sequence_==std::numeric_limits<std::uint64_t>::max())return HapticError::sequence_exhausted;
    auto& entry=found->second;entry.values=values;entry.request=decoded.request;
    entry.sequence=++sequence_;entry.feedback=HapticFeedback::pending;return HapticError::none;
}
HapticError Haptics::stop(std::uint32_t player){return submit(player,{});}
HapticError Haptics::acknowledge(std::uint32_t player,std::uint64_t sequence,bool applied){
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=players_.find(player);
    if(found==players_.end() || found->second.sequence!=sequence)return HapticError::stale_feedback;
    found->second.feedback=applied?HapticFeedback::applied:HapticFeedback::failed;return HapticError::none;
}
std::optional<HapticSample> Haptics::sample(std::uint32_t player)const{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found=players_.find(player);if(found==players_.end())return std::nullopt;
    return found->second;
}
HapticError Haptics::stop_all(){
    std::lock_guard<std::mutex> lock(mutex_);
    if(players_.size()>std::numeric_limits<std::uint64_t>::max()-sequence_)return HapticError::sequence_exhausted;
    for(auto& entry:players_){entry.second.values={};entry.second.request={};
        entry.second.sequence=++sequence_;entry.second.feedback=HapticFeedback::pending;}
    return HapticError::none;
}
Haptics& haptics(){static Haptics service;return service;}
} // namespace mka::input
