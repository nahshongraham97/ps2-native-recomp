#include "mka/movie_playback_session.hpp"
#include <cstdio>
#include <fstream>
#include <stdexcept>
using namespace mka::video;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct GPU final:NativeMovieGpuBackend {
 MovieGpuUpload upload(const MovieRgbaFrame&)override{throw std::runtime_error("no acquisition GPU upload");}
 MovieGpuSubmission present(const NativeMoviePresentPlan&,const MovieHostLayout&)override{throw std::runtime_error("no acquisition presentation");}
 MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>&)override{return {MovieFenceState::complete,MovieGpuError::none};}
 void retire(std::shared_ptr<MovieGpuFence>)noexcept override{}
};
struct Trace {unsigned policies=0,joins=0,stops=0;};
struct Policies final:MoviePlaybackPolicies {
 Trace& trace;std::shared_ptr<const MovieSourceObservationIdentity> source;
 Policies(Trace& t,std::shared_ptr<const MovieSourceObservationIdentity> id):trace(t),source(std::move(id)){}
 bool unused(MovieControlFailure& f){f={MovieControlError::invalid_state,"unexpected source acquisition policy operation",0};return false;}
 bool validate_observation_source(std::shared_ptr<const MovieSourceObservationIdentity> id,MovieControlFailure& f)override{f={};check(id==source,"same opened source identity retained through assembly");++trace.joins;return true;}
 bool validate_audio_format(const std::optional<mka::sound::AudioStreamFormat>& actual,MovieControlFailure& f)override{f={};check(!actual,"actual video-only format absent");return true;}
 bool observe_source(const MovieSourceObservationBatch&,MovieControlFailure& f)override{return unused(f);}
 bool begin(const RetailMovieRequest&,std::int32_t,MovieControlFailure& f)override{return unused(f);}
 bool pump_metadata(MovieControlFailure& f)override{return unused(f);}
 bool pause(bool,MovieControlFailure& f)override{return unused(f);}
 bool ready(const MoviePlaybackSnapshot&,bool&,MovieControlFailure& f)override{return unused(f);}
 bool eligible(const MovieVideoFrame&,const MoviePlaybackSnapshot&,bool&,MovieControlFailure& f)override{return unused(f);}
 bool video_submitted(const MovieGpuMetadata&,MovieControlFailure& f)override{return unused(f);}
 bool prepare_audio(const MovieAudioFrame&,std::int32_t,std::vector<float>&,MovieControlFailure& f)override{return unused(f);}
 bool audio_accepted(const MoviePlaybackAudioAcceptance&,MovieControlFailure& f)override{return unused(f);}
 bool set_gain(std::int32_t,MovieControlFailure& f)override{return unused(f);}
 bool end_admitted(const MoviePlaybackSnapshot&,bool&,MovieControlFailure& f)override{return unused(f);}
 bool stop(MovieControlFailure& f)override{f={};++trace.stops;return true;}
};
MoviePlaybackConfiguration config(){MoviePlaybackConfiguration c;c.layout={1,640,448,MovieAspectMode::explicit_display_aspect,4,3,MovieWindowPolicy::centered_fit};c.filter=MovieFilter::nearest;c.queued_byte_budget=1024*1024;return c;}
}
int main(int argc,char** argv){try{
 check(argc==6,"usage: test_native_movie_session_native_audio DLLdir asset_root video_relative audio_relative original_MOVIER.ovl");
 std::ifstream original(std::filesystem::u8path(argv[5]),std::ios::binary);check(bool(original),"original read-only color source");std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)),{});check(bytes.size()>=0x77dd8+6144,"original color extent");MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"existing color table decoder");
 MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{"KRYPT/V_RAIDON.SFD",argv[3]},{"LOGON.SFD",argv[4]}},af);check(bool(store),"two explicit associated actual sources");
 MovieSourceFailure sf;
 {auto source=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[1]),12,MovieViewSelection::normal,0,sf);check(bool(source),"actual ID12 source");auto identity=source->observation_identity();auto a=source->audio_present(sf),b=source->audio_present(sf);check(a.has_value()&&!a.value()&&b==a&&source->observation_identity()==identity,"successful false audio presence repeats without consuming source or changing identity");NativeRetailMovieSource moved(std::move(*source));check(!source->audio_present(sf)&&sf.kind==MovieSourceError::invalid_owner,"moved source audio query is named invalid owner");auto c=moved.audio_present(sf);check(c.has_value()&&!c.value()&&moved.observation_identity()==identity,"moved actual owner retains presence and identity");}
 {auto source=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[1]),0,MovieViewSelection::normal,0,sf);check(bool(source),"actual LOGON source");auto a=source->audio_present(sf),b=source->audio_present(sf);check(a.has_value()&&a.value()&&b==a,"actual audio presence is true without requesting decoded frames");}
 auto gpu=std::make_shared<GPU>();NativeMovieControlContext control(1.0f);Trace trace;MovieSessionFailure failure;
 MovieSessionPolicyFactory policies=[&](const RetailMovieRequest& r,std::shared_ptr<const MovieSourceObservationIdentity> id,const std::optional<mka::sound::AudioStreamFormat>& actual,MovieSessionFailure& f)->std::unique_ptr<MoviePlaybackPolicies>{f={};++trace.policies;check(r.id==12&&r.logical_key=="KRYPT/V_RAIDON.SFD"&&id&&!actual,"actual video-only request/token/absence reaches assembly");return std::make_unique<Policies>(trace,std::move(id));};
 auto open=[&](std::int64_t id,MoviePlaybackConfiguration c,std::optional<mka::sound::AudioDeviceConfig> host){return NativeMoviePlaybackSession::open_with_native_audio(*store,std::filesystem::u8path(argv[1]),id,MovieViewSelection::normal,0,policies,std::move(host),gpu,*color,c,control,failure);};
 {auto session=open(12,config(),std::nullopt);check(session&&failure.kind==MovieSessionError::none&&trace.policies==1&&trace.joins==1,"unspecified presence derives video-only without requiring audio config");check(session->request(failure)->id==12&&session->close(failure),"actual video-only Session closes without polling/playback");}
 {mka::sound::AudioDeviceConfig unused;auto session=open(12,config(),unused);check(session&&failure.kind==MovieSessionError::none&&trace.policies==2,"unused invalid generic host audio config ignored on video-only; no endpoint opening");check(session->close(failure),"unused settings video-only cleanup");}
 const auto calls=trace.policies;
 {auto c=config();c.audio=MoviePlaybackAudioPresence::required;check(!open(12,c,std::nullopt)&&failure.kind==MovieSessionError::control&&std::string(failure.control.operation)=="movie session contradictory source audio presence"&&trace.policies==calls,"explicit required contradicts actual video-only before any device/policy");}
 {auto c=config();c.client_sample_rate=48000;check(!open(12,c,mka::sound::AudioDeviceConfig{})&&failure.kind==MovieSessionError::control&&std::string(failure.control.operation)=="movie session video-only client fields conflict"&&trace.policies==calls,"video-only cannot silently zero explicit playback client fields");}
 {auto c=config();c.audio=MoviePlaybackAudioPresence::absent;check(!open(0,c,std::nullopt)&&failure.kind==MovieSessionError::control&&std::string(failure.control.operation)=="movie session contradictory source audio presence"&&trace.policies==calls,"explicit absent contradicts actual audio before host/device/policy");}
 {check(!open(0,config(),std::nullopt)&&failure.kind==MovieSessionError::device&&failure.device.kind==mka::sound::AudioDeviceError::invalid_endpoint&&std::string(failure.operation)=="movie session actual audio requires explicit host configuration"&&trace.policies==calls,"actual audio with missing host config fails before NativeAudioDevice.open");}
 {check(!open(0,config(),mka::sound::AudioDeviceConfig{})&&failure.kind==MovieSessionError::device&&failure.device.kind==mka::sound::AudioDeviceError::invalid_endpoint&&std::string(failure.operation)=="movie session native audio configuration"&&trace.policies==calls,"actual audio invalid host config fails validation without endpoint open");}
 {mka::sound::AudioDeviceConfig host;host.endpoint_id_utf8="fixture-never-opened";host.sample_rate=48000;host.channels=2;host.channel_mask=3;host.buffer_duration_100ns=100000;host.conversion=mka::sound::AudioConversion::exact_shared;auto c=config();c.client_sample_rate=44100;check(!open(0,c,host)&&failure.kind==MovieSessionError::control&&std::string(failure.control.operation)=="movie session configured client format conflict"&&trace.policies==calls,"configured conflict rejects valid-shaped host before endpoint open");}
 {check(!open(-1,config(),mka::sound::AudioDeviceConfig{})&&failure.kind==MovieSessionError::source&&failure.source.kind==MovieSourceError::catalog&&trace.policies==calls,"source semantic error precedes unused host audio validation");}
 check(trace.stops==0,"unstarted acquisition cleanup does not fabricate policy start/stop lifecycle");
 check(std::puts("PASS source-first native Session audio presence and lazy acquisition; no endpoint activation or playback")>=0&&std::fflush(stdout)==0,"checked fixture output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL source-first native audio %s\n",e.what());return 1;}}
