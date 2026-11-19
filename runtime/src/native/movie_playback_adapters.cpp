#include "mka/movie_playback.hpp"
#include <new>

namespace mka::video {
namespace {
void audio_failure(MovieControlFailure& target,const sound::AudioDeviceFailure& f) noexcept {
    target={f.kind==sound::AudioDeviceError::wrong_thread?MovieControlError::wrong_thread:
        MovieControlError::native_service,f.operation,f.native_code};
}
class OwnedSource final : public MoviePlaybackInput {
public:
    explicit OwnedSource(std::unique_ptr<NativeRetailMovieSource> s):source_(std::move(s)){}
    const RetailMovieRequest& request() const noexcept override {return *source_->request();}
    MovieReadResult next(MovieControlFailure& f) override {
        f={};MovieSourceFailure sf;auto result=source_->next(sf);
        if(result.status==MovieReadStatus::error)
            f={MovieControlError::native_service,sf.operation,sf.decoder_failure.native_code};
        return result;
    }
private:
    std::unique_ptr<NativeRetailMovieSource> source_;
};
class OwnedAudio final : public MoviePlaybackAudioOutput {
public:
    explicit OwnedAudio(std::unique_ptr<sound::NativeAudioDevice> d):device_(std::move(d)){}
    std::optional<sound::AudioStreamFormat> format(MovieControlFailure& f) override {
        f={};sound::AudioDeviceFailure af;auto s=device_->stream_format(af);
        if(!s)audio_failure(f,af);return s;
    }
    std::optional<sound::AudioQueueStatus> queue_status(MovieControlFailure& f) override {
        f={};sound::AudioDeviceFailure af;auto s=device_->queue_status(af);
        if(!s)audio_failure(f,af);return s;
    }
    std::optional<sound::AudioClockReading> clock(MovieControlFailure& f) override {
        f={};sound::AudioDeviceFailure af;auto c=device_->clock(af);
        if(!c)audio_failure(f,af);return c;
    }
    MovieAudioSubmitResult queue(std::span<const float> pcm,MovieControlFailure& f) override {
        f={};sound::AudioDeviceFailure af;
        auto receipt=device_->queue_with_receipt(pcm.data(),pcm.size(),af);
        if(receipt)return {MovieAudioSubmitStatus::accepted,receipt};
        if(af.kind==sound::AudioDeviceError::buffer_full)return {MovieAudioSubmitStatus::full,{}};
        audio_failure(f,af);return {};
    }
    bool pause(bool paused,MovieControlFailure& f) override {
        f={};sound::AudioDeviceFailure af;auto s=device_->queue_status(af);
        if(!s){audio_failure(f,af);return false;}
        if(paused&&s->started) {
            if(!device_->stop(af)){audio_failure(f,af);return false;}
        } else if(!paused&&!s->started&&s->padding_frames) {
            if(!device_->start(af)){audio_failure(f,af);return false;}
        }
        return true;
    }
    bool stop_and_reset(MovieControlFailure& f) override {
        f={};sound::AudioDeviceFailure af;auto s=device_->queue_status(af);
        if(!s){audio_failure(f,af);return false;}
        if(s->started&&!device_->stop(af)){audio_failure(f,af);return false;}
        if(!device_->reset(af)){audio_failure(f,af);return false;}
        return true;
    }
private:
    std::unique_ptr<sound::NativeAudioDevice> device_;
};
}
std::unique_ptr<MoviePlaybackInput> own_movie_playback_source(
        std::unique_ptr<NativeRetailMovieSource> source,MovieControlFailure& f) {
    f={};
    if(!source||!source->valid()||!source->request()) {
        f={MovieControlError::native_service,"movie playback requires live owned native source",0};return {};
    }
    try {return std::make_unique<OwnedSource>(std::move(source));}
    catch(const std::bad_alloc&){f={MovieControlError::native_service,"movie playback source adapter allocation",0};return {};}
}
std::unique_ptr<MoviePlaybackAudioOutput> own_movie_playback_audio(
        std::unique_ptr<sound::NativeAudioDevice> device,MovieControlFailure& f) {
    f={};
    if(!device){f={MovieControlError::native_service,"movie playback requires explicit native audio owner",0};return {};}
    sound::AudioDeviceFailure af;
    if(!device->stream_format(af)){audio_failure(f,af);return {};}
    try {return std::make_unique<OwnedAudio>(std::move(device));}
    catch(const std::bad_alloc&){f={MovieControlError::native_service,"movie playback audio adapter allocation",0};return {};}
}
} // namespace mka::video
