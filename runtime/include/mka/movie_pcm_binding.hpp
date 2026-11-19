#pragma once
#include "mka/movie_decode.hpp"
#include "mka/movie_pcm_mix.hpp"
#include "mka/audio_device.hpp"
#include <memory>
#include <utility>
namespace mka::video {
struct MoviePcmPlaneRoute {
    std::int64_t metadata_tenth_db,explicit_pan;
    std::size_t output0,output1;
    MoviePcmPlaneRoute(std::int64_t gain,std::int64_t pan,std::size_t a,std::size_t b)
        :metadata_tenth_db(gain),explicit_pan(pan),output0(a),output1(b){}
};
struct MoviePcmBindingConfiguration {
    std::string source_key;
    MovieAudioChannelLayout source_layout;
    std::uint32_t source_sample_rate=0;
    sound::AudioStreamFormat client_format;
    MoviePcmMixMode mode=MoviePcmMixMode::unspecified;
    std::vector<MoviePcmPlaneRoute> routes;
};
enum class MoviePcmBindingError { none,invalid_owner,invalid_configuration,unsupported_profile,source_layout_mismatch,source_rate_mismatch,invalid_frame,allocation,host_capacity,pcm };
struct MoviePcmBindingFailure {
    MoviePcmBindingError kind=MoviePcmBindingError::none;
    const char* operation="";
    MoviePcmMixFailure pcm;
};
class NativeMoviePcmBinding {
public:
    static std::unique_ptr<NativeMoviePcmBinding> create(MoviePcmBindingConfiguration,MoviePcmBindingFailure&) noexcept;
    const std::string& source_key() const noexcept{return config_.source_key;}
    const sound::AudioStreamFormat& client_format() const noexcept{return config_.client_format;}
    bool prepare_audio(const MovieAudioFrame&,std::int32_t requested_tenth_db,std::vector<float>&,MoviePcmBindingFailure&) const noexcept;
    NativeMoviePcmBinding(const NativeMoviePcmBinding&)=delete;
    NativeMoviePcmBinding& operator=(const NativeMoviePcmBinding&)=delete;
private:
    explicit NativeMoviePcmBinding(MoviePcmBindingConfiguration c):config_(std::move(c)){}
    const MoviePcmBindingConfiguration config_;
};
}
