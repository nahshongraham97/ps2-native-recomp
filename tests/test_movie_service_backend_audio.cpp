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
    return {MoviePlaybackAudioPresence::required,std::move(pcm),{clock->frequency,clock->step,0,0,MovieFrameEligibilityMode::direct},*clock,MovieClockFilterConfig::source_defaults(),MovieNativeTargetRoute::published_position,MovieNativeCompletion::all,true,true,false,0,0,0,d.rate,MovieOutputPositionProfile::successful_frame_no_repeat_no_skip};
}
}
int main(int argc,char** argv){try{
    check(argc==5,"usage: test_native_movie_service_backend_audio DLLdir asset_root LOGON_relative MOVIER.ovl");MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{"LOGON.SFD",argv[3]}},af);check(bool(store),"one existing LOGON association");
    std::ifstream original(std::filesystem::u8path(argv[4]),std::ios::binary);check(bool(original),"existing read-only color source");std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)),{});check(bytes.size()>=0x77dd8+6144,"existing table extent");MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"existing native color table");
    enum class Case {normal,clock_failure,reset_failure};
    for(auto scenario:{Case::normal,Case::clock_failure,Case::reset_failure}){
        auto data=std::make_shared<Data>();auto gpu=std::make_shared<GPU>();std::int64_t ticks=0;unsigned profiles=0;MovieServiceFailure failure;
        auto service=NativeMovieService::create_with_native_clock(1.0f,
            [data,&profiles](const RetailMovieRequest& request,std::shared_ptr<const MovieSourceObservationIdentity> id,const std::optional<sound::AudioStreamFormat>& actual,MovieSessionFailure& f)->std::optional<MovieServicePolicyConfiguration>{f={};++profiles;check(request.id==0&&request.logical_key=="LOGON.SFD"&&id==data->source&&actual&&*actual==sound::AudioStreamFormat{data->rate,data->channels,3},"actual same-source data determines client/profile join");return profile(request,*data);},
            [&,data,gpu](const RetailMovieRequest& request,std::int64_t language,MovieSessionPolicyFactory policies,NativeMovieControlContext& control,MovieSessionFailure& f)->std::unique_ptr<NativeMoviePlaybackSession>{
                MovieSourceFailure sf;auto source=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[1]),request.id,request.requested_view,language,sf);if(!source){f.kind=MovieSessionError::source;f.source=std::move(sf);return {};}
                auto present=source->audio_present(sf);if(!present||!*present){f.kind=MovieSessionError::source;f.operation="fixture requires actual audio source";f.source=std::move(sf);return {};}
                data->source=source->observation_identity();MovieControlFailure cf;auto input=own_movie_playback_source(std::move(source),cf);if(!input){f.kind=MovieSessionError::control;f.control=cf;return {};}
                std::deque<MovieReadResult> buffered;
                for(;;){auto r=input->next(cf);if(r.status!=MovieReadStatus::frame||cf.kind!=MovieControlError::none){f.kind=MovieSessionError::control;f.operation="fixture actual source ended/failed before audio metadata";f.control=cf;return {};}
                    const auto* a=std::get_if<MovieAudioFrame>(&r.frame);if(a){check(a->sample_rate>0&&a->channels.size()==2&&a->channel_layout.channel_count==a->channels.size(),"explicit two-plane fixture profile admits actual metadata");data->rate=a->sample_rate;data->channels=static_cast<std::uint32_t>(a->channels.size());data->layout=a->channel_layout;data->capacity=std::max(1u,data->rate/20);}
                    const bool found_audio=a!=nullptr;buffered.push_back(std::move(r));++data->prefetched;if(found_audio)break;
                }
                auto concrete=policies(request,data->source,sound::AudioStreamFormat{data->rate,data->channels,3},f);if(!concrete)return {};
                MoviePlaybackConfiguration config;config.audio=MoviePlaybackAudioPresence::required;config.client_sample_rate=data->rate;config.client_channels=data->channels;config.client_channel_mask=3;config.layout={1,640,448,MovieAspectMode::explicit_display_aspect,4,3,MovieWindowPolicy::centered_fit};config.filter=MovieFilter::nearest;config.queued_byte_budget=8*1024*1024;
                return NativeMoviePlaybackSession::create_from_ports(std::make_unique<BufferedInput>(std::move(input),std::move(buffered),data),std::move(concrete),std::make_unique<Audio>(data),gpu,*color,config,control,f);
            },[&](std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f){return NativeMovieHostClock::create_with_reader(key,std::move(id),1000,[&]{return std::optional<std::int64_t>{ticks};},f);},failure);
        check(service&&service->start(0,MovieViewSelection::normal,0,0,failure)&&profiles==1,"automatic actual-audio Service starts without parser/command/clock ingress");
        auto initial=service->snapshot(failure);check(initial&&initial->playback&&initial->playback->paused&&initial->playback->audio_time&&initial->playback->audio_time->elapsed.numerator==0&&!initial->playback->audio_time->active&&!data->started,"actual output zero anchor before PCM/Start");
        if(scenario==Case::clock_failure){data->clock_error=true;auto result=service->poll(failure);check(result.progress==MovieControlProgress::error&&result.failure.native_code==72&&data->destroyed==1&&data->resets==1&&!gpu->presents,"actual native clock error propagates with teardown before presentation");continue;}
        bool complete=false,saw_audio_start=false,saw_video_start=false,saw_output_time=false;std::uint64_t polls=0;
        for(;polls<20000&&!complete;++polls){data->advance();ticks+=10;auto result=service->poll(failure);
            if(result.progress==MovieControlProgress::error){std::fprintf(stderr,"audio backend failure poll%llu %s / %s code%lld\n",(unsigned long long)polls,failure.operation,result.failure.operation,(long long)result.failure.native_code);check(false,"automatic audio backend remains successful");}
            auto state=service->snapshot(failure);check(bool(state),"actual automatic source/output observation");
            if(state->context){if(state->context->audio_started){check(state->context->accepted_audio_frames>0&&data->total_accepted>0,"audio stage start follows successful accepted prefill");saw_audio_start=true;}saw_video_start|=state->context->video_started;}
            if(state->playback&&state->playback->audio_time){const auto& time=*state->playback->audio_time;check(time.source==data->source&&time.output_clock&&time.output_clock->frequency==1000003&&!time.output_clock->precise,"actual raw native clock precision/units/source preserved");if(time.elapsed.numerator>0)saw_output_time=true;}
            if(scenario==Case::reset_failure&&data->total_accepted){data->reset_error=true;check(!service->close(failure)&&failure.cleanup.control.native_code==71&&data->resets==1&&data->destroyed==1&&gpu->retired==gpu->presents,"actual reset failure retains named cleanup while releasing source/fences");complete=true;break;}
            complete=result.progress==MovieControlProgress::complete;
            if(complete){check(result.completion==MovieControlCompletion::backend_ended&&state->backend&&state->backend->worker_stopped&&state->native_work&&state->native_work->flushed&&!state->native_work->retained_inputs&&!state->native_work->retained_outputs&&state->context&&state->context->audio_finished&&state->context->video_finished&&state->context->stop_acknowledged,"actual audio/video work and native quiescence complete without EOF shortcut");check(state->playback&&state->playback->cancelled_source_retirement&&state->playback->cancelled_source_retirement->pending==0&&state->playback->cancelled_source_retirement->retired==data->total_accepted&&state->context->accepted_audio_frames==data->total_accepted,"all actual accepted source audio retires before successful reset cancellation");}
        }
        check(complete,"fixture protocol watchdog (200s simulated time), not a runtime movie capacity");
        if(scenario==Case::normal){check(saw_audio_start&&saw_video_start&&saw_output_time&&data->resume_after_prefill&&data->starts==1&&data->queued>1&&data->buffered_forwarded==data->prefetched&&data->destroyed==1&&data->resets==1&&gpu->presents>0&&gpu->uploads==gpu->presents&&gpu->retired==gpu->presents,"actual automatic preparation/audio ledger/start/drain with all buffered reads forwarded and GPU owners retired");check(std::printf("{\"source\":\"LOGON.SFD\",\"source_rate\":%u,\"source_planes\":%u,\"prefetched_reads\":%u,\"forwarded_reads\":%u,\"accepted_client_frames\":%llu,\"output_starts\":%u,\"resets\":%u,\"submissions\":%llu,\"fences_retired\":%llu,\"polls\":%llu}\n",data->rate,data->channels,data->prefetched,data->buffered_forwarded,(unsigned long long)data->total_accepted,data->starts,data->resets,(unsigned long long)gpu->presents,(unsigned long long)gpu->retired,(unsigned long long)polls)>=0,"checked audio measurement");}
        check(service->close(failure),"idempotent final source cancellation");
    }
    check(std::puts("PASS automatic native audio Service timing/acceptance/drain deterministic ports; no physical playback or content comparison")>=0&&std::fflush(stdout)==0,"checked footer");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL automatic audio service %s\n",e.what());return 1;}}
