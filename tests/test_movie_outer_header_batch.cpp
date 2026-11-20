#include "mka/movie_native_context.hpp"
#include "mka/movie_decode.hpp"
#include <cstdio>
#include <stdexcept>
#include <type_traits>
using namespace mka::video;
namespace {
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
class RateProbe final:public MoviePlaybackPolicyContext {
public:
 std::unique_ptr<NativeMoviePlaybackContext> inner;bool alter=false;
 explicit RateProbe(std::unique_ptr<NativeMoviePlaybackContext> c):inner(std::move(c)){}
 bool bind_source(std::shared_ptr<const MovieSourceObservationIdentity> p,MovieControlFailure& f)override{return inner->bind_source(std::move(p),f);}
 bool begin(const RetailMovieRequest& r,std::int32_t g,MovieControlFailure& f)override{return inner->begin(r,g,f);}
 bool pump_metadata(MovieControlFailure& f)override{return inner->pump_metadata(f);}
 bool observe_source(const MovieSourceObservationBatch& b,MovieControlFailure& f)override{return inner->observe_source(b,f);}
 bool pause(bool p,MovieControlFailure& f)override{return inner->pause(p,f);}
 bool ready(const MoviePlaybackSnapshot& s,bool& o,MovieControlFailure& f)override{return inner->ready(s,o,f);}
 std::optional<MoviePolicyDecision> decision(const MovieVideoFrame& v,const MoviePlaybackSnapshot& s,MovieControlFailure& f)override{auto d=inner->decision(v,s,f);if(d&&alter)if(auto* o=std::get_if<MovieOrdinaryDecision>(&*d))o->rate_record.frame_rate_code=o->rate_record.frame_rate_code==1?2:1;return d;}
 bool video_submitted(const MovieGpuMetadata& m,MovieControlFailure& f)override{return inner->video_submitted(m,f);}
 bool audio_accepted(const MoviePlaybackAudioAcceptance& a,MovieControlFailure& f)override{return inner->audio_accepted(a,f);}
 bool end_admitted(const MoviePlaybackSnapshot& s,bool& o,MovieControlFailure& f)override{return inner->end_admitted(s,o,f);}
 bool stop(MovieControlFailure& f)override{return inner->stop(f);}
};
}
int main(int argc,char** argv){try{
 static_assert(!std::is_default_constructible_v<MovieOuterHeaderBatch>);
 static_assert(!std::is_copy_constructible_v<MovieOuterHeaderBatch>);
 check(argc==4,"usage: test_native_movie_outer_header_batch DLLdir asset_root video_relative");
 constexpr const char* key="KRYPT/V_RAIDON.SFD";
 MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{key,argv[3]}},af);check(bool(store),"one existing source association");auto asset=store->open(key,af);check(bool(asset),"retained actual file");MovieDecodeFailure df;auto decoder=NativeMovieDecoder::open(std::filesystem::u8path(argv[1]),std::move(*asset),df);check(bool(decoder),"actual native source owner");auto token=decoder->observation_identity();auto read=decoder->next(df);check(read.status==MovieReadStatus::frame&&read.observations&&std::holds_alternative<MovieVideoFrame>(read.frame),"one actual preparation read only");
 MovieAdmissionFeedFailure ff;auto feed=NativeMovieAdmissionFeed::create(key,token,0,ff);check(feed&&feed->push(*read.observations,ff),"same actual raw source enters native parser");auto parsed=feed->parse_outer_batch(ff);check(parsed&&parsed->progress==MovieOuterHeaderBatchProgress::first_slice&&parsed->batch,"one structured outer header operation reaches first slice");auto batch=parsed->batch;auto current=batch->current();auto view=feed->snapshot(ff);
 check(batch->initiating_prefix().prefix.raw_codeword==0x1b3&&batch->parsed_units()>1&&current->gop&&current->picture&&current->coding&&current->picture->origin.prefix.es_prefix_first_byte>batch->initiating_prefix().prefix.es_prefix_first_byte&&batch->stopping_prefix()&&view&&view->work_pending&&!view->outer_batch_pending&&view->es_begin==batch->parse_end(),"actual B3-origin batch owns later parsed GOP/picture/coding and exact checked stop");
 check(batch->offered_end()>=batch->parse_end()&&batch->timestamp_prefix()&&batch->timestamp_prefix()->prefix.es_prefix_first_byte>=batch->initiating_prefix().prefix.es_prefix_first_byte&&batch->timestamp_prefix()->prefix.es_prefix_first_byte+4<=batch->offered_end()&&(batch->timestamp_prefix()->prefix.classification&4),"original offered span retains first raw picture-prefix/PES separately from structured current and stop");
 for(const auto& event:read.observations->events)if(const auto* candidate=std::get_if<MovieVideoPrefixObserved>(&event.value))if(candidate->prefix.es_prefix_first_byte>=batch->initiating_prefix().prefix.es_prefix_first_byte&&(candidate->prefix.classification&4)){check(candidate->prefix==batch->timestamp_prefix()->prefix&&candidate->pes==batch->timestamp_prefix()->pes&&candidate->quarter_pts==batch->timestamp_prefix()->quarter_pts,"first actual raw picture-class prefix selected in original offer order");break;}
 MovieCatalogFailure cf;auto request=RetailMovieCatalog{}.resolve(12,MovieViewSelection::normal,0,cf);check(request&&request->logical_key==key,"actual retail catalog source identity");MovieControlFailure failure;MovieServiceClockFailure clock_failure;auto clock=NativeMovieServiceClock::create({30000,1000},clock_failure);check(bool(clock),"explicit component clock configuration");auto service=MovieEligibilityServiceBinding::create(key,std::move(*clock),failure);auto context=NativeMoviePlaybackContext::create({*request,token,service,MovieClockFilterConfig::source_defaults(),MovieNativeTargetRoute::descriptor,MovieNativeCompletion::video,true,false,true,0,0,0,0},failure);check(bool(context),"concrete source-owned context");auto proxy=std::make_unique<RateProbe>(std::move(context));auto* probe=proxy.get();auto policies=NativeMovieKernelPolicies::create(*request,MoviePlaybackAudioPresence::absent,{},MovieFrameEligibilityConfig{30000,1000,0,0,MovieFrameEligibilityMode::direct},std::move(proxy),service,failure);check(policies&&policies->validate_observation_source(token,failure)&&policies->validate_audio_format(std::nullopt,failure)&&policies->pause(true,failure)&&policies->begin(*request,0,failure)&&policies->observe_source(*read.observations,failure),"actual source observations join real policies/context");
 const auto origin=batch->initiating_prefix().prefix.es_prefix_first_byte;
 check(!probe->inner->admit_header({1,origin,current,MovieNativeDescriptorRoute::gop,false,true,true},failure)&&probe->inner->snapshot(failure)->admissions==0,"unit-origin API cannot claim later unbounded units without actual batch ownership");
 auto cloned=std::make_shared<const MovieAdmissionCurrent>(*current);
 check(!probe->inner->admit_header({1,origin,cloned,MovieNativeDescriptorRoute::gop,false,true,true,batch},failure)&&probe->inner->snapshot(failure)->admissions==0,"actual batch cannot authenticate a caller-replaced current snapshot");
 check(!probe->inner->admit_header({1,current->picture->origin.prefix.es_prefix_first_byte,current,MovieNativeDescriptorRoute::gop,false,true,true,batch},failure)&&probe->inner->snapshot(failure)->admissions==0,"initiating B3 must not be relabeled to later picture");
 check(probe->inner->admit_header({1,origin,current,MovieNativeDescriptorRoute::gop,false,true,true,batch},failure),"genuine completed batch admits later already-parsed units");auto admitted=probe->inner->snapshot(failure);check(admitted&&admitted->eligibility_rate&&admitted->eligibility_rate->parsed_batch==batch&&admitted->eligibility_rate->admitted_header_es_byte==origin,"latched parser rate retains exact initiating-batch ownership");
 const auto& frame=std::get<MovieVideoFrame>(read.frame);check(frame.codec_timing.picture_source&&probe->inner->publish_output({1,1,*frame.codec_timing.picture_source,0},failure)&&probe->inner->sample_service(true,failure),"explicit fixture output/timing publication for actual selected source");bool yes=false;check(policies->eligible(frame,{},yes,failure),"genuine batch rate reaches actual ordinary kernel");auto before=policies->last_route_result();probe->alter=true;check(!policies->eligible(frame,{},yes,failure)&&policies->last_route_result()==before,"altered in-range rate with genuine private batch fails before eligibility mutation");
 // Split the actual same source bytes at a real incomplete GOP. No fabricated
 // media/receipt or decoder fidelity comparison is used by this parser test.
 std::vector<std::uint8_t> bytes;std::vector<MovieVideoPrefixObserved> prefixes;
 for(const auto& event:read.observations->events){if(const auto* raw=std::get_if<MovieVideoEsObserved>(&event.value))bytes.insert(bytes.end(),raw->bytes.begin(),raw->bytes.end());else if(const auto* prefix=std::get_if<MovieVideoPrefixObserved>(&event.value))prefixes.push_back(*prefix);}
 const auto cut=current->gop->origin.prefix.es_prefix_first_byte+4;check(cut<bytes.size(),"actual incomplete GOP cut");std::uint64_t sequence=0;
 const auto chunk=[&](std::uint64_t begin,std::uint64_t end){MovieSourceObservationBatch b{key,token,{}};b.events.push_back({++sequence,MovieVideoEsObserved{begin,end,std::vector<std::uint8_t>(bytes.begin()+begin,bytes.begin()+end)}});for(const auto& prefix:prefixes)if(prefix.prefix.es_prefix_first_byte>=begin&&prefix.prefix.es_prefix_first_byte+4<=end)b.events.push_back({++sequence,prefix});return b;};
 auto partial=NativeMovieAdmissionFeed::create(key,token,0,ff);check(partial&&partial->push(chunk(0,cut),ff),"actual first partial source chunk");auto waiting=partial->parse_outer_batch(ff);auto pending=partial->snapshot(ff);check(waiting&&!waiting->batch&&waiting->progress==MovieOuterHeaderBatchProgress::need_more&&pending&&pending->outer_batch_pending,"partial batch retains initiating B3 without private completion");const auto cursor=pending->es_begin;
 check(!partial->skip(1,ff)&&ff.kind==MovieAdmissionFeedError::scope&&partial->snapshot(ff)->es_begin==cursor,"non-parser skip cannot bypass active batch provenance");check(!partial->parse_next(ff)&&ff.kind==MovieAdmissionFeedError::scope,"unit parser cannot replace active batch writer");check(!partial->begin_inner_parse(ff)&&ff.kind==MovieAdmissionFeedError::scope,"inner processing cannot replace active outer parser");
 check(partial->push(chunk(cut,bytes.size()),ff),"actual remaining source bytes append");auto complete=partial->parse_outer_batch(ff);check(complete&&complete->batch&&complete->batch->initiating_prefix().prefix.es_prefix_first_byte==origin&&complete->batch->parse_end()==batch->parse_end()&&!partial->snapshot(ff)->outer_batch_pending,"completed partial batch retains original initiating origin and exact endpoint");
 std::optional<MovieVideoPrefixObserved> original_first;
 for(const auto& prefix:prefixes)if((prefix.prefix.classification&4)&&prefix.prefix.es_prefix_first_byte+4<=cut){original_first=prefix;break;}
 check(complete->batch->offered_end()==cut&&bool(complete->batch->timestamp_prefix())==bool(original_first)&&(!original_first||(complete->batch->timestamp_prefix()->prefix==original_first->prefix&&complete->batch->timestamp_prefix()->pes==original_first->pes)),"later input does not replace first raw timestamp-query presence or provenance in original incomplete offer");
 // Delayed prefix/PES metadata must not change a present raw query into an
 // absent timestamp or let a later observed prefix win the raw-byte search.
 auto delayed=NativeMovieAdmissionFeed::create(key,token,0,ff);MovieSourceObservationBatch offered{key,token,{}};std::vector<MovieVideoPrefixObserved> late;std::uint64_t serial=0;
 const auto first_at=batch->timestamp_prefix()->prefix.es_prefix_first_byte;
 for(const auto& event:read.observations->events){
  if(const auto* prefix=std::get_if<MovieVideoPrefixObserved>(&event.value);prefix&&prefix->prefix.es_prefix_first_byte>=first_at){late.push_back(*prefix);continue;}
  offered.events.push_back({++serial,event.value});
 }
 check(delayed&&delayed->push(offered,ff),"real offered bytes precede delayed monotone prefix metadata");auto missing=delayed->parse_outer_batch(ff);auto missing_state=delayed->snapshot(ff);
 check(missing&&!missing->batch&&missing_state&&missing_state->outer_batch_pending&&missing_state->es_begin==0&&missing_state->current.sequence_parse_entries==0,"present raw query waits for real metadata before parsing or claiming absence");
 MovieSourceObservationBatch associations{key,token,{}};for(const auto& prefix:late)associations.events.push_back({++serial,prefix});check(!associations.events.empty()&&delayed->push(associations,ff),"actual first and subsequent raw prefix metadata arrive in order");auto resolved=delayed->parse_outer_batch(ff);
 check(resolved&&resolved->batch&&resolved->batch->offered_end()==batch->offered_end()&&resolved->batch->timestamp_prefix()&&resolved->batch->timestamp_prefix()->prefix==batch->timestamp_prefix()->prefix&&resolved->batch->timestamp_prefix()->pes==batch->timestamp_prefix()->pes,"resolved private batch retains original first raw query and offered extent");
 // Synthetic closed-channel negative, deliberately not a valid complete
 // movie: no metadata can arrive after the parser accepts input closure.
 auto closed=NativeMovieAdmissionFeed::create(key,token,0,ff);check(closed&&closed->push(offered,ff),"closed-channel negative retains actual raw bytes");auto wait_closed=closed->parse_outer_batch(ff);check(wait_closed&&!wait_closed->batch,"missing metadata initially waits on open channel");
 const auto last_sequence=offered.events.back().sequence;MovieSourceObservationBatch closure{key,token,{{last_sequence+1,MovieProgramEndObservation{0,4,0}}}};
 check(closed->push(closure,ff),"synthetic input-close event for invalid incomplete protocol");check(!closed->parse_outer_batch(ff)&&ff.kind==MovieAdmissionFeedError::invalid_prefix&&closed->snapshot(ff)->failed,"closed missing-prefix channel fails by name rather than waiting forever");
 for(std::size_t length=1;length<4;++length){auto truncated=NativeMovieAdmissionFeed::create(key,token,0,ff);MovieSourceObservationBatch bad{key,token,{{1,MovieVideoEsObserved{0,length,std::vector<std::uint8_t>(bytes.begin(),bytes.begin()+length)}},{2,MovieProgramEndObservation{0,4,0}}}};check(truncated&&truncated->push(bad,ff)&&!truncated->parse_outer_batch(ff)&&ff.kind==MovieAdmissionFeedError::parser,"closed truncated raw prefix cannot wait for impossible input");}
 check(std::puts("PASS genuine outer parse batch admission/partial scope/rate provenance; no retail driver or playback")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL outer header batch %s\n",e.what());return 1;}}
