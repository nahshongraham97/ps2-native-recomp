#include "mka/movie_frame_work.hpp"
#include "mka/movie_decode.hpp"
#include <cstdio>
#include <stdexcept>
#include <type_traits>
using namespace mka::video;
namespace {void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}}
// One existing source through the NEW work coordinator, metadata/ownership only.
// No media content/hash/golden comparison, presentation or audio device.
int main(int argc,char** argv){try{
    static_assert(!std::is_copy_constructible_v<NativeMovieFrameWorkCoordinator>);
    check(argc==4,"usage: test_native_movie_frame_work DLLdir asset_root relative_video_only_file");
    constexpr const char* key="KRYPT/V_RAIDON.SFD";
    MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{key,argv[3]}},af);check(bool(store),"one original source association");
    auto asset=store->open(key,af);check(bool(asset),"same associated source owner");
    MovieDecodeFailure df;auto decoder=NativeMovieDecoder::open(std::filesystem::u8path(argv[1]),std::move(*asset),df);check(bool(decoder),"actual native source");
    auto identity=decoder->observation_identity();auto audit=decoder->audio_audit(df);check(identity&&audit&&!audit->present,"explicit video-only source facts");
    MovieAdmissionFeedFailure ff;auto feed=NativeMovieAdmissionFeed::create(key,identity,0,ff);check(bool(feed),"same-source shared feed");
    MovieFrameWorkFailure wf;auto work=NativeMovieFrameWorkCoordinator::create(*feed,key,identity,MovieFrameWorkProfile::frame_no_repeat_no_skip,wf);check(bool(work),"actual normal native work owner");
    MovieOutputPositionFailure pf;auto position=NativeMovieOutputPosition::create(key,identity,MovieOutputPositionProfile::successful_frame_no_repeat_no_skip,pf);check(bool(position),"independent actual processing publication owner");
    std::uint64_t admission=0,processed=0,selected=0,without=0,inner_differs=0,b_pictures=0;
    std::set<std::uint64_t> actual_decoded,actual_selected;bool eof=false,flushed=false;
    const auto accept=[&](const MovieFrameWorkResult& result){
        check(result.work&&result.processing_header&&result.processing_header->picture&&result.processing_header->coding,"work has distinct frozen outer/current processing identities");
        if(result.progress==MovieFrameWorkProgress::flushed){check(eof&&!result.input,"flush is distinct from another codec input/work increment");}
        else {check(result.input&&result.consumed_end>result.consumed_begin&&result.input->es_begin()<=result.processing_header->picture->origin.prefix.es_prefix_first_byte&&result.consumed_end<=result.input->es_end(),"actual processed body joins authentic codec interval");++processed;
            if(result.processing_header->picture!=result.work->current_header->picture)++inner_differs;
            if(std::get<MoviePictureHeader>(result.processing_header->picture->unit.payload).coding_type==3)++b_pictures;
        }
        auto update=result.progress==MovieFrameWorkProgress::flushed?position->flush(key,identity,*result.work,result.selected,pf):position->complete_work(key,identity,*result.work,result.selected,pf);
        check(bool(update),"real work result feeds native publication arithmetic");
        if(result.selected){check(actual_decoded.contains(result.selected->picture_es_byte)&&actual_selected.insert(result.selected->picture_es_byte).second&&update->publication,"selected actual source is unique and owned, never ordinal");check(update->publication->value==selected,"contiguous publication for this observed no-skip source");++selected;}
        else {check(result.progress==MovieFrameWorkProgress::processed_without_output&&processed==1&&!update->publication,"only definite first reference is no-output");++without;}
    };
    while(!flushed){
        if(!eof){auto read=decoder->next(df);check(read.status!=MovieReadStatus::error&&df.kind==MovieDecodeError::none,"native source read for processing integration");
            if(read.observations){for(const auto& event:read.observations->events)if(const auto* frame=std::get_if<MovieDecodedPictureObserved>(&event.value))check(actual_decoded.insert(frame->picture.picture_es_byte).second,"actual decoded source observation unique");
                check(feed->push(*read.observations,ff)&&work->observe_source(*read.observations,wf),"same actual observations reach shared input/work owners");}
            eof=read.status==MovieReadStatus::end;
        }
        bool waiting=false;
        while(!waiting){
            auto state=work->snapshot(wf);check(bool(state),"processing state snapshot");
            if(state->outer_active||state->waiting_output){
                auto result=work->next(wf);check(bool(result),"checked native processing operation");
                if(result->progress!=MovieFrameWorkProgress::need_more){accept(*result);continue;}
                if(!eof){waiting=true;break;}
            }else{
                auto view=feed->snapshot(ff);check(view&&!view->inner_parse_active,"next outer parse inherits outer current only");
                if(view->es_begin<view->es_end){
                    auto candidate=feed->parse_next(ff);check(bool(candidate),"outer checked parser invocation");
                    if(candidate->kind==MovieAdmissionCandidateKind::parsed_header)continue;
                    if(candidate->kind==MovieAdmissionCandidateKind::first_slice){
                        auto outer=std::make_shared<const MovieAdmissionCurrent>(candidate->current);++admission;
                        check(work->begin_outer(admission,outer,admission,wf),"explicit fixture outer admission freezes before inner processing");
                        check(!work->begin_outer(admission,outer,admission,wf)&&wf.kind==MovieFrameWorkError::invalid_order,"nested outer operation rejected without replacement");continue;
                    }
                    check(candidate->kind==MovieAdmissionCandidateKind::need_more,"no unsupported source units skipped by fixture");
                }
                if(!eof){waiting=true;break;}
            }
            auto final=work->flush_after_decoder_end(eof,wf);check(final&&final->progress!=MovieFrameWorkProgress::need_more,"actual drained source produces explicit final selected reference");accept(*final);
            flushed=final->progress==MovieFrameWorkProgress::flushed;if(flushed)break;
        }
    }
    auto state=work->snapshot(wf);auto published=position->snapshot(pf);auto remaining=feed->snapshot(ff);
    check(state&&published&&remaining&&state->flushed&&!state->outer_active&&!state->waiting_output&&!state->retained_inputs&&!state->retained_outputs&&!remaining->inner_parse_active&&remaining->es_begin==remaining->es_end,"complete source processing leaves no unexplained input/output/parser ownership");
    check(processed>1&&processed==actual_decoded.size()&&selected==actual_decoded.size()&&actual_selected==actual_decoded&&without==1&&inner_differs>0&&b_pictures>0&&state->processed==processed&&published->successful_work==processed&&published->flushed,"first-reference/no-output, inner/outer distinction, B selection and flush cover actual source exactly");
    check(!work->flush_after_decoder_end(true,wf)&&wf.kind==MovieFrameWorkError::invalid_order,"duplicate flush cannot publish again");
    std::printf("{\"processed\":%llu,\"selected\":%llu,\"first_reference_no_output\":%llu,\"inner_outer_differences\":%llu,\"b_pictures\":%llu}\n",(unsigned long long)processed,(unsigned long long)selected,(unsigned long long)without,(unsigned long long)inner_differs,(unsigned long long)b_pictures);
    check(std::puts("PASS actual native frame work source identities/outer scope/delayed selection/flush; no playback or content comparison")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native frame work %s\n",e.what());return 1;}}
