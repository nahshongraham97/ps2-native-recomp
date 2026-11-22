#include "mka/sound_output.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#endif
namespace mka::sound {
namespace {
bool failed(AudioDeviceFailure& f,AudioDeviceError kind,const char* operation,std::int32_t code=0) noexcept {
    f={kind,code,operation};return false;
}
}
std::optional<AudioDeviceConfig> plan_game_sound_output(const AudioEndpointInventory& inventory,AudioDeviceFailure& f){
    f={};
    if(inventory.requested_role!=AudioEndpointRole::console){failed(f,AudioDeviceError::invalid_role,"Windows game-role audio endpoint required");return {};}
    if(inventory.default_failure.kind!=AudioDeviceError::none){f=inventory.default_failure;return {};}
    const AudioEndpointInfo* selected=nullptr;
    for(const auto& endpoint:inventory.active_render_endpoints){
        if(endpoint.endpoint_id_utf8!=inventory.default_endpoint_id_utf8)continue;
        if(selected||!endpoint.default_for_role){failed(f,AudioDeviceError::invalid_endpoint,"inconsistent game-role endpoint inventory");return {};}
        selected=&endpoint;
    }
    if(!selected||inventory.default_endpoint_id_utf8.empty()){
        failed(f,AudioDeviceError::endpoint_unavailable,"Windows game-role endpoint is absent");return {};
    }
    if(selected->mix_failure.kind!=AudioDeviceError::none){f=selected->mix_failure;return {};}
    if(selected->period_failure.kind!=AudioDeviceError::none){f=selected->period_failure;return {};}
    const auto& mix=selected->mix;
    auto bits=mix.channel_mask;unsigned channels=0;while(bits){bits&=bits-1;++channels;}
    if(!mix.sample_rate||!mix.channels||channels!=mix.channels||!mix.channel_mask||(mix.channel_mask&~0x3ffffu)||
       !mix.container_bits||mix.container_bits%8||!mix.valid_bits||mix.valid_bits>mix.container_bits||
       (mix.encoding!=AudioMixEncoding::ieee_float&&mix.encoding!=AudioMixEncoding::pcm)){
        failed(f,AudioDeviceError::format_unsupported,"invalid queried endpoint mix format");return {};
    }
    if(!selected->default_period_100ns){failed(f,AudioDeviceError::invalid_duration,"queried shared engine period is absent");return {};}
    // Two processing intervals provide a native buffering margin. Both the
    // actual engine period and the proved worker interval participate; no
    // source config word is reinterpreted as an output rate or buffer size.
    const auto period=std::max(selected->default_period_100ns,retail_sound_worker_microseconds*10);
    if(period>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/2){
        failed(f,AudioDeviceError::host_capacity,"native audio buffer duration representation");return {};
    }
    try {
        AudioDeviceConfig config{selected->endpoint_id_utf8,mix.sample_rate,2,3,period*2,AudioConversion::engine_conversion};
        if(!validate_audio_device_config(config,f))return {};
        return config;
    }catch(const std::bad_alloc&){failed(f,AudioDeviceError::allocation,"native game output plan allocation");}
     catch(const std::length_error&){failed(f,AudioDeviceError::host_capacity,"native game output plan capacity");}
    return {};
}
namespace {
class WasapiSoundOutput final:public NativeSoundOutput {
public:
    ~WasapiSoundOutput() override {
        AudioDeviceFailure ignored;close(ignored);
        device_.reset();
#ifdef _WIN32
        if(com_owned_)CoUninitialize();
#endif
    }
    bool initialize(AudioDeviceFailure& f){
#ifdef _WIN32
        const HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(FAILED(hr))return failed(f,AudioDeviceError::com_not_initialized,"CoInitializeEx sound worker STA",hr);
        com_owned_=true;
        auto inventory=inspect_audio_endpoints(AudioEndpointRole::console,f);if(!inventory)return false;
        auto config=plan_game_sound_output(*inventory,f);if(!config)return false;
        device_=NativeAudioDevice::open(*config,f);if(!device_)return false;
        auto actual=device_->stream_format(f);if(!actual)return false;
        format_=*actual;return true;
#else
        return failed(f,AudioDeviceError::unsupported_host,"native game output requires Windows WASAPI");
#endif
    }
    AudioStreamFormat format() const noexcept override{return format_;}
    std::optional<AudioQueueStatus> queue_status(AudioDeviceFailure& f) override{return device_->queue_status(f);}
    std::optional<AudioSubmission> queue(const float* pcm,std::size_t size,AudioDeviceFailure& f) override {
        return device_->queue_with_receipt(pcm,size,f);
    }
    bool start(AudioDeviceFailure& f) override {
        if(!device_->start(f))return false;started_=true;return true;
    }
    bool close(AudioDeviceFailure& f) noexcept override {
        f={};if(closed_)return true;
        try {
            AudioDeviceFailure stop_failure;
            if(device_){
                if(started_&&!device_->stop(stop_failure)){/* close still releases the failed native stream */}
                started_=false;
                const bool ok=device_->close(f);closed_=true;
                if(stop_failure.kind!=AudioDeviceError::none){f=stop_failure;return false;}
                return ok;
            }
            closed_=true;return true;
        }catch(...){return failed(f,AudioDeviceError::failed_owner,"exception closing native game output");}
    }
private:
    bool com_owned_{},started_{},closed_{};
    std::unique_ptr<NativeAudioDevice> device_;
    AudioStreamFormat format_;
};
}
std::unique_ptr<NativeSoundOutput> open_native_game_sound_output(AudioDeviceFailure& f){
    f={};
    try {auto output=std::make_unique<WasapiSoundOutput>();if(!output->initialize(f))return {};return output;}
    catch(const std::bad_alloc&){failed(f,AudioDeviceError::allocation,"native game output allocation");}
    catch(const std::length_error&){failed(f,AudioDeviceError::host_capacity,"native game output capacity");}
    catch(...){failed(f,AudioDeviceError::failed_owner,"exception opening native game output");}
    return {};
}
}
