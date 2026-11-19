#include "mka/movie_playback.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <thread>
using namespace mka::video;
namespace sound=mka::sound;
namespace {
void check(bool v,const char* text) {if(!v)throw std::runtime_error(text);}
bool error(MovieControlFailure& f,const char* op) {f={MovieControlError::native_service,op,17};return false;}
struct Trace {std::vector<std::string> calls;};
struct Input final:MoviePlaybackInput {
    RetailMovieRequest requested;std::vector<MovieReadResult> events;std::size_t cursor=0;Trace& trace;
    Input(RetailMovieRequest r,std::vector<MovieReadResult> e,Trace& t):requested(std::move(r)),events(std::move(e)),trace(t){}
    const RetailMovieRequest& request()const noexcept override{return requested;}
    MovieReadResult next(MovieControlFailure& f)override{f={};trace.calls.push_back("read");if(cursor==events.size())return {MovieReadStatus::end,{}};return events[cursor++];}
};
struct Policies final:MoviePlaybackPolicies {
    Trace& trace;bool is_ready=false,is_eligible=true,allow_end=false,stop_error=false,pause_error=false,wrong_cardinality=false;int transforms=0,stops=0,eligibility_calls=0,video_submissions=0,end_checks=0;std::vector<MoviePlaybackAudioAcceptance> accepted;
    explicit Policies(Trace& t):trace(t){}
    bool begin(const RetailMovieRequest&,std::int32_t,MovieControlFailure& f)override{f={};trace.calls.push_back("begin");return true;}
    bool pump_metadata(MovieControlFailure& f)override{f={};trace.calls.push_back("metadata");return true;}
    bool pause(bool p,MovieControlFailure& f)override{f={};trace.calls.push_back(p?"policy_pause":"policy_resume");return pause_error?error(f,"test policy pause"):true;}
    bool ready(const MoviePlaybackSnapshot&,bool& result,MovieControlFailure& f)override{f={};result=is_ready;return true;}
    bool eligible(const MovieVideoFrame&,const MoviePlaybackSnapshot&,bool& result,MovieControlFailure& f)override{f={};++eligibility_calls;result=is_eligible;return true;}
    bool video_submitted(const MovieGpuMetadata&,MovieControlFailure& f)override{f={};++video_submissions;return true;}
    bool prepare_audio(const MovieAudioFrame& a,std::int32_t,std::vector<float>& pcm,MovieControlFailure& f)override{
        f={};++transforms;check(a.channels.size()==2,"test policy explicit two source planes");pcm.clear();for(std::size_t i=0;i<a.channels[0].size();++i)for(const auto& plane:a.channels)pcm.push_back(static_cast<float>(plane[i])/32768.0f);if(wrong_cardinality)pcm.resize(pcm.size()+2);return true;
    }
    bool audio_accepted(const MoviePlaybackAudioAcceptance& a,MovieControlFailure& f)override{f={};accepted.push_back(a);return true;}
    bool set_gain(std::int32_t,MovieControlFailure& f)override{f={};return true;}
    bool end_admitted(const MoviePlaybackSnapshot&,bool& result,MovieControlFailure& f)override{f={};++end_checks;result=allow_end;return true;}
    bool stop(MovieControlFailure& f)override{++stops;trace.calls.push_back("policy_stop");f={};return stop_error?error(f,"test policy stop"):true;}
};
struct Audio final:MoviePlaybackAudioOutput {
    Trace& trace;bool race_full=false,bad_receipt=false,stop_error=false,started=false;int queues=0,resets=0;std::uint64_t submitted=0,epoch=0;std::uint32_t capacity=2,padding=0;std::vector<float> samples;std::vector<sound::AudioSubmission> receipts;
    explicit Audio(Trace& t):trace(t){}
    std::optional<sound::AudioStreamFormat> format(MovieControlFailure& f)override{f={};return sound::AudioStreamFormat{48000,2,3};}
    std::optional<sound::AudioQueueStatus> queue_status(MovieControlFailure& f)override{f={};return sound::AudioQueueStatus{capacity,padding,capacity-padding,submitted,epoch,started};}
    std::optional<sound::AudioClockReading> clock(MovieControlFailure& f)override{f={};return sound::AudioClockReading{0,123456,0,epoch,true};}
    MovieAudioSubmitResult queue(std::span<const float> pcm,MovieControlFailure& f)override{
        f={};++queues;if(race_full){race_full=false;return {MovieAudioSubmitStatus::full,{}};}
        check(pcm.size()%2==0&&pcm.size()/2<=capacity-padding,"client chunk does not exceed writable frames");const auto count=pcm.size()/2;
        sound::AudioSubmission r{submitted,static_cast<std::uint64_t>(count),epoch};samples.insert(samples.end(),pcm.begin(),pcm.end());submitted+=count;padding+=static_cast<std::uint32_t>(count);receipts.push_back(r);if(bad_receipt)++r.first_client_frame;
        return {MovieAudioSubmitStatus::accepted,r};
    }
    bool pause(bool p,MovieControlFailure& f)override{f={};trace.calls.push_back(p?"audio_pause":"audio_resume");started=!p&&padding>0;return true;}
    void advance(std::uint32_t frames){check(started,"explicit retirement requires playing");check(frames<=padding,"advance only queued frames");padding-=frames;}
    bool stop_and_reset(MovieControlFailure& f)override{++resets;trace.calls.push_back("audio_stop");f={};if(stop_error)return error(f,"test audio stop");started=false;padding=0;submitted=0;++epoch;return true;}
};
struct Frame final:MovieGpuFrame {
    MovieGpuMetadata info;std::uint32_t w,h;std::uint64_t id;
    Frame(const MovieRgbaFrame& r,std::uint64_t i):w(r.width),h(r.height),id(i){info.conversion_role=r.conversion_role;info.codec_timing=r.codec_timing;}
    std::uint64_t identity()const noexcept override{return id;}std::uint32_t width()const noexcept override{return w;}std::uint32_t height()const noexcept override{return h;}const MovieGpuMetadata& metadata()const noexcept override{return info;}
};
struct Fence final:MovieGpuFence {MovieFenceState state=MovieFenceState::pending;explicit Fence(NativeMoviePresentPlan p):MovieGpuFence(std::move(p)){} };
struct GPU final:NativeMovieGpuBackend {
    int uploads=0,presents=0,retired=0;bool present_error=false;std::vector<std::uint8_t> uploaded;std::vector<std::weak_ptr<Fence>> issued;
    MovieGpuUpload upload(const MovieRgbaFrame& f)override{check(validate_movie_upload(f)==MovieGpuError::none,"real table-converted RGBA validates");++uploads;uploaded=f.rgba;return {MovieGpuError::none,std::make_shared<Frame>(f,uploads)};}
    MovieGpuSubmission present(const NativeMoviePresentPlan& p,const MovieHostLayout& l)override{check(validate_movie_present(p)==MovieGpuError::none&&movie_destination(p,l).has_value(),"explicit layout/filter present plan");++presents;auto fence=std::make_shared<Fence>(p);issued.push_back(fence);return {present_error?MovieGpuError::present_failed:MovieGpuError::none,fence};}
    MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>& f)override{auto own=std::dynamic_pointer_cast<Fence>(f);check(bool(own),"owned fake fence");return {own->state,own->state==MovieFenceState::failed?MovieGpuError::fence_failed:MovieGpuError::none};}
    void retire(std::shared_ptr<MovieGpuFence> f)noexcept override{if(f)++retired;}
    void set_fences(MovieFenceState state){for(auto& w:issued)if(auto f=w.lock())f->state=state;}
};
MovieReadResult video(){MovieVideoFrame f;f.width=2;f.height=2;f.planes[0]={16,128,235,42};f.planes[1]={64};f.planes[2]={192};f.sample_aspect_numerator=1;f.sample_aspect_denominator=1;return {MovieReadStatus::frame,std::move(f)};}
MovieReadResult pcm(std::size_t count=5,std::uint32_t rate=48000){MovieAudioFrame f;f.sample_rate=rate;f.channel_layout.channel_count=2;f.channels.resize(2);for(std::size_t i=0;i<count;++i){f.channels[0].push_back(static_cast<std::int16_t>(100+i));f.channels[1].push_back(static_cast<std::int16_t>(-100-int(i)));}return {MovieReadStatus::frame,std::move(f)};}
MoviePlaybackConfiguration config(bool audio){MoviePlaybackConfiguration c;c.audio=audio?MoviePlaybackAudioPresence::required:MoviePlaybackAudioPresence::absent;if(audio){c.client_sample_rate=48000;c.client_channels=2;c.client_channel_mask=3;}c.layout={1,640,448,MovieAspectMode::explicit_display_aspect,4,3,MovieWindowPolicy::centered_fit};c.filter=MovieFilter::nearest;c.queued_byte_budget=1024*1024;c.initial_base_volume=1.0f;return c;}
struct Rig {
    Trace trace;GPU gpu;Policies* policy=nullptr;Audio* audio=nullptr;Input* input=nullptr;std::unique_ptr<NativeMovieStreamPlayback> playback;
    Rig(const NativeMovieColorTable& color,std::vector<MovieReadResult> events,bool with_audio){MovieCatalogFailure cf;auto request=RetailMovieCatalog{}.resolve(0,MovieViewSelection::normal,0,cf);check(bool(request),"real semantic request");auto in=std::make_unique<Input>(*request,std::move(events),trace);input=in.get();auto policies=std::make_unique<Policies>(trace);policy=policies.get();std::unique_ptr<Audio> out;if(with_audio){out=std::make_unique<Audio>(trace);audio=out.get();}MovieControlFailure failure;playback=NativeMovieStreamPlayback::create(std::move(in),std::move(policies),std::move(out),gpu,color,config(with_audio),failure);check(bool(playback),"composition create");}
    void start(){MovieControlFailure f;check(playback->pause(true,f)&&playback->start(input->requested.logical_key,f),"pause before single-use start");}
    void resume(){MovieControlFailure f;check(playback->pause(false,f),"explicit fixture resume");}
    void pump(){MovieControlFailure f;check(playback->pump_streams(f),"stream pump");}
    MoviePlaybackSnapshot snapshot(){MoviePlaybackSnapshot s;MovieControlFailure f;check(playback->snapshot(s,f),"snapshot");return s;}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"usage: test_native_movie_playback original_MOVIER.ovl");std::ifstream file(std::filesystem::u8path(argv[1]),std::ios::binary);check(bool(file),"original table source");std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});check(bytes.size()>=0x77dd8+6144,"retail table extent");MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"retail table identity");
    {Rig r(*color,{pcm(2),video()},true);NativeMovieControlContext context(1.0f);NativeMovieController controller(*r.playback,context);MovieControlFailure f;check(controller.start(r.input->requested.logical_key,4,f),"real controller start");check(r.snapshot().paused&&!r.audio->started,"controller start remains paused");auto first=controller.poll();check(first.progress==MovieControlProgress::ongoing&&controller.state()==MovieControlState::preparing,"preparing prefill progresses while paused");check(r.audio->submitted>0&&!r.audio->started,"paused accepts PCM without starting endpoint");auto read=std::find(r.trace.calls.begin(),r.trace.calls.end(),"read");check(read!=r.trace.calls.end()&&std::find(r.trace.calls.begin(),read,"policy_pause")!=read&&std::find(r.trace.calls.begin(),read,"audio_pause")!=read,"both pause requests precede first source read");r.policy->is_ready=true;check(controller.poll().progress==MovieControlProgress::ongoing&&controller.state()==MovieControlState::playing&&r.audio->started,"explicit ready resumes after prefill");check(r.gpu.uploads==1&&r.policy->video_submissions==1,"ready video converted/uploaded and policy observes submission");auto expected=color->pixel(16,64,192);check(std::equal(expected.begin(),expected.end(),r.gpu.uploaded.begin()),"actual color-table conversion used");check(controller.stop(f),"controller cleanup");}
    {Rig r(*color,{pcm()},true);r.audio->race_full=true;r.start();r.pump();check(r.policy->transforms==1,"typed full race prepares packet once");r.pump();check(r.audio->padding==2&&r.snapshot().source_retirement&&r.snapshot().source_retirement->retired==0,"paused prefill never retires");r.resume();r.audio->advance(2);check(r.snapshot().source_retirement->retired==2,"explicit partial source retirement2");r.pump();r.audio->advance(2);check(r.snapshot().source_retirement->retired==4,"explicit partial source retirement4");r.pump();check(r.policy->transforms==1&&r.audio->samples.size()==10,"retained transform submitted exactly once");check(r.audio->receipts.size()==3,"five frames split2/2/1");for(std::size_t i=0;i<3;++i){const auto& a=r.playback->audio_acceptances()[i];check(a.submission.first_client_frame==i*2&&a.submission.client_frames==(i==2?1u:2u)&&a.packet_client_offset==i*2&&a.source_first_frame==0&&a.source_frames==5,"exact client receipt/source packet ranges");}MovieControlFailure f;check(r.playback->stop(f),"audio stop");auto stopped=r.snapshot();check(stopped.cancelled_source_retirement&&stopped.cancelled_source_retirement->retired==4&&stopped.cancelled_source_retirement->pending==1&&stopped.cancelled_source_retirement->cancelled,"reset cancels pending at observed retirement");check(stopped.source_retirement&&stopped.source_retirement->epoch==1&&stopped.source_retirement->submitted==0&&stopped.source_retirement->retired==0&&stopped.audio_queue->reset_epoch==1,"poststop new epoch diagnostics");}
    {Rig r(*color,{pcm()},true);r.audio->bad_receipt=true;r.start();MovieControlFailure f;check(!r.playback->pump_streams(f),"invalid accepted receipt terminal");const auto count=r.audio->queues;check(!r.playback->pump_streams(f)&&r.audio->queues==count,"invalid receipt never retries accepted PCM");check(r.playback->take_error(f)&&f.kind!=MovieControlError::none,"terminal failure retained");r.playback->stop(f);}
    {Rig r(*color,{video()},false);r.start();r.pump();r.resume();r.policy->is_eligible=false;MovieControlFailure f;check(r.playback->try_present(f)==MoviePresentation::no_frame&&r.playback->try_present(f)==MoviePresentation::no_frame&&r.gpu.uploads==0,"eligibility wait retains source without redundant uploads");r.policy->is_eligible=true;check(r.playback->try_present(f)==MoviePresentation::presented&&r.gpu.uploads==1,"one admitted upload");r.pump();check(r.gpu.retired==0&&r.snapshot().outstanding_fences==1,"pending fence retains ownership");r.gpu.set_fences(MovieFenceState::complete);r.pump();check(r.gpu.retired==1&&r.snapshot().outstanding_fences==0,"completed fence retired once");MoviePlaybackStatus status;r.policy->is_ready=true;check(r.playback->status(status,f)&&status!=MoviePlaybackStatus::ended,"EOF alone not ended");r.policy->allow_end=true;check(r.playback->status(status,f)&&status==MoviePlaybackStatus::ended,"only explicit completion policy admits ended");check(r.playback->stop(f)&&r.gpu.retired==1,"stop no duplicate retirement");}
    for(bool failed_submit:{false,true}){Rig r(*color,{video()},false);r.start();r.pump();r.resume();r.gpu.present_error=failed_submit;MovieControlFailure f;auto presented=r.playback->try_present(f);if(failed_submit)check(presented==MoviePresentation::error,"failed submission named");else{check(presented==MoviePresentation::presented,"failed fence setup");r.gpu.set_fences(MovieFenceState::failed);check(!r.playback->pump_streams(f),"failed completion terminal");}r.playback->stop(f);check(r.gpu.retired==1,"failed submission/fence retired once");}
    {Rig r(*color,{pcm(2),video()},true);r.start();r.pump();r.resume();MovieControlFailure f;check(r.playback->try_present(f)==MoviePresentation::presented,"cleanup fence");r.audio->stop_error=true;r.policy->stop_error=true;check(!r.playback->stop(f)&&std::string(f.operation)=="test audio stop","first cleanup failure preserved");check(r.audio->resets==1&&r.policy->stops==1&&r.gpu.retired==1,"all independent cleanup attempted");}
    {Rig r(*color,{video()},true);r.start();MovieControlFailure f;check(!r.playback->pump_streams(f)&&std::string(f.operation)=="movie playback required source audio missing at EOF","required source audio cannot silently disappear at EOF");check(!r.playback->pump_streams(f),"missing audio remains terminal");r.playback->stop(f);}
    {Rig r(*color,{},true);r.policy->pause_error=true;MovieControlFailure f;check(!r.playback->pause(true,f)&&std::string(f.operation)=="test policy pause","prestart pause policy failure");r.playback->stop(f);check(r.audio->resets==1&&r.policy->stops==1,"prestart failure cleans both services once");r.playback->stop(f);check(r.audio->resets==1&&r.policy->stops==1,"prestart cleanup is not duplicated");}
    {Rig r(*color,{pcm(1),pcm(1),video(),pcm(1)},true);r.start();r.pump();check(r.input->cursor>=3&&r.audio->submitted==2&&r.audio->padding==2,"one pump drains multiple events to real backpressure with pending video");check(r.snapshot().source_retirement->retired==0,"multi-event paused retirement zero");MovieControlFailure f;r.playback->stop(f);}
    {Rig r(*color,{pcm(2,44100)},true);r.start();MovieControlFailure f;check(!r.playback->pump_streams(f)&&r.audio->queues==0,"unequal source rate rejected before queue");r.playback->stop(f);}
    {Rig r(*color,{pcm(2)},true);r.policy->wrong_cardinality=true;r.start();MovieControlFailure f;check(!r.playback->pump_streams(f)&&r.audio->queues==0,"changed transform cardinality rejected before queue");r.playback->stop(f);}
    {Rig r(*color,{pcm(2),video()},true);r.start();r.pump();r.resume();r.policy->is_ready=true;r.policy->allow_end=true;MovieControlFailure f;check(r.playback->try_present(f)==MoviePresentation::presented,"drain gate GPU submission");r.gpu.set_fences(MovieFenceState::complete);r.pump();check(r.snapshot().decoder_eof&&r.snapshot().queued_audio==0&&r.snapshot().queued_video==0&&r.snapshot().outstanding_fences==0,"drain gate reaches empty decoded queues and completed GPU");MoviePlaybackStatus status;check(r.playback->status(status,f)&&status!=MoviePlaybackStatus::ended&&r.policy->end_checks==0,"client padding inherently blocks even accepting end policy");check(r.audio->padding==2&&r.snapshot().source_retirement->pending==2,"accepted is not retired");r.audio->advance(2);check(r.playback->status(status,f)&&status==MoviePlaybackStatus::ended&&r.policy->end_checks==1,"explicit client retirement permits policy completion");check(r.playback->stop(f),"drain gate cleanup");}
    {Rig r(*color,{},false);r.start();MovieControlFailure foreign;bool success=true;std::thread thread([&]{success=r.playback->pump_streams(foreign);});thread.join();check(!success&&foreign.kind==MovieControlError::wrong_thread,"wrong thread rejected");r.pump();MovieControlFailure f;check(!r.playback->take_error(f),"foreign rejection does not poison owner");r.playback->stop(f);}
    {Rig r(*color,{},false);MovieControlFailure f;check(r.playback->pause(true,f)&&!r.playback->start("wrong-key",f),"mismatched exact key rejects");}
    // Construction guards do not call real endpoints.
    {Trace trace;GPU gpu;MovieCatalogFailure cf;auto request=RetailMovieCatalog{}.resolve(0,MovieViewSelection::normal,0,cf);check(bool(request),"guard request");MovieControlFailure f;auto in=[&]{return std::make_unique<Input>(*request,std::vector<MovieReadResult>{},trace);};auto policy=[&]{return std::make_unique<Policies>(trace);};check(!NativeMovieStreamPlayback::create(in(),{}, {},gpu,*color,config(false),f),"missing policies reject");auto c=config(false);c.audio=MoviePlaybackAudioPresence::unspecified;check(!NativeMovieStreamPlayback::create(in(),policy(),{},gpu,*color,c,f),"unspecified audio presence rejects");check(!NativeMovieStreamPlayback::create(in(),policy(),{},gpu,*color,config(true),f),"required audio missing rejects");c=config(true);c.client_channel_mask=4;check(!NativeMovieStreamPlayback::create(in(),policy(),std::make_unique<Audio>(trace),gpu,*color,c,f),"actual audio format mask mismatch rejects");c=config(false);c.initial_base_volume.reset();check(!NativeMovieStreamPlayback::create(in(),policy(),{},gpu,*color,c,f),"missing explicit base volume rejects");}
    std::puts("PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
