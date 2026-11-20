#include "mka/movie_decode.hpp"
#include <cstdio>
#include <deque>
#include <stdexcept>
#include <type_traits>
using namespace mka::video;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Interval {std::uint64_t begin,end;};
struct ReceiptValue {
 std::string key;std::shared_ptr<const MovieSourceObservationIdentity> identity;
 std::uint64_t id,begin,end;std::uint8_t stream;
 explicit ReceiptValue(const MovieCodecInputReceipt& r):key(r.source_key()),identity(r.source_identity()),id(r.input_id()),begin(r.es_begin()),end(r.es_end()),stream(r.stream_id()){}
 bool matches(const MovieCodecInputReceipt& r)const{return key==r.source_key()&&identity==r.source_identity()&&id==r.input_id()&&begin==r.es_begin()&&end==r.es_end()&&stream==r.stream_id();}
};
}
int main(int argc,char** argv){try{
 static_assert(!std::is_default_constructible_v<MovieCodecInputReceipt>);
 static_assert(!std::is_copy_constructible_v<MovieCodecInputReceipt>);
 static_assert(!std::is_aggregate_v<MovieCodecInputReceipt>);
 static_assert(!std::is_constructible_v<MovieCodecInputReceipt,std::string,std::shared_ptr<const MovieSourceObservationIdentity>,std::uint64_t,std::uint8_t,std::uint64_t,std::uint64_t>);
 check(argc==4,"usage: test_native_movie_input_acceptance DLLdir asset_root relative_video_only_file");
 constexpr const char* key="KRYPT/V_RAIDON.SFD";
 MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{key,argv[3]}},af);check(bool(store),"one exact existing video-only source binding");auto asset=store->open(key,af);check(bool(asset),"same-owned associated source");MovieDecodeFailure failure;auto decoder=NativeMovieDecoder::open(std::filesystem::u8path(argv[1]),std::move(*asset),failure);check(bool(decoder),"actual native decoder owner");asset.reset();
 auto identity=decoder->observation_identity();check(bool(identity),"actual source epoch");auto audit=decoder->audio_audit(failure);check(audit&&!audit->present&&!audit->decoder_failed,"fixture is actual video-only source, no audio decode/output scope");
 std::uint64_t sequence=0,raw_end=0,accepted_end=0,raw_count=0,receipt_count=0,last_id=0,decoded_events=0,decoded_before_receipt=0,program_end_count=0,split_candidates=0;
 std::optional<std::uint8_t> stream;std::deque<Interval> unaccepted;
 std::shared_ptr<const MovieCodecInputReceipt> first,last;std::optional<ReceiptValue> first_value,last_value;
 bool ended=false;
 for(;;){auto read=decoder->next(failure);check(read.status!=MovieReadStatus::error&&failure.kind==MovieDecodeError::none,"native accepted-input fixture read");
  if(read.observations){const auto& batch=*read.observations;check(batch.source_key==key&&batch.identity==identity&&!batch.events.empty(),"immutable actual source batch epoch/key");
   for(const auto& event:batch.events){check(sequence!=UINT64_MAX&&event.sequence==sequence+1,"all observation sequence numbers preserved");++sequence;
    if(const auto* raw=std::get_if<MovieVideoEsObserved>(&event.value)){
     check(!ended&&raw->es_begin==raw_end&&raw->es_end>raw->es_begin&&raw->es_end-raw->es_begin==raw->bytes.size(),"original validated input intervals contiguous and nonempty");
     unaccepted.push_back({raw->es_begin,raw->es_end});raw_end=raw->es_end;++raw_count;
     const auto n=raw->bytes.size();if(n>4&&raw->bytes[n-4]==0&&raw->bytes[n-3]==0&&raw->bytes[n-2]==1&&raw->bytes[n-1]==0xb7)++split_candidates;
    }else if(const auto* accepted=std::get_if<MovieCodecInputAccepted>(&event.value)){
     check(!ended&&accepted->receipt,"no receipt for null drain/end or absent issuer");const auto& r=*accepted->receipt;
     check(r.source_key()==key&&r.source_identity()==identity&&last_id!=UINT64_MAX&&r.input_id()==last_id+1,"receipt source epoch/key and native monotonic input identity");
     check(r.stream_id()>=0xe0&&r.stream_id()<=0xef&&(!stream||r.stream_id()==*stream),"actual raw MPEGPS video stream retained");stream=r.stream_id();
     check(!unaccepted.empty()&&r.es_begin()==accepted_end&&r.es_begin()==unaccepted.front().begin&&r.es_end()==unaccepted.front().end&&r.es_end()>r.es_begin()&&r.es_end()<=raw_end,"exactly one receipt for full ORIGINAL unsplit input range");
     accepted_end=r.es_end();last_id=r.input_id();unaccepted.pop_front();++receipt_count;
     if(!first){first=accepted->receipt;first_value.emplace(r);}last=accepted->receipt;last_value.emplace(r);
    }else if(std::holds_alternative<MovieDecodedPictureObserved>(event.value)){
     ++decoded_events;if(!unaccepted.empty())++decoded_before_receipt; // Allowed existing receive-before-receipt order.
    }else if(std::holds_alternative<MovieProgramEndObservation>(event.value)){
     check(!ended&&unaccepted.empty()&&accepted_end==raw_end,"actual input closure follows complete accepted coverage");ended=true;++program_end_count;
    }else check(std::holds_alternative<MovieVideoPrefixObserved>(event.value),"known raw-prefix observation variant");
   }
  }
  if(read.status==MovieReadStatus::end)break;
  check(std::holds_alternative<MovieVideoFrame>(read.frame),"video-only fixture publishes no audio frame");
 }
 check(raw_count>0&&receipt_count==raw_count&&last_id==receipt_count&&unaccepted.empty()&&accepted_end==raw_end&&ended&&program_end_count==1,"complete one-source native input coverage, no missing/duplicate/split/drain receipts");
 auto again=decoder->next(failure);check(again.status==MovieReadStatus::end&&failure.kind==MovieDecodeError::none&&!again.observations,"stable EOF emits no extra receipt or event");
 decoder.reset();store.reset();check(first&&last&&first_value&&last_value&&first_value->matches(*first)&&last_value->matches(*last)&&first->input_id()==1&&first->es_begin()==0&&last->es_end()==accepted_end,"immutable receipt bytes/epoch/value lifetime after all source/decoder owners release");
 check(std::printf("{\"key\":\"KRYPT/V_RAIDON.SFD\",\"raw_intervals\":%llu,\"receipts\":%llu,\"accepted_es_bytes\":%llu,\"stream_id\":%u,\"first_input_id\":%llu,\"last_input_id\":%llu,\"decoded_events\":%llu,\"decoded_events_before_receipt\":%llu,\"program_end_events\":%llu,\"split_candidate_intervals\":%llu}\n",static_cast<unsigned long long>(raw_count),static_cast<unsigned long long>(receipt_count),static_cast<unsigned long long>(accepted_end),unsigned(*stream),static_cast<unsigned long long>(first->input_id()),static_cast<unsigned long long>(last_id),static_cast<unsigned long long>(decoded_events),static_cast<unsigned long long>(decoded_before_receipt),static_cast<unsigned long long>(program_end_count),static_cast<unsigned long long>(split_candidates))>=0,"checked measurement output");
 check(std::puts("PASS native codec original-input acceptance receipts; no decoded-work or playback admission")>=0&&std::fflush(stdout)==0,"checked footer");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native input acceptance %s\n",e.what());return 1;}}
