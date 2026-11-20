#include "mka/movie_service_acquisition.hpp"
#include <cstdio>
#include <fstream>
#include <set>
#include <stdexcept>
using namespace mka::video;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Frame final:MovieGpuFrame {
    MovieGpuMetadata info;std::uint32_t w,h;std::uint64_t id;
    Frame(const MovieRgbaFrame& r,std::uint64_t n):w(r.width),h(r.height),id(n){
        info.conversion_role=r.conversion_role;info.codec_timing=r.codec_timing;
        info.source_pts=r.source_pts;info.timestamp=r.timestamp;
    }
    std::uint64_t identity()const noexcept override{return id;}
    std::uint32_t width()const noexcept override{return w;}
    std::uint32_t height()const noexcept override{return h;}
    const MovieGpuMetadata& metadata()const noexcept override{return info;}
};
struct Fence final:MovieGpuFence {explicit Fence(NativeMoviePresentPlan p):MovieGpuFence(std::move(p)){} };
// Deterministic native output ports, not a physical context, renderer or screen.
struct GPU final:NativeMovieGpuBackend {
    std::uint64_t uploads=0,presents=0,retired=0;bool reject_upload=false;
    std::set<std::uint64_t> selected;
    MovieGpuUpload upload(const MovieRgbaFrame& r)override{
        if(reject_upload)return {MovieGpuError::upload_failed,{}};
        check(validate_movie_upload(r)==MovieGpuError::none,"actual prepared native frame is valid");
        return {MovieGpuError::none,std::make_shared<Frame>(r,++uploads)};
    }
    MovieGpuSubmission present(const NativeMoviePresentPlan& p,const MovieHostLayout& l)override{
        check(validate_movie_present(p)==MovieGpuError::none&&movie_destination(p,l).has_value(),"valid native submission contract");
        check(p.frame->metadata().codec_timing.picture_source.has_value(),"selected source survives native preparation");
        check(selected.insert(p.frame->metadata().codec_timing.picture_source->picture_es_byte).second,"exact selected source submitted once");
        ++presents;return {MovieGpuError::none,std::make_shared<Fence>(p)};
    }
    MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>& f)override{check(bool(f),"owned submitted fence");return {MovieFenceState::complete,MovieGpuError::none};}
    void retire(std::shared_ptr<MovieGpuFence> f)noexcept override{if(f)++retired;}
};
MovieServicePolicyConfiguration profile(){
    MovieServiceClockFailure f;auto clock=movie_service_source_config(MovieServiceSourceMode::mode0,f);check(bool(clock),"source mode0 config");
    return {MoviePlaybackAudioPresence::absent,{},
        {clock->frequency,clock->step,0,0,MovieFrameEligibilityMode::direct},*clock,
        MovieClockFilterConfig::source_defaults(),MovieNativeTargetRoute::published_position,
        MovieNativeCompletion::video,true,false,false,0,0,0,0,
        MovieOutputPositionProfile::successful_frame_no_repeat_no_skip};
}
}
int main(int argc,char** argv){try{
    check(argc==5,"usage: test_native_movie_service_backend DLLdir asset_root relative_video_only_file MOVIER.ovl");
    constexpr const char* key="KRYPT/V_RAIDON.SFD";
    MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{key,argv[3]}},af);check(bool(store),"exact native source association");
    std::ifstream original(std::filesystem::u8path(argv[4]),std::ios::binary);check(bool(original),"existing source color table");
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)),{});check(bytes.size()>=0x77dd8+6144,"table byte extent");
    MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"verified native color conversion table");
    for(bool reject_upload:{false,true}){
        auto gpu=std::make_shared<GPU>();gpu->reject_upload=reject_upload;
        MoviePlaybackConfiguration playback;playback.audio=MoviePlaybackAudioPresence::unspecified;
        playback.layout={1,640,448,MovieAspectMode::explicit_display_aspect,4,3,MovieWindowPolicy::centered_fit};
        playback.filter=MovieFilter::nearest;playback.queued_byte_budget=8*1024*1024;
        MovieServiceAcquisitionFailure why;auto acquisition=NativeMovieServiceAcquisition::create(*store,
            std::filesystem::u8path(argv[1]),gpu,*color,{{key,playback,{}}},why);check(bool(acquisition),"actual native acquisition factory");
        auto factory=acquisition->session_factory(why);check(bool(factory),"owned source factory");
        std::int64_t ticks=0;unsigned callbacks=0;NativeMovieService* live=nullptr;bool guarded=false;
        MovieServiceFailure f;
        auto service=NativeMovieService::create_with_native_clock(1.0f,
            [](const RetailMovieRequest&,std::shared_ptr<const MovieSourceObservationIdentity>,const std::optional<mka::sound::AudioStreamFormat>& format,MovieSessionFailure& f)->std::optional<MovieServicePolicyConfiguration>{f={};check(!format,"video-only never acquires endpoint");return profile();},
            std::move(factory),[&](std::string_view source,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& hf){
                return NativeMovieHostClock::create_with_reader(source,id,1000,[&]()->std::optional<std::int64_t>{
                    ++callbacks;
                    if(live){MovieServiceFailure nested;if(!live->snapshot(nested)&&nested.kind==MovieServiceError::invalid_state)guarded=true;}
                    return ticks;
                },hf);
            },f);check(bool(service),"automatic native service with deterministic clock port");live=service.get();
        if(!service->start(12,MovieViewSelection::normal,0,0,f)){
            std::fprintf(stderr,"start failure %s / %s\n",f.operation,f.control.operation);check(false,"automatic backend starts after actual session");
        }
        auto identity=service->request_identity(f);check(bool(identity),"real service source epoch");
        check(!service->parse_header_batch(*identity,f)&&f.kind==MovieServiceError::invalid_state,"automatic backend rejects external parser event mutation");
        bool complete=false;MovieControlResult result;
        std::uint64_t max_processed=0,max_published=0;bool saw_prepared=false,saw_metadata=false;
        // Deterministic elapsed stepping, no sleep or physical endpoints.
        for(unsigned poll=0;poll<2000&&!complete;++poll){
            ticks+=100;
            result=service->poll(f);
            if(result.progress==MovieControlProgress::error){
                if(!reject_upload)std::fprintf(stderr,"backend failure poll%u %s / %s code%lld\n",poll,f.operation,result.failure.operation,static_cast<long long>(result.failure.native_code));
                check(reject_upload,"normal automatic backend must not fail");complete=true;break;
            }
            auto state=service->snapshot(f);check(bool(state),"actual automatic service observation");
            if(state->backend){max_processed=std::max(max_processed,state->backend->processed);max_published=std::max(max_published,state->backend->published);}
            if(state->context){saw_prepared|=state->context->video_started;saw_metadata|=state->context->latest_prepared_descriptor.has_value();}
            complete=result.progress!=MovieControlProgress::ongoing;
        }
        check(complete&&guarded&&callbacks,"finite native backend progress and reentry guard");
        if(reject_upload){check(result.progress==MovieControlProgress::error&&!gpu->presents&&!gpu->uploads,"actual upload failure never acknowledges native preparation/presentation");}
        else {
            check(result.progress==MovieControlProgress::complete&&result.completion==MovieControlCompletion::backend_ended,"source work and output drain complete naturally");
            check(max_processed==120&&max_published==120&&gpu->presents==120&&gpu->uploads==120&&gpu->retired==120&&gpu->selected.size()==120,"all120 actual selected sources prepared/submitted/retired once through Service");
            check(saw_prepared&&saw_metadata,"actual native preparation and rewrite reach owned Context");
            std::printf("{\"processed\":%llu,\"published\":%llu,\"submitted_ports\":%llu,\"retired_ports\":%llu}\n",(unsigned long long)max_processed,(unsigned long long)max_published,(unsigned long long)gpu->presents,(unsigned long long)gpu->retired);
        }
        live=nullptr;check(service->close(f),"separate idempotent resource cancellation");
    }
    check(std::puts("PASS automatic native movie service backend work/preparation/drain through deterministic ports; no physical playback or content comparison")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native service backend %s\n",e.what());return 1;}}
