#include "mka/movie_backend_driver.hpp"
#include <limits>
#include <new>
namespace mka::video {
namespace {
struct Guard{bool& b;explicit Guard(bool& x):b(x){b=true;}~Guard(){b=false;}};
bool fail(MovieControlFailure& f,const char* op,std::int64_t code=0) noexcept {
    if(f.kind==MovieControlError::none)f={MovieControlError::native_service,op,code};return false;
}
}
NativeMovieBackendDriver::NativeMovieBackendDriver(std::string key,
    std::shared_ptr<const MovieSourceObservationIdentity> id,NativeMovieAdmissionFeed& feed,
    NativeMovieFrameWorkCoordinator& work,NativeMovieOutputPosition& position,NativeMoviePlaybackContext& context,
    bool video,bool audio,std::unique_ptr<NativeMovieHostClock> clock,std::unique_ptr<NativeMovieTimestampAssociation> timestamps)
    :key_(std::move(key)),identity_(std::move(id)),feed_(feed),work_(work),position_(position),context_(context),
     video_(video),audio_(audio),clock_(std::move(clock)),timestamps_(std::move(timestamps)){}
NativeMovieBackendDriver::~NativeMovieBackendDriver(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();}
bool NativeMovieBackendDriver::access(MovieControlFailure& f) const noexcept {
    f={};if(owner_!=std::this_thread::get_id()){f={MovieControlError::wrong_thread,"movie backend owner thread",0};return false;}
    if(busy_){f={MovieControlError::invalid_state,"movie backend reentry",0};return false;}return true;
}
std::unique_ptr<NativeMovieBackendDriver> NativeMovieBackendDriver::create(std::string_view key,
    std::shared_ptr<const MovieSourceObservationIdentity> identity,NativeMovieAdmissionFeed& feed,
    NativeMovieFrameWorkCoordinator& work,NativeMovieOutputPosition& position,NativeMoviePlaybackContext& context,
    bool video,bool audio,std::unique_ptr<NativeMovieHostClock> clock,MovieControlFailure& f) noexcept {
    f={};if(key.empty()||!identity||!clock||!video){fail(f,"movie backend normal video source/clock required");return {};}
    MovieTimestampAssociationFailure why;auto timestamps=NativeMovieTimestampAssociation::create(key,identity,why);
    if(!timestamps){fail(f,"movie backend source timestamp owner",static_cast<std::int64_t>(why.kind));return {};}
    try{return std::unique_ptr<NativeMovieBackendDriver>(new NativeMovieBackendDriver(std::string(key),std::move(identity),feed,work,position,context,video,audio,std::move(clock),std::move(timestamps)));}
    catch(const std::bad_alloc&){fail(f,"movie backend owner allocation");return {};}
}
bool NativeMovieBackendDriver::begin_after_start(MovieControlFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    if(state_.begun)return fail(f,"movie backend repeated source start");
    auto state=context_.snapshot(f);if(!state||!state->begun||state->closed)return fail(f,"movie backend actual started context required");
    frequency_=state->service.frequency;baseline_=expected_=state->service.counter;
    if(frequency_<=0)return fail(f,"movie backend service frequency");
    MovieHostClockFailure why;
    if(!clock_->begin(key_,identity_,why)||!clock_->pause(key_,identity_,why))return fail(f,"movie backend native clock start",static_cast<std::int64_t>(why.kind));
    if(!context_.command(MovieNativeCommand::owner_start,f))return false;
    state_.begun=true;return true;
}
bool NativeMovieBackendDriver::pause(bool paused,MovieControlFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    if(state_.begun&&!state_.worker_stopped&&paused_!=paused){
        MovieHostClockFailure why;const bool ok=paused?clock_->pause(key_,identity_,why):clock_->resume(key_,identity_,why);
        if(!ok)return fail(f,"movie backend actual pause clock",static_cast<std::int64_t>(why.kind));
    }
    paused_=paused;return true;
}
bool NativeMovieBackendDriver::update_clock(const MoviePlaybackSnapshot& actual,MovieControlFailure& f) {
    if(actual.paused!=paused_)return fail(f,"movie backend actual pause observation mismatch");
    auto state=context_.snapshot(f);if(!state)return false;
    if(state->service.frequency!=frequency_||state->service.counter!=expected_)return fail(f,"movie backend clock changed outside owner");
    MovieHostClockFailure why;auto timer=clock_->read(key_,identity_,why);
    if(!timer)return fail(f,"movie backend native elapsed clock",static_cast<std::int64_t>(why.kind));
    if(audio_){
        if(!actual.audio_time||!actual.audio_queue)return fail(f,"movie backend actual output time unavailable");
        const auto& a=*actual.audio_time;
        if(a.source_key!=key_||a.source!=identity_||!a.anchor||(audio_anchor_&&audio_anchor_!=a.anchor)||
           a.cancelled||!a.output_clock||a.output_clock->reset_epoch!=actual.audio_queue->reset_epoch||
           a.elapsed.numerator<0||a.elapsed.denominator<=0)return fail(f,"movie backend foreign/reset output time");
        if(last_audio_&&__int128(a.elapsed.numerator)*last_audio_->denominator<__int128(last_audio_->numerator)*a.elapsed.denominator)
            return fail(f,"movie backend output time regression");
        if(!context_.sample_audio(a.elapsed,timer->elapsed,a.query_admitted,a.active,f))return false;
        audio_anchor_=a.anchor;last_audio_=a.elapsed;
    }else{
        const __int128 projected=__int128(frequency_)*timer->elapsed.numerator/timer->elapsed.denominator;
        const __int128 next=__int128(baseline_)+projected,delta=projected-projected_;
        if(projected<0||delta<0||next<INT64_MIN||next>INT64_MAX)return fail(f,"movie backend elapsed projection capacity");
        if(delta&&!context_.increment_service_ticks(expected_,delta,f))return false;
        if(!context_.sample_service(true,f))return false;
        projected_=projected;expected_=static_cast<std::int64_t>(next);
    }
    return true;
}
bool NativeMovieBackendDriver::admit(const std::shared_ptr<const MovieOuterHeaderBatch>& batch,MovieControlFailure& f) {
    const auto& c=*batch->current();
    const auto serial=c.gop_parse_entries;
    const bool changed=!state_.previous_published_gop||*state_.previous_published_gop!=serial;
    // Outer publication changes this serial even when timeline admission is off.
    state_.previous_published_gop=serial;++state_.batches;
    const auto raw=batch->initiating_prefix().prefix.classification;
    const auto effective=state_.admission_mask==0xcc?raw:(raw&0xcc);
    if(!(effective&0x4c)||!(effective&state_.admission_mask))return fail(f,"movie backend unadmitted first-slice batch");
    const auto* sequence=c.sequence?std::get_if<MovieSequenceHeader>(&c.sequence->unit.payload):nullptr;
    const auto* gop=c.gop?std::get_if<MovieGopHeader>(&c.gop->unit.payload):nullptr;
    const auto* picture=c.picture?std::get_if<MoviePictureHeader>(&c.picture->unit.payload):nullptr;
    const auto* coding=c.coding?std::get_if<MoviePictureCodingExtension>(&c.coding->unit.payload):nullptr;
    if(!sequence||!gop||!picture||!coding||coding->picture_structure!=3||coding->repeat_first_field||c.info54)
        return fail(f,"movie backend unsupported normal descriptor history");
    // 80A018 changed admission writes current half0. Temporal0 also removes
    // the reset sentinel, proving subsequent no-repeat zero propagation.
    if(changed)state_.zero_history_established=picture->temporal_reference==0;
    else if(picture->temporal_reference==0)state_.zero_history_established=true;
    else if(!state_.zero_history_established)return fail(f,"movie backend unproved_history_predecessor",picture->temporal_reference);
    MovieTimestampAssociationFailure tf;
    auto associated=timestamps_->update(key_,identity_,batch->timestamp_prefix(),sequence->frame_rate_code,
        picture->temporal_reference,picture->coding_type,changed,tf);
    if(!associated)return fail(f,"movie backend actual source timestamp association",static_cast<std::int64_t>(tf.kind));
    std::optional<std::uint64_t> timestamp;
    if(associated->adjusted_timestamp22500){
        if(*associated->adjusted_timestamp22500<0)return fail(f,"movie backend negative adjusted source timestamp");
        timestamp=static_cast<std::uint64_t>(*associated->adjusted_timestamp22500);
    }
    MovieRewriteDescriptor direct{{sequence->frame_rate_code,gop->drop_frame,gop->hours,gop->minutes,gop->seconds,gop->pictures,picture->temporal_reference,0},0};
    MovieDescriptorRewriteFailure rf;
    // Retail normal defaults: comparison margin53=5; source31E8 absent.
    auto prepared=rewrite_.update({direct,timestamp,serial,false,5,false,changed},rf);
    if(!prepared)return fail(f,"movie backend descriptor rewrite",static_cast<std::int64_t>(rf.kind));
    auto state=context_.snapshot(f);if(!state)return false;
    MovieNativeHeaderAdmission event{batch->batch_id(),batch->initiating_prefix().prefix.es_prefix_first_byte,batch->current(),
        {},false,!state->eligibility_rate,!state->first_baseline,batch,*prepared};
    if(!context_.admit_header(event,f))return false;
    if(effective&state_.admission_mask)state_.admission_mask=0xcc;
    ++state_.admissions;
    MovieFrameWorkFailure wf;
    if(!work_.begin_outer(event.admission_id,event.current,event.admission_id,wf))return fail(f,wf.operation,static_cast<std::int64_t>(wf.kind));
    return true;
}
bool NativeMovieBackendDriver::publish(const MovieFrameWorkResult& result,MovieControlFailure& f) {
    if(!result.work||!context_.validate_work(*result.work->current_header,f))return fail(f,"movie backend actual work record required");
    MovieOutputPositionFailure why;
    auto update=result.progress==MovieFrameWorkProgress::flushed?
        position_.flush(key_,identity_,*result.work,result.selected,why):
        position_.complete_work(key_,identity_,*result.work,result.selected,why);
    if(!update)return fail(f,why.operation,static_cast<std::int64_t>(why.kind));
    if(update->publication){
        if(!result.timing_admission_id||!context_.publish_output({result.work->admission_id,*result.timing_admission_id,
            update->publication->selected,update->publication->value},f))return fail(f,"movie backend checked work publication");
        if(!pending_.emplace(update->publication->selected.picture_es_byte,update->publication->selected).second)
            return fail(f,"movie backend repeated selected publication");
        ++state_.published;
    }
    if(result.progress!=MovieFrameWorkProgress::flushed)++state_.processed;
    return true;
}
bool NativeMovieBackendDriver::finish(const MoviePlaybackBackendObservation& o,MoviePlaybackBackendResult& out,MovieControlFailure& f) {
    if(o.head_video){
        if(!o.head_video->codec_timing.picture_source)return fail(f,"movie backend head lacks selected source");
        const auto& source=*o.head_video->codec_timing.picture_source;auto found=pending_.find(source.picture_es_byte);
        out.head_frame_published=found!=pending_.end()&&found->second==source;
        out.prepare_head_video=out.head_frame_published&&!o.head_video_prepared;
    }
    auto state=context_.snapshot(f);if(!state)return false;
    if(video_&&!state->video_started&&out.head_frame_published&&o.head_video_prepared)
        if(!context_.command(MovieNativeCommand::video_start,f))return false;
    if(audio_&&!state->audio_started&&state->accepted_audio_frames&&o.playback.audio_queue&&o.playback.audio_queue->submitted_frames)
        if(!context_.command(MovieNativeCommand::audio_start,f))return false;
    MovieFrameWorkFailure wf;auto work=work_.snapshot(wf);
    if(!work)return fail(f,wf.operation,static_cast<std::int64_t>(wf.kind));
    if(work->flushed&&!state_.worker_stopped){
        const auto& s=o.playback;
        const bool drained=s.decoder_eof&&!s.queued_video&&!s.queued_audio&&!s.outstanding_fences&&pending_.empty()&&
            (!audio_||(s.audio_queue&&s.source_retirement&&!s.audio_queue->padding_frames&&!s.source_retirement->pending));
        if(drained){
            if(work->outer_active||work->waiting_output||work->retained_inputs||work->retained_outputs)
                return fail(f,"movie backend flushed worker still owns pending work");
            if(video_&&!context_.command(MovieNativeCommand::video_finish,f))return false;
            if(audio_&&!context_.command(MovieNativeCommand::audio_finish,f))return false;
            MovieHostClockFailure why;
            if(!clock_->stop(key_,identity_,why))return fail(f,"movie backend native worker clock stop",static_cast<std::int64_t>(why.kind));
            // Successful native worker quiescence, distinct from controller's
            // subsequent Stream/device resource cancellation.
            if(!context_.command(MovieNativeCommand::owner_stop,f))return false;
            state_.worker_stopped=true;
        }
    }
    out.needs_source_input=!work->flushed&&!out.head_frame_published&&!o.playback.decoder_eof;
    return true;
}
bool NativeMovieBackendDriver::pump(const MoviePlaybackBackendObservation& o,MoviePlaybackBackendResult& out,MovieControlFailure& f) noexcept {
    out={};if(!access(f))return false;Guard guard(busy_);
    if(!state_.begun||o.request.logical_key!=key_||o.source!=identity_)return fail(f,"movie backend foreign/unstarted source");
    try {
        if(state_.worker_stopped)return finish(o,out,f);
        if(!update_clock(o.playback,f))return false;
        for(;;){
            if(o.head_video&&o.head_video->codec_timing.picture_source){const auto& selected=*o.head_video->codec_timing.picture_source;
                auto found=pending_.find(selected.picture_es_byte);if(found!=pending_.end()&&found->second==selected)break;}
            MovieFrameWorkFailure wf;auto work=work_.snapshot(wf);if(!work)return fail(f,wf.operation,static_cast<std::int64_t>(wf.kind));
            if(work->flushed)break;
            if(work->outer_active||work->waiting_output){
                auto result=work_.next(wf);if(!result)return fail(f,wf.operation,static_cast<std::int64_t>(wf.kind));
                if(result->progress!=MovieFrameWorkProgress::need_more){if(!publish(*result,f))return false;continue;}
                if(!o.playback.decoder_eof)break;
            }else{
                MovieAdmissionFeedFailure ff;auto view=feed_.snapshot(ff);
                if(!view)return fail(f,"movie backend shared feed snapshot",static_cast<std::int64_t>(ff.kind));
                if(view->es_begin<view->es_end){
                    auto batch=feed_.parse_outer_batch(ff);
                    if(!batch)return fail(f,"movie backend actual outer parse batch",static_cast<std::int64_t>(ff.kind));
                    if(batch->progress==MovieOuterHeaderBatchProgress::need_more){if(!o.playback.decoder_eof)break;return fail(f,"movie backend closed source incomplete batch");}
                    if(batch->progress==MovieOuterHeaderBatchProgress::first_slice){if(!admit(batch->batch,f))return false;continue;}
                    if(!o.playback.decoder_eof)break;
                }else if(!o.playback.decoder_eof)break;
            }
            auto final=work_.flush_after_decoder_end(o.playback.decoder_eof,wf);
            if(!final||final->progress==MovieFrameWorkProgress::need_more)return fail(f,"movie backend final native work incomplete",static_cast<std::int64_t>(wf.kind));
            if(!publish(*final,f))return false;
        }
        return finish(o,out,f);
    }catch(const std::bad_alloc&){return fail(f,"movie backend ownership allocation");}
     catch(...){return fail(f,"movie backend unexpected native operation exception");}
}
bool NativeMovieBackendDriver::video_submitted(const MovieGpuMetadata& m,MovieControlFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    if(!m.codec_timing.picture_source)return fail(f,"movie backend submitted source missing");
    const auto& source=*m.codec_timing.picture_source;auto found=pending_.find(source.picture_es_byte);
    if(found==pending_.end()||found->second!=source)return fail(f,"movie backend submitted source not published");
    pending_.erase(found);return true;
}
} // namespace mka::video
