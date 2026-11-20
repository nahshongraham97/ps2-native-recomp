#pragma once
// Deterministic integration ports; no physical endpoint or renderer.
#include "mka/movie_service.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <set>
#include <stdexcept>
using namespace mka::video;
namespace sound=mka::sound;
namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Data {
    std::uint32_t rate=0,channels=0,capacity=0,padding=0;
    MovieAudioChannelLayout layout;
    std::shared_ptr<const MovieSourceObservationIdentity> source;
    std::uint64_t submitted=0,retired=0,total_accepted=0,epoch=0,position=0;
    unsigned starts=0,resets=0,queued=0,prefetched=0,buffered_forwarded=0,destroyed=0;
    bool started=false,clock_error=false,reset_error=false,resume_after_prefill=false;
    void advance(){if(started){const auto n=std::min(padding,std::max(1u,rate/100));padding-=n;retired+=n;position+=10000;}}
};
struct BufferedInput final:MoviePlaybackInput {
    std::unique_ptr<MoviePlaybackInput> actual;std::deque<MovieReadResult> pending;std::shared_ptr<Data> data;
    BufferedInput(std::unique_ptr<MoviePlaybackInput> input,std::deque<MovieReadResult> reads,std::shared_ptr<Data> d):actual(std::move(input)),pending(std::move(reads)),data(std::move(d)){}
    ~BufferedInput()override{++data->destroyed;}
    const RetailMovieRequest& request()const noexcept override{return actual->request();}
    std::shared_ptr<const MovieSourceObservationIdentity> observation_identity()const noexcept override{return actual->observation_identity();}
    MovieReadResult next(MovieControlFailure& f)override{f={};if(pending.empty())return actual->next(f);auto r=std::move(pending.front());pending.pop_front();++data->buffered_forwarded;return r;}
};
struct Audio final:MoviePlaybackAudioOutput {
    std::shared_ptr<Data> d;explicit Audio(std::shared_ptr<Data> data):d(std::move(data)){}
    std::optional<sound::AudioStreamFormat> format(MovieControlFailure& f)override{f={};return sound::AudioStreamFormat{d->rate,d->channels,3};}
    std::optional<sound::AudioQueueStatus> queue_status(MovieControlFailure& f)override{f={};return sound::AudioQueueStatus{d->capacity,d->padding,d->capacity-d->padding,d->submitted,d->epoch,d->started};}
    std::optional<sound::AudioClockReading> clock(MovieControlFailure& f)override{f={};if(d->clock_error){f={MovieControlError::native_service,"fixture output clock query failed",72};return {};}return sound::AudioClockReading{d->position,1000003,998877,d->epoch,false};}
    MovieAudioSubmitResult queue(std::span<const float> pcm,MovieControlFailure& f)override{f={};check(d->channels&&pcm.size()%d->channels==0,"complete actual client frames");const auto count=pcm.size()/d->channels;check(count>0&&count<=d->capacity-d->padding,"actual capacity respected");for(float v:pcm)check(std::isfinite(v),"finite prepared PCM only; no content comparison");sound::AudioSubmission receipt{d->submitted,count,d->epoch};d->submitted+=count;d->total_accepted+=count;d->padding+=static_cast<std::uint32_t>(count);++d->queued;return {MovieAudioSubmitStatus::accepted,receipt};}
    bool pause(bool p,MovieControlFailure& f)override{f={};if(p)d->started=false;else if(!d->started&&d->padding){check(d->submitted>0&&d->total_accepted>0,"successful acceptance precedes output Start");d->resume_after_prefill=true;d->started=true;++d->starts;}return true;}
    bool stop_and_reset(MovieControlFailure& f)override{f={};++d->resets;d->started=false;if(d->reset_error){f={MovieControlError::native_service,"fixture native output Reset failed",71};return false;}d->padding=0;d->submitted=0;d->position=0;++d->epoch;return true;}
};
struct Frame final:MovieGpuFrame {
    MovieGpuMetadata m;std::uint32_t w,h;std::uint64_t id;
    Frame(const MovieRgbaFrame& r,std::uint64_t n):w(r.width),h(r.height),id(n){m.conversion_role=r.conversion_role;m.codec_timing=r.codec_timing;m.source_pts=r.source_pts;m.timestamp=r.timestamp;m.sample_aspect_numerator=r.source_sample_aspect_numerator;m.sample_aspect_denominator=r.source_sample_aspect_denominator;m.color_range=r.source_color_range;m.color_space=r.source_color_space;m.color_primaries=r.source_color_primaries;m.color_transfer=r.source_color_transfer;}
    std::uint64_t identity()const noexcept override{return id;}std::uint32_t width()const noexcept override{return w;}std::uint32_t height()const noexcept override{return h;}const MovieGpuMetadata& metadata()const noexcept override{return m;}
};
struct Fence final:MovieGpuFence {unsigned polls=0;explicit Fence(NativeMoviePresentPlan p):MovieGpuFence(std::move(p)){} };
struct GPU final:NativeMovieGpuBackend {
    std::uint64_t uploads=0,presents=0,retired=0;std::set<std::uint64_t> selected;
    MovieGpuUpload upload(const MovieRgbaFrame& r)override{check(validate_movie_upload(r)==MovieGpuError::none,"actual prepared frame metadata valid");return {MovieGpuError::none,std::make_shared<Frame>(r,++uploads)};}
    MovieGpuSubmission present(const NativeMoviePresentPlan& p,const MovieHostLayout& l)override{check(validate_movie_present(p)==MovieGpuError::none&&movie_destination(p,l).has_value(),"actual native presentation plan");check(p.frame->metadata().codec_timing.picture_source&&selected.insert(p.frame->metadata().codec_timing.picture_source->picture_es_byte).second,"selected source submitted once");++presents;return {MovieGpuError::none,std::make_shared<Fence>(p)};}
    MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>& f)override{auto owned=std::dynamic_pointer_cast<Fence>(f);check(bool(owned),"owned fence identity");return {++owned->polls>1?MovieFenceState::complete:MovieFenceState::pending,MovieGpuError::none};}
    void retire(std::shared_ptr<MovieGpuFence> f)noexcept override{if(f)++retired;}
};
MovieServicePolicyConfiguration profile(const RetailMovieRequest& request,const Data& d){
    MovieServiceClockFailure why;auto clock=movie_service_source_config(MovieServiceSourceMode::mode2,why);check(bool(clock),"explicit source service configuration");
    MoviePcmBindingConfiguration pcm;pcm.source_key=request.logical_key;pcm.source_layout=d.layout;pcm.source_sample_rate=d.rate;pcm.client_format={d.rate,d.channels,3};pcm.mode=MoviePcmMixMode::generic_stateless;pcm.plane_policy=MoviePcmPlanePolicy::preserve;
    // Explicit fixture business/speaker routing. UNSPEC codec planes are not
    // relabeled as stereo or claimed to be actual retail settings.
    pcm.routes.emplace_back(0,-15,0,1);pcm.routes.emplace_back(0,15,0,1);
    return {MoviePlaybackAudioPresence::required,std::move(pcm),{clock->frequency,clock->step,0,0,MovieFrameEligibilityMode::direct},*clock,MovieClockFilterConfig::source_defaults(),MovieNativeTargetRoute::published_position,MovieNativeCompletion::all,true,true,false,0,0,d.epoch,d.rate,MovieOutputPositionProfile::successful_frame_no_repeat_no_skip};
}
}
