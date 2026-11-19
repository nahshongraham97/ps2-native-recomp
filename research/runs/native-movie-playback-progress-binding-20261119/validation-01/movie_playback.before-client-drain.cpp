#include "mka/movie_playback.hpp"
#include "mka/movie_gain.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#ifdef __FAST_MATH__
#error Native movie playback requires finite-value checks
#endif

namespace mka::video {
namespace {
bool error(MovieControlFailure& f, const char* operation) noexcept {
    if(f.kind==MovieControlError::none) f={MovieControlError::native_service,operation,0};
    return false;
}
bool valid_status(const sound::AudioQueueStatus& s) noexcept {
    return s.capacity_frames && std::uint64_t(s.padding_frames)+s.writable_frames==s.capacity_frames;
}
template<class T> void reserve_one(std::vector<T>& v) {
    if(v.size()<v.capacity())return;
    const auto remaining=v.max_size()-v.capacity();
    const auto growth=std::min(remaining,v.capacity()/2+1);
    v.reserve(v.capacity()+growth);
}
}
NativeMovieStreamPlayback::NativeMovieStreamPlayback(std::unique_ptr<MoviePlaybackInput> i,
        std::unique_ptr<MoviePlaybackPolicies> p, std::unique_ptr<MoviePlaybackAudioOutput> a,
        NativeMovieGpuBackend& g, NativeMovieColorTable c, MoviePlaybackConfiguration cfg,
        std::int32_t gain)
    : input_(std::move(i)),policies_(std::move(p)),audio_(std::move(a)),gpu_(g),
      color_(std::move(c)),config_(std::move(cfg)),gain_(gain) {}

std::unique_ptr<NativeMovieStreamPlayback> NativeMovieStreamPlayback::create(
        std::unique_ptr<MoviePlaybackInput> input, std::unique_ptr<MoviePlaybackPolicies> policies,
        std::unique_ptr<MoviePlaybackAudioOutput> audio, NativeMovieGpuBackend& gpu,
        NativeMovieColorTable color, MoviePlaybackConfiguration config, MovieControlFailure& f) {
    f={};
    try {
        if(!input||!policies||input->request().logical_key.empty()||
           input->request().logical_key.find('\0')!=std::string::npos) {
            error(f,"movie playback requires owned source and explicit policies");return {};
        }
        if(!config.initial_base_volume) {error(f,"movie playback missing initial source volume");return {};}
        MovieGainFailure gain_failure;
        const auto gain=movie_gain_initial(*config.initial_base_volume,gain_failure);
        if(!gain) {f={MovieControlError::invalid_volume,gain_failure.operation,0};return {};}
        if(!config.queued_byte_budget||color.role()==MovieColorRole::unspecified||
           (config.filter!=MovieFilter::nearest&&config.filter!=MovieFilter::linear)||
           !config.layout.window_width||!config.layout.window_height||
           config.layout.policy!=MovieWindowPolicy::centered_fit||
           (config.layout.aspect!=MovieAspectMode::source_sample_aspect&&
            config.layout.aspect!=MovieAspectMode::explicit_display_aspect)||
           (config.layout.aspect==MovieAspectMode::explicit_display_aspect&&
            (!config.layout.display_aspect_numerator||!config.layout.display_aspect_denominator))) {
            error(f,"movie playback explicit host presentation and queue policy");return {};
        }
        std::optional<sound::AudioQueueStatus> initial;
        if(config.audio==MoviePlaybackAudioPresence::required) {
            if(!audio||!config.client_sample_rate||!config.client_channels||!config.client_channel_mask) {
                error(f,"movie playback missing required native audio output format");return {};
            }
            const auto actual=audio->format(f);
            if(!actual||*actual!=sound::AudioStreamFormat{config.client_sample_rate,
                   config.client_channels,config.client_channel_mask}) {
                error(f,"movie playback client format differs from owned output");return {};
            }
            initial=audio->queue_status(f);
            if(!initial||!valid_status(*initial)||initial->started||initial->submitted_frames||initial->padding_frames) {
                error(f,"movie playback requires exclusive empty stopped audio stream");return {};
            }
        } else if(config.audio!=MoviePlaybackAudioPresence::absent||audio||
                  config.client_sample_rate||config.client_channels||config.client_channel_mask) {
            error(f,"movie playback explicit audio presence mismatch");return {};
        }
        auto result=std::unique_ptr<NativeMovieStreamPlayback>(new NativeMovieStreamPlayback(
            std::move(input),std::move(policies),std::move(audio),gpu,std::move(color),config,*gain));
        result->next_audio_=initial;
        return result;
    } catch(...) {error(f,"movie playback construction allocation or service exception");return {};}
}
NativeMovieStreamPlayback::~NativeMovieStreamPlayback() {
    if(std::this_thread::get_id()!=owner_) std::terminate();
    MovieControlFailure f;
    stop(f);
}
bool NativeMovieStreamPlayback::owner(MovieControlFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_) {
        f={MovieControlError::wrong_thread,"movie playback owner thread",0};return false;
    }
    return true;
}
bool NativeMovieStreamPlayback::fail(MovieControlFailure& f,const char* operation) noexcept {
    error(f,operation);
    if(terminal_.kind==MovieControlError::none) {terminal_=f;error_pending_=true;}
    return false;
}
bool NativeMovieStreamPlayback::pause(bool value,MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    if(terminal_.kind!=MovieControlError::none){f=terminal_;return false;}
    try {
        cleanup_needed_=true;
        if(!policies_->pause(value,f))return fail(f,"movie playback policy pause");
        if(audio_&&!audio_->pause(value,f))return fail(f,"movie playback native audio pause");
        if(source_progress_) {
            MovieAudioProgressFailure pf;
            if(!source_progress_->pause(value,pf))return fail(f,"movie playback source retirement pause");
        }
        paused_=value;return true;
    }catch(...){return fail(f,"movie playback pause exception");}
}
bool NativeMovieStreamPlayback::start(std::string_view key,MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    if(used_||terminal_.kind!=MovieControlError::none) {
        f={MovieControlError::invalid_state,"movie playback source is single use",0};return false;
    }
    if(key!=input_->request().logical_key) {
        f={MovieControlError::invalid_key,"movie playback exact owned request key",0};return false;
    }
    used_=true;active_=true;cleanup_needed_=true;
    try {
        if(!policies_->begin(input_->request(),gain_,f))return fail(f,"movie playback admitted initialization");
        return true;
    }catch(...){return fail(f,"movie playback begin exception");}
}
bool NativeMovieStreamPlayback::pump_metadata(MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    if(terminal_.kind!=MovieControlError::none){f=terminal_;return false;}
    if(!active_)return true;
    try {
        if(!policies_->pump_metadata(f))return fail(f,"movie playback same-asset metadata pump");
        return true;
    }catch(...){return fail(f,"movie playback metadata exception");}
}
std::size_t NativeMovieStreamPlayback::queued_bytes() const noexcept {
    std::size_t bytes=0;
    const auto add=[&](std::size_t count,std::size_t width) {
        if(count>(SIZE_MAX-bytes)/width)bytes=SIZE_MAX;
        else bytes+=count*width;
    };
    for(const auto& v:video_)for(const auto& p:v.source.planes)add(p.size(),1);
    for(const auto& a:pcm_) {
        for(const auto& p:a.source.channels)add(p.size(),sizeof(std::int16_t));
        add(a.client.size(),sizeof(float));
    }
    return bytes;
}
bool NativeMovieStreamPlayback::poll_fences(MovieControlFailure& f) {
    for(std::size_t k=0;k<fences_.size();) {
        const auto result=gpu_.poll(fences_[k]);
        if(result.state==MovieFenceState::pending&&result.error==MovieGpuError::none){++k;continue;}
        gpu_.retire(std::move(fences_[k]));
        fences_.erase(fences_.begin()+k);
        if(result.state!=MovieFenceState::complete||result.error!=MovieGpuError::none)
            return fail(f,"movie playback native GPU completion");
    }
    return true;
}
bool NativeMovieStreamPlayback::submit_audio(MovieControlFailure& f,bool& blocked) {
    blocked=false;
    while(!pcm_.empty()) {
    auto& packet=pcm_.front();
    if(!packet.prepared) {
        if(!source_progress_) {
            MovieAudioProgressFailure pf;
            // Explicit decoder-relative sample INDEX, never the retail clock
            // origin. All accepted source chunks already carry this same index.
            source_progress_=NativeMovieAudioProgress::create({input_->request().logical_key,
                0,packet.source.sample_rate,config_.client_sample_rate,next_audio_->reset_epoch},pf);
            if(!source_progress_)return fail(f,"movie playback equal-rate source progress admission");
            source_audio_rate_=packet.source.sample_rate;
            if(!source_progress_->pause(paused_,pf))return fail(f,"movie playback source progress pause admission");
        }
        if(!source_audio_rate_||packet.source.sample_rate!=*source_audio_rate_)
            return fail(f,"movie playback source sample rate changed before acceptance");
        if(!policies_->prepare_audio(packet.source,gain_,packet.client,f))
            return fail(f,"movie playback admitted PCM transform");
        if(packet.client.empty()||packet.client.size()%config_.client_channels)
            return fail(f,"movie playback complete owned interleaved client PCM");
        if(packet.client.size()/config_.client_channels!=packet.source.channels.front().size())
            return fail(f,"movie playback equal-cardinality PCM required before acceptance");
        for(const float sample:packet.client)if(!std::isfinite(sample))
            return fail(f,"movie playback finite client PCM");
        packet.prepared=true;
    }
    auto available=audio_->queue_status(f);
    if(!available||!valid_status(*available)||!next_audio_||
       available->reset_epoch!=next_audio_->reset_epoch||
       available->submitted_frames!=next_audio_->submitted_frames)
        return fail(f,"movie playback exclusive audio epoch and sequence");
    if(!available->writable_frames){blocked=true;return true;}
    const std::size_t remain=(packet.client.size()-packet.accepted_samples)/config_.client_channels;
    const auto count=std::min<std::size_t>(remain,available->writable_frames);
    if(count>UINT64_MAX-available->submitted_frames)
        return fail(f,"movie playback submitted range overflow");
    // Reserve before any irreversible host acceptance. After successful queue,
    // recording this receipt cannot fail allocation or trigger a duplicate retry.
    if(accepted_.size()==accepted_.max_size())return fail(f,"movie playback receipt host capacity");
    reserve_one(accepted_);
    const auto result=audio_->queue(std::span<const float>(packet.client).subspan(
        packet.accepted_samples,count*config_.client_channels),f);
    if(result.status==MovieAudioSubmitStatus::full&&!result.submission&&f.kind==MovieControlError::none){blocked=true;return true;}
    if(result.status!=MovieAudioSubmitStatus::accepted||!result.submission||f.kind!=MovieControlError::none)
        return fail(f,"movie playback native audio submission");
    const auto receipt=*result.submission;
    if(receipt.reset_epoch!=available->reset_epoch||receipt.first_client_frame!=available->submitted_frames||
       receipt.client_frames!=count)
        return fail(f,"movie playback malformed accepted receipt; never retry accepted packet");
    const MoviePlaybackAudioAcceptance acceptance{packet.first_source,
        packet.source.channels.front().size(),packet.client.size()/config_.client_channels,
        packet.accepted_samples/config_.client_channels,receipt};
    accepted_.push_back(acceptance);
    packet.accepted_samples+=count*config_.client_channels;
    next_audio_->submitted_frames=receipt.first_client_frame+receipt.client_frames;
    MovieAudioProgressFailure pf;
    if(!source_progress_->accept(input_->request().logical_key,packet.source.sample_rate,acceptance,pf))
        return fail(f,"movie playback source/client retirement ledger admission");
    if(!policies_->audio_accepted(acceptance,f))return fail(f,"movie playback source/client ledger admission");
    if(packet.accepted_samples==packet.client.size())pcm_.pop_front();
    if(!audio_->pause(paused_,f))return fail(f,"movie playback native audio start after prequeue");
    }
    return true;
}
bool NativeMovieStreamPlayback::pump_streams(MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    if(terminal_.kind!=MovieControlError::none){f=terminal_;return false;}
    if(!active_)return true;
    try {
        bool blocked=false;
        if(!poll_fences(f)||!submit_audio(f,blocked))return false;
        while(!eof_&&queued_bytes()<config_.queued_byte_budget) {
            if(!video_.empty()) {
                if(!audio_||blocked)break;
                const auto available=audio_->queue_status(f);
                if(!available||!valid_status(*available)||!next_audio_||
                   available->reset_epoch!=next_audio_->reset_epoch||
                   available->submitted_frames!=next_audio_->submitted_frames)
                    return fail(f,"movie playback native pump capacity observation");
                if(!available->writable_frames)break;
            }
            auto read=input_->next(f);
            if(read.status==MovieReadStatus::end) {
                if(f.kind!=MovieControlError::none)return fail(f,"movie playback contradictory source end");
                if(config_.audio==MoviePlaybackAudioPresence::required&&!decoded_audio_)
                    return fail(f,"movie playback required source audio missing at EOF");
                eof_=true;
            } else if(read.status==MovieReadStatus::frame&&f.kind==MovieControlError::none) {
                if(auto* v=std::get_if<MovieVideoFrame>(&read.frame)) {
                    if(decoded_video_==UINT64_MAX)return fail(f,"movie playback video ordinal host capacity");
                    video_.push_back({std::move(*v),{}});++decoded_video_;
                } else {
                    auto& a=std::get<MovieAudioFrame>(read.frame);
                    if(!audio_||!a.sample_rate||a.channels.empty()||a.channels.front().empty()||
                       a.channel_layout.channel_count!=a.channels.size())
                        return fail(f,"movie playback unexpected or invalid source audio");
                    for(const auto& plane:a.channels)if(plane.size()!=a.channels.front().size())
                        return fail(f,"movie playback complete planar source audio");
                    if(a.channels.front().size()>UINT64_MAX-source_audio_cursor_||decoded_audio_==UINT64_MAX)
                        return fail(f,"movie playback source audio extent");
                    const auto size=a.channels.front().size();
                    pcm_.push_back({std::move(a),{},source_audio_cursor_,0,false});
                    source_audio_cursor_+=size;++decoded_audio_;
                }
            } else return fail(f,"movie playback native source read");
            if(!blocked&&!submit_audio(f,blocked))return false;
        }
        return true;
    }catch(...){return fail(f,"movie playback stream allocation or service exception");}
}
bool NativeMovieStreamPlayback::snapshot(MoviePlaybackSnapshot& s,MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    try {
        s={eof_,paused_,decoded_video_,decoded_audio_,video_.size(),pcm_.size(),fences_.size(),{}, {}, {}, cancelled_progress_};
        if(audio_) {
            s.audio_queue=audio_->queue_status(f);if(!s.audio_queue)return fail(f,"movie playback audio status");
            s.device_clock=audio_->clock(f);if(!s.device_clock)return fail(f,"movie playback native clock reading");
            if(!valid_status(*s.audio_queue)||!s.device_clock->frequency||!next_audio_||
               s.audio_queue->reset_epoch!=next_audio_->reset_epoch||
               s.audio_queue->submitted_frames!=next_audio_->submitted_frames||
               s.device_clock->reset_epoch!=next_audio_->reset_epoch)
                return fail(f,"movie playback observed audio epoch and sequence");
            if(source_progress_) {
                MovieAudioProgressFailure pf;
                s.source_retirement=source_progress_->observe(*s.audio_queue,pf);
                if(!s.source_retirement)return fail(f,"movie playback same-observation source retirement");
            }
        }
        return true;
    }catch(...){return fail(f,"movie playback snapshot exception");}
}
bool NativeMovieStreamPlayback::status(MoviePlaybackStatus& result,MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    if(terminal_.kind!=MovieControlError::none){result=MoviePlaybackStatus::error;f=terminal_;return false;}
    result=MoviePlaybackStatus::stopped;if(!active_)return true;
    try {
        MoviePlaybackSnapshot s;
        if(!snapshot(s,f))return false;
        bool ready=false;
        if(!policies_->ready(s,ready,f))return fail(f,"movie playback explicit readiness policy");
        result=ready?MoviePlaybackStatus::playing:MoviePlaybackStatus::preparing;
        if(eof_&&video_.empty()&&pcm_.empty()&&fences_.empty()) {
            bool ended=false;
            if(!policies_->end_admitted(s,ended,f))return fail(f,"movie playback explicit source/output drain policy");
            if(ended)result=MoviePlaybackStatus::ended;
        }
        return true;
    }catch(...){return fail(f,"movie playback status policy exception");}
}
MoviePresentation NativeMovieStreamPlayback::try_present(MovieControlFailure& f) noexcept {
    if(!owner(f))return MoviePresentation::error;
    if(terminal_.kind!=MovieControlError::none){f=terminal_;return MoviePresentation::error;}
    if(!active_||paused_||video_.empty())return MoviePresentation::no_frame;
    try {
        MoviePlaybackSnapshot s;
        if(!snapshot(s,f))return MoviePresentation::error;
        auto& next=video_.front();bool eligible=false;
        if(!policies_->eligible(next.source,s,eligible,f)) {fail(f,"movie playback explicit frame eligibility");return MoviePresentation::error;}
        if(!eligible)return MoviePresentation::no_frame;
        if(!next.uploaded) {
            MovieColorError ce;
            auto rgba=convert_movie_yuv420(next.source,color_,ce);
            if(!rgba){fail(f,"movie playback admitted source color conversion");return MoviePresentation::error;}
            auto upload=gpu_.upload(*rgba);
            if(upload.error!=MovieGpuError::none||!upload.frame) {fail(f,"movie playback native frame upload");return MoviePresentation::error;}
            next.uploaded=std::move(upload.frame);
        }
        NativeMoviePresentPlan plan{next.uploaded,{0,0,next.source.width,next.source.height},config_.filter,MovieAlpha::replace_rgba8};
        if(fences_.size()==fences_.max_size()){fail(f,"movie playback fence host capacity");return MoviePresentation::error;}
        reserve_one(fences_);
        auto submission=gpu_.present(plan,config_.layout);
        // Even a failed submission may return an owned in-flight resource.
        if(submission.fence)fences_.push_back(submission.fence);
        if(submission.error!=MovieGpuError::none||!submission.fence||submission.fence->plan.frame!=next.uploaded) {
            fail(f,"movie playback native frame submission");return MoviePresentation::error;
        }
        if(!policies_->video_submitted(next.uploaded->metadata(),f)) {
            fail(f,"movie playback source video submission observation");return MoviePresentation::error;
        }
        video_.pop_front();return MoviePresentation::presented;
    }catch(...){fail(f,"movie playback presentation allocation or service exception");return MoviePresentation::error;}
}
bool NativeMovieStreamPlayback::apply_fade_volume(const MovieFadeStep& step,MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    if(terminal_.kind!=MovieControlError::none){f=terminal_;return false;}
    try {
        MovieGainFailure gf;auto gain=movie_gain_fade(step,gf);
        if(!gain){f={MovieControlError::invalid_volume,gf.operation,0};return fail(f,"movie playback fade gain");}
        if(!policies_->set_gain(*gain,f))return fail(f,"movie playback admitted source gain");
        gain_=*gain;return true;
    }catch(...){return fail(f,"movie playback gain policy exception");}
}
bool NativeMovieStreamPlayback::take_error(MovieControlFailure& f) noexcept {
    if(!owner(f))return true;
    if(!error_pending_)return false;
    f=terminal_;error_pending_=false;return true;
}
bool NativeMovieStreamPlayback::stop(MovieControlFailure& f) noexcept {
    if(!owner(f))return false;
    MovieControlFailure first;
    if(!cleanup_needed_&&fences_.empty()&&video_.empty()&&pcm_.empty())return true;
    // Attempt all cleanup operations even if an earlier service failed.
    if(audio_) {
        try {
            if(!audio_->pause(true,f))error(f,"movie playback stop before reset observation");
            else {
                paused_=true;
                if(source_progress_) {
                    MovieAudioProgressFailure pf;
                    if(!source_progress_->pause(true,pf))error(f,"movie playback retirement stopped publication");
                }
            }
        }catch(...){error(f,"movie playback native stop cleanup exception");}
        if(f.kind!=MovieControlError::none)first=f;
        f={};
    }
    if(source_progress_&&audio_) {
        try {
            const auto before=audio_->queue_status(f);
            MovieAudioProgressFailure pf;
            if(!before||!source_progress_->observe(*before,pf))
                error(f,"movie playback source retirement before reset");
        }catch(...){error(f,"movie playback source retirement cleanup observation exception");}
        if(first.kind==MovieControlError::none&&f.kind!=MovieControlError::none)first=f;
        f={};
    }
    try {
        if(audio_) {
            if(!audio_->stop_and_reset(f))error(f,"movie playback audio cleanup");
            else {
                auto actual=audio_->queue_status(f);
                if(!actual||!valid_status(*actual)||actual->started||actual->padding_frames||actual->submitted_frames)
                    error(f,"movie playback audio reset state");
                else {
                    next_audio_=actual;
                    if(source_progress_) {
                        MovieAudioProgressFailure pf;
                        if(!source_progress_->pause(true,pf))error(f,"movie playback source retirement stop");
                        else {
                            // All pending decoded data is being abandoned. The
                            // new unused epoch explicitly starts at the decoder
                            // cursor; the prior cancelled epoch stays separate.
                            auto cancelled=source_progress_->reset({input_->request().logical_key,
                                source_audio_cursor_,*source_audio_rate_,config_.client_sample_rate,
                                actual->reset_epoch},*actual,pf);
                            if(!cancelled)error(f,"movie playback source retirement reset cancellation");
                            else cancelled_progress_=std::move(cancelled);
                        }
                    }
                }
            }
        }
    }
    catch(...){error(f,"movie playback audio cleanup exception");}
    if(first.kind==MovieControlError::none&&f.kind!=MovieControlError::none)first=f;
    f={};
    try {if(!policies_->stop(f))error(f,"movie playback policy cleanup");}
    catch(...){error(f,"movie playback policy cleanup exception");}
    if(first.kind==MovieControlError::none&&f.kind!=MovieControlError::none)first=f;
    for(auto& fence:fences_)gpu_.retire(std::move(fence));
    fences_.clear();video_.clear();pcm_.clear();active_=false;paused_=true;cleanup_needed_=false;
    f=first;
    if(f.kind!=MovieControlError::none)return fail(f,"movie playback cleanup failure");
    return true;
}
} // namespace mka::video
