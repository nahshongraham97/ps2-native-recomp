#include "mka/movie_native_context.hpp"
#include "mka/movie_admission_feed.hpp"
#include "mka/movie_input_acceptance.hpp"
#include <cstdio>
#include <stdexcept>
using namespace mka::video;
namespace {
void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
struct Consumers {
    std::shared_ptr<MovieEligibilityServiceBinding> service;
    NativeMoviePlaybackContext* context=nullptr;
    std::unique_ptr<NativeMovieKernelPolicies> kernel;
    std::unique_ptr<NativeMovieAdmissionFeed> feed;
    Consumers(const RetailMovieRequest& request,std::shared_ptr<const MovieSourceObservationIdentity> identity){
        MovieControlFailure f;MovieServiceClockFailure sf;auto clock=NativeMovieServiceClock::create({30000,1000},sf);check(bool(clock),"explicit fixture service clock");
        service=MovieEligibilityServiceBinding::create(request.logical_key,std::move(*clock),f);check(bool(service),"same actual source clock binding");
        auto owned=NativeMoviePlaybackContext::create({request,identity,service,MovieClockFilterConfig::source_defaults(),MovieNativeTargetRoute::published_position,MovieNativeCompletion::video,true,false,true,0,0,0,0},f);check(bool(owned),"concrete native context");context=owned.get();
        kernel=NativeMovieKernelPolicies::create(request,MoviePlaybackAudioPresence::absent,{},MovieFrameEligibilityConfig{30000,1000,0,0,MovieFrameEligibilityMode::direct},std::move(owned),service,f);check(bool(kernel),"actual concrete kernel");
        check(kernel->validate_observation_source(identity,f)&&kernel->validate_audio_format(std::nullopt,f)&&kernel->pause(true,f)&&kernel->begin(request,0,f),"actual source/format join and paused begin");
        MovieAdmissionFeedFailure ff;feed=NativeMovieAdmissionFeed::create(request.logical_key,identity,0,ff);check(bool(feed),"same source feed");
    }
    void reject(const MovieSourceObservationBatch& batch,MovieInputReceiptError expected){
        MovieControlFailure f;MovieAdmissionFeedFailure ff;auto before=context->snapshot(f);auto bytes=feed->snapshot(ff);const auto clock=service->service(f)->state();
        check(before&&bytes,"before rejection snapshots");
        check(!kernel->observe_source(batch,f)&&f.kind!=MovieControlError::none&&f.native_code==static_cast<std::int64_t>(expected),"kernel/context rejects invalid real receipt");
        auto after=context->snapshot(f);check(after&&after->observation_sequence==before->observation_sequence&&after->last_input_receipt==before->last_input_receipt&&after->admissions==before->admissions&&after->outputs_pending==before->outputs_pending&&after->cached_clock==before->cached_clock&&after->first_baseline==before->first_baseline&&after->owner_started==before->owner_started&&after->video_started==before->video_started&&service->service(f)->state()==clock,"rejected receipt leaves context/source/clock/admission state unchanged");
        check(!context->observe_source(batch,f)&&f.kind!=MovieControlError::none&&f.native_code==static_cast<std::int64_t>(expected),"direct context independently rejects exact receipt error after kernel rejection");
        auto direct=context->snapshot(f);check(direct&&direct->observation_sequence==before->observation_sequence&&direct->last_input_receipt==before->last_input_receipt&&direct->admissions==before->admissions&&direct->outputs_pending==before->outputs_pending&&direct->cached_clock==before->cached_clock&&direct->first_baseline==before->first_baseline&&direct->owner_started==before->owner_started&&direct->video_started==before->video_started&&service->service(f)->state()==clock,"direct context rejection independently preserves source/clock/admission/receipt state");
        check(!feed->push(batch,ff)&&ff.kind!=MovieAdmissionFeedError::none&&ff.input_receipt==expected,"feed rejects invalid real receipt");auto retained=feed->snapshot(ff);
        check(retained&&retained->observation_sequence==bytes->observation_sequence&&retained->last_input_receipt==bytes->last_input_receipt&&retained->es_begin==bytes->es_begin&&retained->es_end==bytes->es_end&&retained->buffered_bytes==bytes->buffered_bytes&&retained->failed==bytes->failed&&retained->program_end_observed==bytes->program_end_observed&&retained->current.gop_parse_entries==bytes->current.gop_parse_entries&&!retained->work_pending,"rejected receipt leaves feed bytes/receipt/parser state unchanged");
    }
};
std::shared_ptr<const MovieCodecInputReceipt> first_receipt(const MovieSourceObservationBatch& b){for(const auto& e:b.events)if(auto* p=std::get_if<MovieCodecInputAccepted>(&e.value))return p->receipt;return {};}
}
int main(int argc,char** argv){try{
    check(argc==4,"usage: test_native_movie_input_acceptance_consumers DLLdir asset_root video_relative");
    MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{"KRYPT/V_RAIDON.SFD",argv[3]}},af);check(bool(store),"one exact video-only source binding");
    MovieSourceFailure sf;auto source=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[1]),12,MovieViewSelection::normal,0,sf);check(bool(source),"actual ID12 source open");
    auto request=*source->request();auto identity=source->observation_identity();auto read=source->next(sf);check(read.status==MovieReadStatus::frame&&read.observations&&sf.kind==MovieSourceError::none,"one actual first-next source batch");auto batch=std::move(*read.observations);auto receipt=first_receipt(batch);check(receipt&&receipt->input_id()==1&&receipt->source_key()==request.logical_key&&receipt->source_identity()==identity,"authentic same-source first receipt");
    std::uint64_t end=0,serial=0;std::shared_ptr<const MovieCodecInputReceipt> previous;
    for(const auto& event:batch.events){check(event.sequence==++serial,"actual ordered observation sequence");if(auto* raw=std::get_if<MovieVideoEsObserved>(&event.value)){check(raw->es_begin==end&&raw->es_end-end==raw->bytes.size(),"actual contiguous source byte coverage");end=raw->es_end;}else if(auto* input=std::get_if<MovieCodecInputAccepted>(&event.value)){check(check_movie_input_receipt(input->receipt,batch.source_key,batch.identity,previous,end,false)==MovieInputReceiptError::none,"every actual receipt covered at its exact event order");previous=input->receipt;}}
    check(previous&&end>=previous->es_end(),"actual batch contains accepted input coverage");
    auto other=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[1]),12,MovieViewSelection::normal,0,sf);check(bool(other),"second genuine same-key source epoch");auto foreign_read=other->next(sf);check(foreign_read.observations.has_value(),"foreign epoch first-next metadata");auto foreign=first_receipt(*foreign_read.observations);check(foreign&&foreign->source_identity()!=identity,"genuine foreign receipt identity");other.reset();source.reset();store.reset();
    Consumers accepted(request,identity);MovieControlFailure f;MovieAdmissionFeedFailure ff;
    check(accepted.kernel->observe_source(batch,f)&&accepted.feed->push(batch,ff),"same actual batch accepted by concrete kernel/context/feed");auto state=accepted.context->snapshot(f);auto feed=accepted.feed->snapshot(ff);
    check(state&&feed&&state->last_input_receipt==previous&&feed->last_input_receipt==previous&&state->observation_sequence==serial&&feed->observation_sequence==serial&&feed->es_end==end&&!state->admissions&&!state->outputs_pending&&!state->owner_started&&!state->video_started&&!state->cached_clock&&!feed->current.picture&&!feed->work_pending,"accepted input is retained without work/admission/readiness/clock inference");
    MovieSourceObservationBatch replay{request.logical_key,identity,{{serial+1,MovieCodecInputAccepted{receipt}}}};accepted.reject(replay,MovieInputReceiptError::input_order);
    for(const auto& rejected:{std::shared_ptr<const MovieCodecInputReceipt>{},foreign}){
        auto bad=batch;for(auto& event:bad.events)if(auto* input=std::get_if<MovieCodecInputAccepted>(&event.value)){input->receipt=rejected;break;}Consumers fresh(request,identity);fresh.reject(bad,rejected?MovieInputReceiptError::foreign_source:MovieInputReceiptError::null_receipt);
    }
    check(receipt->es_end()>0,"nonempty authentic input extent");
    std::vector<std::uint8_t> prefix;for(const auto& e:batch.events)if(auto* raw=std::get_if<MovieVideoEsObserved>(&e.value)){prefix.insert(prefix.end(),raw->bytes.begin(),raw->bytes.end());if(prefix.size()>=receipt->es_end())break;}
    check(prefix.size()>=receipt->es_end(),"independent actual observed-byte prefix retained");prefix.resize(static_cast<std::size_t>(receipt->es_end()-1));
    MovieSourceObservationBatch insufficient{request.logical_key,identity,{{1,MovieVideoEsObserved{0,receipt->es_end()-1,std::move(prefix)}},{2,MovieCodecInputAccepted{receipt}}}};
    check(check_movie_input_receipt(receipt,request.logical_key,identity,{},receipt->es_end()-1,false)==MovieInputReceiptError::unobserved_range,"private authentic receipt fails deliberately insufficient observed range");Consumers short_range(request,identity);short_range.reject(insufficient,MovieInputReceiptError::unobserved_range);
    accepted.kernel.reset();accepted.feed.reset();check(state->last_input_receipt->source_identity()==identity&&feed->last_input_receipt->source_key()=="KRYPT/V_RAIDON.SFD"&&receipt->es_end()>receipt->es_begin(),"owned actual receipts survive source and consumer teardown");
    check(std::puts("PASS actual native input receipt consumers first-next/source ownership; no work admission or playback")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL receipt consumers %s\n",e.what());return 1;}}
