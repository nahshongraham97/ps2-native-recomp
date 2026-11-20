#include "mka/movie_frame_work.hpp"
#include <new>
#include <stdexcept>
namespace mka::video {
namespace {struct Guard {bool& b;explicit Guard(bool& x):b(x){b=true;}~Guard(){b=false;}};}
NativeMovieFrameWorkCoordinator::NativeMovieFrameWorkCoordinator(NativeMovieAdmissionFeed& feed,
    std::string key,std::shared_ptr<const MovieSourceObservationIdentity> identity)
    :feed_(feed),key_(std::move(key)),identity_(std::move(identity)){}
NativeMovieFrameWorkCoordinator::~NativeMovieFrameWorkCoordinator(){if(busy_||owner_!=std::this_thread::get_id())std::terminate();if(inner_scope_){MovieAdmissionFeedFailure why;if(!feed_.end_inner_parse(why))std::terminate();}}
bool NativeMovieFrameWorkCoordinator::fail(MovieFrameWorkFailure& f,MovieFrameWorkError kind,const char* operation) const noexcept {f.kind=kind;f.operation=operation;return false;}
bool NativeMovieFrameWorkCoordinator::access(MovieFrameWorkFailure& f) const noexcept {
    f={};if(owner_!=std::this_thread::get_id())return fail(f,MovieFrameWorkError::wrong_thread,"movie frame work owner thread");
    if(busy_)return fail(f,MovieFrameWorkError::reentry,"movie frame work reentry");
    if(state_.failed)return fail(f,MovieFrameWorkError::failed,"movie frame work terminal owner");return true;
}
std::unique_ptr<NativeMovieFrameWorkCoordinator> NativeMovieFrameWorkCoordinator::create(
    NativeMovieAdmissionFeed& feed,std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,
    MovieFrameWorkProfile profile,MovieFrameWorkFailure& f) noexcept {
    f={};if(profile!=MovieFrameWorkProfile::frame_no_repeat_no_skip){f.kind=MovieFrameWorkError::unsupported_profile;f.operation="movie frame work explicit normal profile";return {};}
    auto source=feed.snapshot(f.feed);
    if(!source||key.empty()||key.find('\0')!=key.npos||!id||!source->current.source||source->current.source->source_key!=key||source->current.source->identity!=id||source->observation_sequence){f.kind=MovieFrameWorkError::invalid_owner;f.operation="movie frame work fresh same-source feed";return {};}
    try{return std::unique_ptr<NativeMovieFrameWorkCoordinator>(new NativeMovieFrameWorkCoordinator(feed,std::string(key),std::move(id)));}
    catch(const std::bad_alloc&){f.kind=MovieFrameWorkError::allocation;f.operation="movie frame work owner allocation";return {};}
}
bool NativeMovieFrameWorkCoordinator::observe_source(const MovieSourceObservationBatch& batch,MovieFrameWorkFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    if(batch.source_key!=key_||batch.identity!=identity_||!batch.identity)return fail(f,MovieFrameWorkError::foreign_source,"movie frame work observation source");
    auto feed=feed_.snapshot(f.feed);if(!feed)return fail(f,MovieFrameWorkError::feed,"movie frame work observed feed");
    try {
        auto serial=state_.observation_sequence;auto input=last_input_;auto ended=state_.program_end;
        std::map<std::uint64_t,std::shared_ptr<const MovieCodecInputReceipt>> inputs;
        std::map<std::uint64_t,MovieDecodedPictureObserved> pictures;
        if(batch.events.empty()||state_.flushed)return fail(f,MovieFrameWorkError::invalid_order,"movie frame work observation lifecycle");
        for(const auto& event:batch.events){
            if(serial==UINT64_MAX||event.sequence!=++serial)return fail(f,MovieFrameWorkError::invalid_order,"movie frame work observation sequence");
            if(const auto* accepted=std::get_if<MovieCodecInputAccepted>(&event.value)){
                if(check_movie_input_receipt(accepted->receipt,key_,identity_,input,feed->es_end,ended)!=MovieInputReceiptError::none)return fail(f,MovieFrameWorkError::invalid_receipt,"movie frame work actual native input receipt");
                input=accepted->receipt;inputs.emplace(input->es_begin(),input);
            }else if(const auto* picture=std::get_if<MovieDecodedPictureObserved>(&event.value)){
                const auto at=picture->picture.picture_es_byte;
                if(decoded_.contains(at)||pictures.contains(at)||published_pictures_.contains(at))return fail(f,MovieFrameWorkError::invalid_order,"movie frame work duplicate decoded source");
                pictures.emplace(at,*picture);
            }else if(std::holds_alternative<MovieProgramEndObservation>(event.value)){
                if(ended)return fail(f,MovieFrameWorkError::invalid_order,"movie frame work duplicate program end");ended=true;
            }
        }
        if(serial!=feed->observation_sequence)return fail(f,MovieFrameWorkError::invalid_order,"movie frame work feed delivery mismatch");
        inputs_.merge(inputs);decoded_.merge(pictures);last_input_=std::move(input);state_.observation_sequence=serial;state_.program_end=ended;return true;
    }catch(const std::bad_alloc&){return fail(f,MovieFrameWorkError::allocation,"movie frame work observation allocation");}
     catch(const std::length_error&){return fail(f,MovieFrameWorkError::overflow,"movie frame work observation capacity");}
}
bool NativeMovieFrameWorkCoordinator::begin_outer(std::uint64_t admission,std::shared_ptr<const MovieAdmissionCurrent> header,
    std::optional<std::uint64_t> timing,MovieFrameWorkFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    if(state_.outer_active||pending_||state_.flushed||!admission||!header||!header->source||header->source->source_key!=key_||header->source->identity!=identity_||!header->picture||!header->coding||!timing||!*timing)return fail(f,MovieFrameWorkError::invalid_order,"movie frame work outer admission lifecycle");
    auto current=feed_.snapshot(f.feed);if(!current)return fail(f,MovieFrameWorkError::feed,"movie frame work outer feed snapshot");
    if(current->current.picture!=header->picture||current->current.coding!=header->coding)return fail(f,MovieFrameWorkError::foreign_source,"movie frame work outer current header");
    if(!feed_.begin_inner_parse(f.feed))return fail(f,MovieFrameWorkError::feed,"movie frame work inner parser scope");
    inner_scope_=true;
    outer_=std::move(header);outer_admission_=admission;timing_=timing;state_.outer_active=true;return true;
}
std::optional<MovieFrameWorkResult> NativeMovieFrameWorkCoordinator::release_pending(MovieFrameWorkFailure& f) noexcept {
    if(!pending_)return MovieFrameWorkResult{};
    if(expected_){
        auto actual=decoded_.find(expected_->picture_es_byte);
        if(actual==decoded_.end())return MovieFrameWorkResult{};
        if(actual->second.picture!=*expected_||published_pictures_.contains(expected_->picture_es_byte)){fail(f,MovieFrameWorkError::foreign_source,"movie frame work selected decoded source mismatch");state_.failed=true;return {};}
        if(state_.published==UINT64_MAX){fail(f,MovieFrameWorkError::overflow,"movie frame work output capacity");state_.failed=true;return {};}
        try {published_pictures_.insert(expected_->picture_es_byte);}
        catch(const std::bad_alloc&){fail(f,MovieFrameWorkError::allocation,"movie frame work selected ownership allocation");state_.failed=true;return {};}
        catch(const std::length_error&){fail(f,MovieFrameWorkError::overflow,"movie frame work selected ownership capacity");state_.failed=true;return {};}
        if(!inner_scope_||!feed_.end_inner_parse(f.feed)){fail(f,MovieFrameWorkError::feed,"movie frame work restore outer parser scope");state_.failed=true;return {};}
        inner_scope_=false;
        pending_->selected=actual->second.picture;decoded_.erase(actual);++state_.published;state_.outer_active=false;
    }
    state_.last_work_id=pending_->work->work_id;
    auto result=std::move(*pending_);pending_.reset();expected_.reset();state_.waiting_output=false;return result;
}
bool NativeMovieFrameWorkCoordinator::retire_control_inputs(const MovieAdmissionFeedSnapshot& view,MovieFrameWorkFailure& f) noexcept {
    while(!inputs_.empty()&&inputs_.begin()->second->es_end()<=view.es_begin){
        const auto& receipt=inputs_.begin()->second;
        const auto picture=view.current.picture?view.current.picture->origin.prefix.es_prefix_first_byte:UINT64_MAX;
        // Only grammar-consumed, picture-free intervals retire as controls.
        // A parsed picture without a completed body is never discarded.
        if(picture>=receipt->es_begin()&&picture<receipt->es_end()&&!processed_pictures_.contains(picture)){
            state_.failed=true;return fail(f,MovieFrameWorkError::invalid_order,"movie frame work unprocessed picture input");
        }
        inputs_.erase(inputs_.begin());
    }
    return true;
}
std::optional<MovieFrameWorkResult> NativeMovieFrameWorkCoordinator::next(MovieFrameWorkFailure& f) noexcept {
    if(!access(f))return {};Guard guard(busy_);
    if(pending_)return release_pending(f);
    if(!state_.outer_active||state_.flushed){fail(f,MovieFrameWorkError::invalid_order,"movie frame work next without outer operation");return {};}
    try {
        auto view=feed_.snapshot(f.feed);if(!view){fail(f,MovieFrameWorkError::feed,"movie frame work source snapshot");return {};}
        while(!view->work_pending){
            auto candidate=feed_.parse_next(f.feed);if(!candidate){state_.failed=true;fail(f,MovieFrameWorkError::feed,"movie frame work inner header parse");return {};}
            if(candidate->kind==MovieAdmissionCandidateKind::need_more)return MovieFrameWorkResult{};
            if(candidate->kind!=MovieAdmissionCandidateKind::parsed_header&&candidate->kind!=MovieAdmissionCandidateKind::first_slice){state_.failed=true;fail(f,MovieFrameWorkError::unsupported_profile,"movie frame work unsupported inner source unit");return {};}
            view=feed_.snapshot(f.feed);if(!view){state_.failed=true;fail(f,MovieFrameWorkError::feed,"movie frame work parsed source snapshot");return {};}
            if(!retire_control_inputs(*view,f))return {};
        }
        const auto& current=view->current;
        const auto* picture=current.picture?std::get_if<MoviePictureHeader>(&current.picture->unit.payload):nullptr;
        const auto* coding=current.coding?std::get_if<MoviePictureCodingExtension>(&current.coding->unit.payload):nullptr;
        if(!picture||!coding||coding->picture_structure!=3||coding->repeat_first_field||picture->coding_type<1||picture->coding_type>3){state_.failed=true;fail(f,MovieFrameWorkError::unsupported_profile,"movie frame work frame/no-repeat/no-skip profile");return {};}
        const auto at=current.picture->origin.prefix.es_prefix_first_byte;
        if(processed_pictures_.contains(at)){state_.failed=true;fail(f,MovieFrameWorkError::invalid_order,"movie frame work replayed processing picture");return {};}
        auto accepted=inputs_.upper_bound(at);if(accepted==inputs_.begin())return MovieFrameWorkResult{};--accepted;
        if(at>=accepted->second->es_end())return MovieFrameWorkResult{};
        auto extent=feed_.accepted_picture_body(accepted->second,f.feed);
        if(!extent){state_.failed=true;fail(f,MovieFrameWorkError::feed,"movie frame work accepted compressed body");return {};}
        MoviePictureSource source{accepted->second->es_begin(),at,current.picture->origin.pes,current.picture->origin.quarter_pts};
        std::optional<MoviePictureSource> expected=picture->coding_type==3?std::optional<MoviePictureSource>{source}:reference_;
        if(picture->coding_type==3&&!reference_){state_.failed=true;fail(f,MovieFrameWorkError::unsupported_profile,"movie frame work B before admitted reference");return {};}
        if(state_.last_work_id==UINT64_MAX||state_.processed==UINT64_MAX){state_.failed=true;fail(f,MovieFrameWorkError::overflow,"movie frame work native count capacity");return {};}
        auto inner=std::make_shared<const MovieAdmissionCurrent>(current);
        MovieFrameWorkResult result;
        result.progress=expected?MovieFrameWorkProgress::output:MovieFrameWorkProgress::processed_without_output;
        result.work=MovieOutputPositionWork{state_.last_work_id+1,outer_admission_,outer_};result.processing_header=std::move(inner);result.input=accepted->second;result.timing_admission_id=timing_;result.consumed_begin=extent->es_begin;result.consumed_end=extent->es_end;
        processed_pictures_.insert(at);
        if(!feed_.skip(extent->es_end-extent->es_begin,f.feed)){state_.failed=true;fail(f,MovieFrameWorkError::feed,"movie frame work actual accepted body consumption");return {};}
        if(picture->coding_type!=3)reference_=source;
        last_processing_=result.processing_header;
        inputs_.erase(accepted);++state_.processed;expected_=expected;pending_=std::move(result);state_.waiting_output=bool(expected_);return release_pending(f);
    }catch(const std::bad_alloc&){state_.failed=true;fail(f,MovieFrameWorkError::allocation,"movie frame work processing allocation");return {};}
     catch(const std::length_error&){state_.failed=true;fail(f,MovieFrameWorkError::overflow,"movie frame work host capacity");return {};}
}
std::optional<MovieFrameWorkResult> NativeMovieFrameWorkCoordinator::flush_after_decoder_end(bool eof,MovieFrameWorkFailure& f) noexcept {
    if(!access(f))return {};Guard guard(busy_);
    if(!eof)return MovieFrameWorkResult{};
    if(pending_){auto result=release_pending(f);if(!result)return {};if(result->progress!=MovieFrameWorkProgress::need_more)return result;state_.failed=true;fail(f,MovieFrameWorkError::missing_output,"movie frame work decoder ended before selected output");return {};}
    if(state_.flushed){fail(f,MovieFrameWorkError::invalid_order,"movie frame work duplicate final flush");return {};}
    if(!state_.program_end||!outer_||!reference_||!last_processing_){state_.failed=true;fail(f,MovieFrameWorkError::invalid_order,"movie frame work incomplete source before flush");return {};}
    if(!inner_scope_){if(!feed_.begin_inner_parse(f.feed)){fail(f,MovieFrameWorkError::feed,"movie frame work final parser scope");return {};}inner_scope_=true;}
    auto view=feed_.snapshot(f.feed);if(!view){fail(f,MovieFrameWorkError::feed,"movie frame work flush source snapshot");return {};}
    if(!retire_control_inputs(*view,f))return {};
    if(view->es_begin!=view->es_end){
        auto end=feed_.parse_next(f.feed);
        if(!end||end->kind!=MovieAdmissionCandidateKind::parsed_header||!end->parsed||!std::holds_alternative<MovieSequenceEndHeader>(end->parsed->unit.payload)){state_.failed=true;fail(f,MovieFrameWorkError::invalid_order,"movie frame work unprocessed input at final flush");return {};}
        view=feed_.snapshot(f.feed);
        if(view&&!retire_control_inputs(*view,f))return {};
    }
    if(!view||view->es_begin!=view->es_end||view->work_pending||!inputs_.empty()||decoded_.size()!=1||!decoded_.contains(reference_->picture_es_byte)){state_.failed=true;fail(f,MovieFrameWorkError::missing_output,"movie frame work final exact owned decoded reference");return {};}
    if(state_.last_work_id==UINT64_MAX){state_.failed=true;fail(f,MovieFrameWorkError::overflow,"movie frame work final identity capacity");return {};}
    MovieFrameWorkResult result;result.progress=MovieFrameWorkProgress::flushed;result.work=MovieOutputPositionWork{state_.last_work_id+1,outer_admission_,outer_};result.processing_header=last_processing_;result.timing_admission_id=timing_;pending_=std::move(result);expected_=reference_;
    auto released=release_pending(f);if(!released)return {};state_.flushed=true;return released;
}
std::optional<MovieFrameWorkSnapshot> NativeMovieFrameWorkCoordinator::snapshot(MovieFrameWorkFailure& f) const noexcept {
    if(!access(f))return {};Guard guard(busy_);auto value=state_;value.retained_inputs=inputs_.size();value.retained_outputs=decoded_.size();return value;
}
} // namespace mka::video
