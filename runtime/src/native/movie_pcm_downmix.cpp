#include "mka/movie_pcm_downmix.hpp"
#include <new>
#include <stdexcept>
namespace mka::video {
std::optional<MoviePcmMixResult> movie_pcm_downmix_duplicate(
        std::span<const std::int16_t> first,std::span<const std::int16_t> second,
        MoviePcmMixFailure& f) noexcept {
    f={};
    if(first.empty()||!first.data()||!second.data()||first.size()!=second.size()){
        f={MoviePcmMixError::invalid_planes,"nonempty equal original downmix planes"};return {};
    }
    if(first.size()>SIZE_MAX/(2*sizeof(std::int16_t))){
        f={MoviePcmMixError::host_capacity,"owned duplicate PCM byte extent"};return {};
    }
    try {
        MoviePcmMixResult result;result.frames=first.size();result.outputs.resize(2);
        for(auto& p:result.outputs)p.resize(result.frames);
        for(std::size_t i=0;i<result.frames;++i){
            const std::int32_t sum=std::int32_t(first[i])+second[i];
            const std::int32_t q=(7*sum)/10; // bounded signed32; C++ truncates toward zero.
            const auto sample=static_cast<std::int16_t>(q< -32768?-32768:q>32767?32767:q);
            result.outputs[0][i]=sample;result.outputs[1][i]=sample;
        }
        return result;
    }catch(const std::bad_alloc&){f={MoviePcmMixError::allocation,"owned downmix PCM allocation"};return {};}
    catch(const std::length_error&){f={MoviePcmMixError::host_capacity,"owned downmix vector extent"};return {};}
}
}
