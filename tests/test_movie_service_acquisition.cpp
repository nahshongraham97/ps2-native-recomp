#include "mka/movie_service_acquisition.hpp"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <thread>
using namespace mka::video;
namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct GPU final:NativeMovieGpuBackend {
    unsigned& released;explicit GPU(unsigned& n):released(n){}~GPU()override{++released;}
    MovieGpuUpload upload(const MovieRgbaFrame&)override{throw std::runtime_error("no acquisition upload");}
    MovieGpuSubmission present(const NativeMoviePresentPlan&,const MovieHostLayout&)override{throw std::runtime_error("no acquisition presentation");}
    MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>&)override{return {MovieFenceState::complete,MovieGpuError::none};}
    void retire(std::shared_ptr<MovieGpuFence>)noexcept override{}
};
MoviePlaybackConfiguration playback(){MoviePlaybackConfiguration c;c.audio=MoviePlaybackAudioPresence::absent;c.layout={1,640,448,MovieAspectMode::explicit_display_aspect,4,3,MovieWindowPolicy::centered_fit};c.filter=MovieFilter::nearest;c.queued_byte_budget=1024*1024;return c;}
MovieServicePolicyConfiguration profile(){return {MoviePlaybackAudioPresence::absent,std::nullopt,
    MovieFrameEligibilityConfig{30000,1000,0,0,MovieFrameEligibilityMode::direct},{30000,1000},
    MovieClockFilterConfig::source_defaults(),MovieNativeTargetRoute::published_position,
    MovieNativeCompletion::video,true,false,true,0,0,0,0,MovieOutputPositionProfile::successful_frame_no_repeat_no_skip};}
}
// Deliberate component inputs; no game flags or default endpoint selection.
// The supplied single asset MUST be an independently admitted video-only file.
int main(int argc,char** argv){try{
    check(argc==6,"usage: test_native_movie_service_acquisition DLLdir asset_root relative_video_only_file retail_id original_MOVIER.ovl");
    std::size_t used=0;const auto id=std::stoll(argv[4],&used);check(used==std::string(argv[4]).size(),"complete retail ID");
    MovieCatalogFailure cf;auto request=RetailMovieCatalog{}.resolve(id,MovieViewSelection::normal,0,cf);check(request&&id==12&&request->logical_key=="KRYPT/V_RAIDON.SFD","explicit previously measured video-only retail ID12");
    std::ifstream original(std::filesystem::u8path(argv[5]),std::ios::binary);check(bool(original),"read original color table");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)),{});check(bytes.size()>=0x77dd8+6144,"color bytes extent");
    MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"existing color table decoder");
    MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{request->logical_key,argv[3]}},af);check(bool(store),"one exact associated native file");
    unsigned released=0;auto gpu=std::make_shared<GPU>(released);std::weak_ptr<GPU> weak=gpu;
    MovieServiceAcquisitionFailure failure;
    auto bad=playback();bad.audio=MoviePlaybackAudioPresence::required;
    check(!NativeMovieServiceAcquisition::create(*store,std::filesystem::u8path(argv[1]),gpu,*color,{{request->logical_key,bad,{}}},failure)&&failure.kind==MovieServiceAcquisitionError::invalid_setting,"required audio cannot silently become absent");
    check(!NativeMovieServiceAcquisition::create(*store,std::filesystem::u8path(argv[1]),gpu,*color,{{request->logical_key,playback(),{}},{request->logical_key,playback(),{}}},failure)&&failure.kind==MovieServiceAcquisitionError::invalid_setting,"duplicate exact key rejected");
    auto automatic=playback();automatic.audio=MoviePlaybackAudioPresence::unspecified;
    mka::sound::AudioDeviceConfig unused_host;unused_host.endpoint_id_utf8="deliberately-unusable-unused-video-only-endpoint";
    auto owner=NativeMovieServiceAcquisition::create(*store,std::filesystem::u8path(argv[1]),gpu,*color,{{request->logical_key,automatic,unused_host}},failure);check(bool(owner),"unspecified source audio with unused invalid generic host settings is retained passively");
    NativeMovieControlContext control(1.0f);unsigned called=0;MovieSessionPolicyFactory reject=[&](const RetailMovieRequest&,std::shared_ptr<const MovieSourceObservationIdentity>,const std::optional<mka::sound::AudioStreamFormat>& actual,MovieSessionFailure& f)->std::unique_ptr<MoviePlaybackPolicies>{++called;check(!actual,"video-only must not acquire endpoint");f.kind=MovieSessionError::control;f.operation="fixture deliberate policy rejection";return {};};
    auto forged=*request;forged.logical_extent.width++;
    check(!owner->acquire(forged,0,reject,control,failure)&&failure.kind==MovieServiceAcquisitionError::invalid_request&&called==0,"full request rejected before source/policy acquisition");
    MovieServiceAcquisitionFailure foreign;std::thread t([&]{owner->session_factory(foreign);});t.join();check(foreign.kind==MovieServiceAcquisitionError::wrong_thread,"foreign acquisition thread rejected");
    check(!owner->acquire(*request,0,reject,control,failure)&&failure.kind==MovieServiceAcquisitionError::session&&failure.session.kind==MovieSessionError::control&&called==1,"actual associated source reaches actual-format policy factory with nested failure retained");
    {
        const auto missing_libraries=std::filesystem::u8path(argv[1])/"missing-acquisition-fixture-libraries";
        check(!std::filesystem::exists(missing_libraries),"explicit missing-library fixture path must not exist");
        auto missing_owner=NativeMovieServiceAcquisition::create(*store,missing_libraries,gpu,*color,{{request->logical_key,automatic,unused_host}},failure);
        check(bool(missing_owner),"missing library retained as acquisition input, not fallback");
        auto missing_factory=missing_owner->session_factory(failure);const auto before_calls=called;MovieSessionFailure missing;
        check(!missing_factory(*request,0,reject,control,missing)&&missing.kind==MovieSessionError::source&&missing.source.kind==MovieSourceError::decoder&&missing.source.decoder_failure.kind==MovieDecodeError::library_load&&!missing.source.decoder_failure.operation.empty()&&called==before_calls,"actual native source missing-library error moves through acquisition callback without invoking policy");
    }
    auto factory=owner->session_factory(failure);check(bool(factory),"concrete retained acquisition callback");owner.reset();store.reset();gpu.reset();check(!weak.expired(),"factory retains native store/GPU owner after external teardown");
    unsigned profiles=0;MovieServiceFailure sf;auto service=NativeMovieService::create(1.0f,[&](const RetailMovieRequest& r,std::shared_ptr<const MovieSourceObservationIdentity> token,const std::optional<mka::sound::AudioStreamFormat>& actual,MovieSessionFailure& f)->std::optional<MovieServicePolicyConfiguration>{f={};++profiles;check(r.logical_key==request->logical_key&&token&&!actual,"actual source epoch and explicit absent client reach concrete service");return profile();},factory,sf);
    check(service&&service->start(id,MovieViewSelection::normal,0,4,sf),"production acquisition opens actual source and concrete native Session");
    auto active=service->snapshot(sf);check(active&&active->active&&active->request->logical_key==request->logical_key&&profiles==1,"same semantic request active without invented readiness");
    auto identity=service->request_identity(sf);check(bool(identity),"same actual request identity before preparation poll");
    auto empty=service->source_feed_snapshot(*identity,sf);check(empty&&empty->observation_sequence==0&&empty->buffered_bytes==0,"actual source feed initially empty before first poll");
    const auto polled=service->poll(sf);check(polled.progress==MovieControlProgress::ongoing&&sf.kind==MovieServiceError::none,"one actual preparation poll pumps native source without completion");
    auto received=service->source_feed_snapshot(*identity,sf);check(received&&received->current.source->source_key==request->logical_key&&received->current.source->identity==identity->source&&received->observation_sequence>0&&received->es_end>received->es_begin&&received->buffered_bytes>0,"same-epoch real native source observations reached service-owned feed");
    auto preparing=service->snapshot(sf);check(preparing&&preparing->active&&preparing->context&&!preparing->context->owner_started&&!preparing->context->video_started&&preparing->context->admissions==0,"source delivery invents neither readiness acknowledgments nor business admission");
    check(service->close(sf)&&!service->snapshot(sf)->active,"actual session resource cancellation");service.reset();factory={};check(weak.expired()&&released==1,"retained GPU released only after session/factory teardown");
    check(std::puts("PASS native service acquisition associated video-only Session; no endpoint activation or playback")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL movie acquisition %s\n",e.what());return 1;}}
