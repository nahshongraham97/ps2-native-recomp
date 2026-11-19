#include "mka/movie_adx_blocks.hpp"
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {
bool fail(AdxBlockFailure& f,AdxBlockError e,const char* s) noexcept{f={e,s};return false;}
std::int64_t floor4096(std::int64_t n) noexcept{return n>=0?n/4096:-((-n+4095)/4096);}
}
std::optional<AdxBlockResult> decode_movie_adx_blocks(std::span<const std::uint8_t> bytes,
        const AdxBlockProfile& profile,AdxBlockFailure& f) noexcept {
    f={};
    if(profile.mode!=AdxBlockMode::version4_flags0){fail(f,AdxBlockError::unsupported_profile,"explicit version4 flags0 scale-state route");return {};}
    const auto count=profile.coefficients.size();
    if(!count||profile.histories.size()!=count){fail(f,AdxBlockError::invalid_profile,"explicit coefficient/history pair per plane");return {};}
    if(count>SIZE_MAX/18){fail(f,AdxBlockError::host_capacity,"encoded group extent");return {};}
    const auto group_bytes=count*18;
    if(bytes.empty()||!bytes.data()||bytes.size()%group_bytes){fail(f,AdxBlockError::invalid_extent,"complete nonempty encoded groups");return {};}
    const auto groups=bytes.size()/group_bytes;
    if(groups>SIZE_MAX/32){fail(f,AdxBlockError::host_capacity,"decoded frame extent");return {};}
    const auto frames=groups*32;
    if(frames>SIZE_MAX/sizeof(std::int16_t)||count>SIZE_MAX/(frames*sizeof(std::int16_t))){fail(f,AdxBlockError::host_capacity,"owned planar PCM byte extent");return {};}
    // End markers require a separate admission/trim owner, never partial PCM.
    for(std::size_t offset=0;offset<bytes.size();offset+=18)if(bytes[offset]&0x80){fail(f,AdxBlockError::marker,"encoded scale sign-bit marker requires explicit admission");return {};}
    try {
        AdxBlockResult result;result.frames=frames;result.final_histories=profile.histories;result.planes.resize(count);
        for(auto& plane:result.planes)plane.resize(frames);
        for(std::size_t group=0;group<groups;++group)for(std::size_t plane=0;plane<count;++plane){
            const auto* block=bytes.data()+group*group_bytes+plane*18;
            const std::int64_t scale=((std::uint32_t(block[0])<<8|block[1])&0x1fff)+1;
            auto& history=result.final_histories[plane];const auto [c1,c2]=profile.coefficients[plane];
            for(std::size_t i=0;i<32;++i){
                const auto packed=block[2+i/2];const int nibble=(i%2)?packed&15:packed>>4;const int signed_nibble=nibble<8?nibble:nibble-16;
                // Two signed16 products sum within [-2147418112,2147483648].
                // Native wide arithmetic avoids the extreme source32 wrap.
                const auto predictor=floor4096(std::int64_t(c1)*history.first+std::int64_t(c2)*history.second);
                const auto value=predictor+signed_nibble*scale;
                const auto sample=static_cast<std::int16_t>(value< -32768?-32768:value>32767?32767:value);
                history.second=history.first;history.first=sample;result.planes[plane][group*32+i]=sample;
            }
        }
        return result;
    }catch(const std::bad_alloc&){fail(f,AdxBlockError::allocation,"owned ADX block decode allocation");return {};}
    catch(const std::length_error&){fail(f,AdxBlockError::host_capacity,"owned ADX vector extent");return {};}
}
}
