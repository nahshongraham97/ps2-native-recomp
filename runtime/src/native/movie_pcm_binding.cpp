#include "mka/movie_pcm_binding.hpp"
#include <bit>
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {
bool fail(MoviePcmBindingFailure& f,MoviePcmBindingError e,const char* op) noexcept{f.kind=e;f.operation=op;return false;}
bool layout_valid(const MovieAudioChannelLayout& l) noexcept {
    if(!l.channel_count)return false;
    switch(l.order){
    case MovieAudioChannelOrder::unspecified:return !l.mask&&l.channels.empty();
    case MovieAudioChannelOrder::native:return l.mask&&std::popcount(*l.mask)==l.channel_count&&l.channels.size()==l.channel_count;
    case MovieAudioChannelOrder::custom:return !l.mask&&l.channels.size()==l.channel_count;
    case MovieAudioChannelOrder::ambisonic:return l.channels.size()==l.channel_count;
    }return false;
}
}
std::unique_ptr<NativeMoviePcmBinding> NativeMoviePcmBinding::create(MoviePcmBindingConfiguration c,MoviePcmBindingFailure& f) noexcept {
    f={};
    if(c.mode!=MoviePcmMixMode::generic_stateless){fail(f,MoviePcmBindingError::unsupported_profile,"explicit generic stateless PCM binding");return {};}
    if(c.source_key.empty()||!layout_valid(c.source_layout)||!c.source_sample_rate||!c.client_format.sample_rate||!c.client_format.channels||!c.client_format.channel_mask||std::popcount(c.client_format.channel_mask)!=c.client_format.channels||c.routes.size()!=c.source_layout.channel_count){fail(f,MoviePcmBindingError::invalid_configuration,"explicit key/layout/rates/client mask/routes");return {};}
    if(c.source_sample_rate!=c.client_format.sample_rate){fail(f,MoviePcmBindingError::source_rate_mismatch,"equal source/client sample rate");return {};}
    for(const auto& route:c.routes)if(route.output0==route.output1||route.output0>=c.client_format.channels||route.output1>=c.client_format.channels){fail(f,MoviePcmBindingError::invalid_configuration,"distinct explicit output pair in client extent");return {};}
    try{return std::unique_ptr<NativeMoviePcmBinding>(new NativeMoviePcmBinding(std::move(c)));}
    catch(const std::bad_alloc&){fail(f,MoviePcmBindingError::allocation,"owned immutable PCM binding");return {};}
    catch(const std::length_error&){fail(f,MoviePcmBindingError::host_capacity,"PCM binding host extent");return {};}
}
bool NativeMoviePcmBinding::prepare_audio(const MovieAudioFrame& a,std::int32_t requested,std::vector<float>& output,MoviePcmBindingFailure& f) const noexcept {
    f={};
    if(a.sample_rate!=config_.source_sample_rate)return fail(f,MoviePcmBindingError::source_rate_mismatch,"bound source rate");
    if(a.channel_layout!=config_.source_layout||a.channels.size()!=config_.routes.size())return fail(f,MoviePcmBindingError::source_layout_mismatch,"complete bound layout/plane identity");
    if(a.channels.empty()||a.channels.front().empty())return fail(f,MoviePcmBindingError::invalid_frame,"nonempty original PCM frame");
    for(const auto& p:a.channels)if(p.size()!=a.channels.front().size())return fail(f,MoviePcmBindingError::invalid_frame,"equal original plane frame counts");
    try {
        MoviePcmMixProfile matrix{MoviePcmMixMode::generic_stateless,config_.client_format.channels,{}};
        matrix.coefficients.resize(config_.routes.size());
        std::vector<std::span<const std::int16_t>> planes;planes.reserve(a.channels.size());
        for(std::size_t i=0;i<config_.routes.size();++i){
            const auto& route=config_.routes[i];
            const auto gain=movie_pcm_gain(requested,route.metadata_tenth_db);
            auto pair=movie_pcm_pan(gain.table_value,route.explicit_pan,f.pcm);
            if(!pair)return fail(f,MoviePcmBindingError::pcm,"bound gain/pan coefficients");
            auto& row=matrix.coefficients[i];row.assign(config_.client_format.channels,0);
            row[route.output0]=(*pair)[0];row[route.output1]=(*pair)[1];planes.emplace_back(a.channels[i]);
        }
        auto mixed=mix_movie_pcm(matrix,planes,f.pcm);
        if(!mixed)return fail(f,MoviePcmBindingError::pcm,"bound original PCM mix");
        auto staged=movie_pcm_float_interleaved(*mixed,f.pcm);
        if(!staged)return fail(f,MoviePcmBindingError::pcm,"bound final float representation");
        output.swap(*staged);return true;
    }catch(const std::bad_alloc&){return fail(f,MoviePcmBindingError::allocation,"staged PCM binding allocation");}
    catch(const std::length_error&){return fail(f,MoviePcmBindingError::host_capacity,"staged PCM binding extent");}
}
}
