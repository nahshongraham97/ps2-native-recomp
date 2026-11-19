#include "mka/movie_pcm_mix.hpp"
#include <bit>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {
constexpr std::uint16_t gain_table[]={
#define MKA_MOVIE_PCM_GAIN(v) v,
#define MKA_MOVIE_PCM_PAN(v)
#include "generated/movie_pcm_mix_tables.inc"
#undef MKA_MOVIE_PCM_GAIN
#undef MKA_MOVIE_PCM_PAN
};
constexpr std::uint16_t pan_table[]={
#define MKA_MOVIE_PCM_GAIN(v)
#define MKA_MOVIE_PCM_PAN(v) v,
#include "generated/movie_pcm_mix_tables.inc"
#undef MKA_MOVIE_PCM_GAIN
#undef MKA_MOVIE_PCM_PAN
};
constexpr bool tables_valid(){for(auto x:gain_table)if(x>256)return false;for(auto x:pan_table)if(x>255)return false;return true;}
static_assert(std::size(gain_table)==1000&&std::size(pan_table)==31&&tables_valid());
static_assert(gain_table[0]==256&&gain_table[24]==194&&gain_table[960]==0);
static_assert(pan_table[0]==255&&pan_table[15]==180&&pan_table[30]==0);
bool fail(MoviePcmMixFailure& f,MoviePcmMixError k,const char* op) noexcept{f={k,op};return false;}
std::int64_t contribution(std::int16_t sample,std::uint16_t coefficient) noexcept{
    if(coefficient==255)return sample;
    const std::int64_t product=std::int64_t(sample)*coefficient;
    return product>=0?product/256:-((-product+255)/256);
}
float normalized(std::int16_t sample) noexcept{
    // All admitted magnitudes<=32767: construct exact native binary32 bits.
    if(!sample)return 0.0f;
    const bool neg=sample<0;const auto mag=static_cast<std::uint32_t>(neg?-std::int32_t(sample):sample);
    const int leading=31-std::countl_zero(mag);
    const auto mantissa=(mag<<(23-leading))&0x7fffffu;
    return std::bit_cast<float>((neg?0x80000000u:0u)|std::uint32_t(leading-15+127)<<23|mantissa);
}
static_assert(sizeof(std::size_t)<=8);
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
}
const char* movie_pcm_mix_error_name(MoviePcmMixError e) noexcept{
    switch(e){case MoviePcmMixError::none:return "none";case MoviePcmMixError::unsupported_profile:return "unsupported_profile";case MoviePcmMixError::invalid_matrix:return "invalid_matrix";case MoviePcmMixError::invalid_planes:return "invalid_planes";case MoviePcmMixError::invalid_gain:return "invalid_gain";case MoviePcmMixError::host_capacity:return "host_capacity";case MoviePcmMixError::allocation:return "allocation";}return "unknown_error";
}
MoviePcmGain movie_pcm_gain(std::int64_t requested,std::int64_t metadata) noexcept{
    const __int128 sum=static_cast<__int128>(requested)+metadata;
    const auto db=static_cast<std::int32_t>(sum>0?0:sum< -999?-999:sum);
    return {db,gain_table[-db]};
}
std::optional<std::array<std::uint16_t,2>> movie_pcm_pan(std::uint16_t gain,std::int64_t pan,MoviePcmMixFailure& f) noexcept{
    f={};if(gain>256){fail(f,MoviePcmMixError::invalid_gain,"literal gain table value0..256 required");return {};}
    const int p=pan< -15?-15:pan>15?15:static_cast<int>(pan);
    const auto coefficient=[&](std::size_t i){const auto n=(2u*gain*pan_table[i]+255u)/510u;return static_cast<std::uint16_t>(n>255?255:n);};
    return std::array<std::uint16_t,2>{coefficient(p+15),coefficient(15-p)};
}
std::optional<MoviePcmMixResult> mix_movie_pcm(const MoviePcmMixProfile& profile,std::span<const std::span<const std::int16_t>> planes,MoviePcmMixFailure& f) noexcept{
    f={};if(profile.mode!=MoviePcmMixMode::generic_stateless){fail(f,MoviePcmMixError::unsupported_profile,"explicit generic stateless profile required");return {};}
    if(!profile.output_count||profile.coefficients.size()!=planes.size()){fail(f,MoviePcmMixError::invalid_matrix,"explicit plane/output matrix extent");return {};}
    if(planes.empty()||!planes.data()||planes.front().empty()){fail(f,MoviePcmMixError::invalid_planes,"nonempty original source planes required");return {};}
    const auto frames=planes.front().size();
    for(std::size_t p=0;p<planes.size();++p){
        if(!planes[p].data()||planes[p].size()!=frames){fail(f,MoviePcmMixError::invalid_planes,"equal source frame extents");return {};}
        if(profile.coefficients[p].size()!=profile.output_count){fail(f,MoviePcmMixError::invalid_matrix,"complete coefficient row");return {};}
        for(auto c:profile.coefficients[p])if(c>255){fail(f,MoviePcmMixError::invalid_matrix,"coefficient outside0..255");return {};}
    }
    if(frames>SIZE_MAX/sizeof(std::int16_t)||profile.output_count>SIZE_MAX/(frames*sizeof(std::int16_t))){fail(f,MoviePcmMixError::host_capacity,"owned output byte extent overflow");return {};}
    try{
        MoviePcmMixResult result;result.frames=frames;result.outputs.resize(profile.output_count);
        for(auto& output:result.outputs)output.resize(frames);
        for(std::size_t o=0;o<profile.output_count;++o)for(std::size_t i=0;i<frames;++i){
            // At most SIZE_MAX int16 planes: exact sum bound<2^80 on64bit host.
            __int128 total=0;for(std::size_t p=0;p<planes.size();++p)total+=contribution(planes[p][i],profile.coefficients[p][o]);
            result.outputs[o][i]=static_cast<std::int16_t>(total>32767?32767:total< -32767?-32767:total);
        }
        return result;
    }catch(const std::bad_alloc&){fail(f,MoviePcmMixError::allocation,"owned PCM allocation");return {};}
    catch(const std::length_error&){fail(f,MoviePcmMixError::host_capacity,"owned PCM vector extent");return {};}
}
std::optional<std::vector<float>> movie_pcm_float_interleaved(const MoviePcmMixResult& input,MoviePcmMixFailure& f) noexcept{
    f={};if(!input.frames||input.outputs.empty()){fail(f,MoviePcmMixError::invalid_planes,"complete clipped planar result required");return {};}
    for(const auto& p:input.outputs){if(p.size()!=input.frames){fail(f,MoviePcmMixError::invalid_planes,"clipped result plane extent");return {};}for(auto s:p)if(s< -32767){fail(f,MoviePcmMixError::invalid_planes,"final clipped integer domain");return {};}}
    if(input.frames>SIZE_MAX/sizeof(float)||input.outputs.size()>SIZE_MAX/(input.frames*sizeof(float))){fail(f,MoviePcmMixError::host_capacity,"interleaved float byte extent overflow");return {};}
    try{std::vector<float> out(input.frames*input.outputs.size());for(std::size_t i=0;i<input.frames;++i)for(std::size_t o=0;o<input.outputs.size();++o)out[i*input.outputs.size()+o]=normalized(input.outputs[o][i]);return out;}
    catch(const std::bad_alloc&){fail(f,MoviePcmMixError::allocation,"owned float PCM allocation");return {};}
    catch(const std::length_error&){fail(f,MoviePcmMixError::host_capacity,"float PCM vector extent");return {};}
}
} // namespace mka::video
