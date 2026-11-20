#include "mka/movie_service.hpp"
#include <cmath>
#include <limits>
#include <new>
#include <type_traits>
namespace mka::video {
struct MovieServicePort {
    std::uint64_t generation=0,factory_calls=0;
    std::string key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    NativeMoviePlaybackContext* context=nullptr;
    NativeMovieKernelPolicies* kernel=nullptr;
    std::shared_ptr<MovieEligibilityServiceBinding> clock;
    std::unique_ptr<NativeMovieOutputPosition> position;
    std::unique_ptr<NativeMovieAdmissionFeed> feed;
    MovieAdmissionFeedFailure feed_failure;
    std::map<std::uint64_t,std::shared_ptr<const MovieAdmissionCurrent>> admissions;
    const std::thread::id owner=std::this_thread::get_id();
    bool opening=true;
    void invalidate() noexcept {opening=false;context=nullptr;kernel=nullptr;feed.reset();}
};
namespace {
static_assert(std::is_nothrow_move_assignable_v<MoviePlaybackSnapshot>);
static_assert(std::is_nothrow_move_constructible_v<MoviePlaybackSnapshot>);
static_assert(std::is_nothrow_move_assignable_v<MovieSessionFailure>);
struct Guard {bool& flag;explicit Guard(bool& f):flag(f){flag=true;}~Guard(){flag=false;}};
bool same_request(const RetailMovieRequest& a,const RetailMovieRequest& b) noexcept {return a.id==b.id&&a.logical_key==b.logical_key&&a.logical_extent.width==b.logical_extent.width&&a.logical_extent.height==b.logical_extent.height&&a.requested_view==b.requested_view&&a.selected_view==b.selected_view&&a.route==b.route&&a.route_extra==b.route_extra&&a.subtitle_language==b.subtitle_language&&a.subtitle_tag==b.subtitle_tag;}
bool fail(MovieServiceFailure& f,MovieServiceError kind,const char* op) noexcept {f.kind=kind;f.operation=op;return false;}
std::unique_ptr<MoviePlaybackPolicies> policy_error(MovieSessionFailure& f,const char* op,MovieSessionError kind=MovieSessionError::control) noexcept {if(f.kind==MovieSessionError::none){f.kind=kind;f.operation=op;}return {};}
MovieControlFailure observation_error(const MovieSessionFailure& f) noexcept {
    if(f.control.kind!=MovieControlError::none)return f.control;
    return {f.kind==MovieSessionError::wrong_thread?MovieControlError::wrong_thread:MovieControlError::native_service,f.operation,static_cast<std::int64_t>(f.kind)};
}
MovieControlFailure control_error(const MovieServiceFailure& f) noexcept {if(f.control.kind!=MovieControlError::none)return f.control;if(f.session.control.kind!=MovieControlError::none)return f.session.control;if(f.observation.kind!=MovieControlError::none)return f.observation;if(f.cleanup.kind!=MovieSessionError::none)return observation_error(f.cleanup);return {f.kind==MovieServiceError::wrong_thread?MovieControlError::wrong_thread:MovieControlError::native_service,f.operation,static_cast<std::int64_t>(f.kind)};}
// Lifetime wrapper only: all business calls execute the concrete kernel. Its
// destructor clears every captured raw port before the owned kernel/context.
class LeasedPolicies final:public MoviePlaybackPolicies {
public:
    LeasedPolicies(std::unique_ptr<NativeMovieKernelPolicies> p,NativeMoviePlaybackContext* context,std::shared_ptr<MovieServicePort> port) noexcept:kernel_(std::move(p)),port_(std::move(port)){port_->context=context;port_->kernel=kernel_.get();}
    ~LeasedPolicies() override{port_->invalidate();kernel_.reset();}
    bool validate_observation_source(std::shared_ptr<const MovieSourceObservationIdentity> p,MovieControlFailure& f)override{return kernel_->validate_observation_source(std::move(p),f);}
    bool observe_source(const MovieSourceObservationBatch& b,MovieControlFailure& f)override{
        f={};
        if(!port_->feed){port_->feed_failure.kind=MovieAdmissionFeedError::invalid_owner;f={MovieControlError::native_service,"movie service missing source feed",static_cast<std::int64_t>(port_->feed_failure.kind)};return false;}
        if(!port_->feed->push(b,port_->feed_failure)){f={MovieControlError::native_service,"movie service source feed observation",static_cast<std::int64_t>(port_->feed_failure.kind)};return false;}
        return kernel_->observe_source(b,f);
    }
    bool validate_audio_format(const std::optional<sound::AudioStreamFormat>& x,MovieControlFailure& f)override{return kernel_->validate_audio_format(x,f);}
    bool begin(const RetailMovieRequest& r,std::int32_t g,MovieControlFailure& f)override{return kernel_->begin(r,g,f);}
    bool pump_metadata(MovieControlFailure& f)override{return kernel_->pump_metadata(f);}
    bool pause(bool p,MovieControlFailure& f)override{return kernel_->pause(p,f);}
    bool ready(const MoviePlaybackSnapshot& s,bool& out,MovieControlFailure& f)override{return kernel_->ready(s,out,f);}
    bool eligible(const MovieVideoFrame& v,const MoviePlaybackSnapshot& s,bool& out,MovieControlFailure& f)override{return kernel_->eligible(v,s,out,f);}
    bool video_submitted(const MovieGpuMetadata& m,MovieControlFailure& f)override{return kernel_->video_submitted(m,f);}
    bool prepare_audio(const MovieAudioFrame& a,std::int32_t gain,std::vector<float>& out,MovieControlFailure& f)override{return kernel_->prepare_audio(a,gain,out,f);}
    bool audio_accepted(const MoviePlaybackAudioAcceptance& a,MovieControlFailure& f)override{return kernel_->audio_accepted(a,f);}
    bool set_gain(std::int32_t gain,MovieControlFailure& f)override{return kernel_->set_gain(gain,f);}
    bool end_admitted(const MoviePlaybackSnapshot& s,bool& out,MovieControlFailure& f)override{return kernel_->end_admitted(s,out,f);}
    bool stop(MovieControlFailure& f)override{return kernel_->stop(f);}
private:std::unique_ptr<NativeMovieKernelPolicies> kernel_;std::shared_ptr<MovieServicePort> port_;
};
MovieSessionPolicyFactory policies_for(std::weak_ptr<MovieServicePort> weak,RetailMovieRequest expected,MovieServiceProfileFactory profile_factory){
    return [weak,expected=std::move(expected),profile_factory=std::move(profile_factory)](const RetailMovieRequest& request,std::shared_ptr<const MovieSourceObservationIdentity> identity,const std::optional<sound::AudioStreamFormat>& format,MovieSessionFailure& f)->std::unique_ptr<MoviePlaybackPolicies>{
        f={};auto port=weak.lock();if(!port||!port->opening||std::this_thread::get_id()!=port->owner)return policy_error(f,"movie service expired/foreign policy factory");
        if(port->factory_calls)return policy_error(f,"movie service policy factory reused");
        port->factory_calls=1;
        if(!identity||!same_request(request,expected))return policy_error(f,"movie service actual source request/epoch mismatch");
        try {
            auto profile=profile_factory(request,identity,format,f);
            if(!profile||f.kind!=MovieSessionError::none)return policy_error(f,"movie service missing verified source profile");
            const auto& c=*profile;
            if((c.audio!=MoviePlaybackAudioPresence::required&&c.audio!=MoviePlaybackAudioPresence::absent)||
               (c.audio==MoviePlaybackAudioPresence::required&&(!format||!c.pcm||!c.audio_enabled))||
               (c.audio==MoviePlaybackAudioPresence::absent&&(format||c.pcm||c.audio_enabled)))return policy_error(f,"movie service source audio presence/profile mismatch");
            std::unique_ptr<NativeMoviePcmBinding> pcm;
            if(c.pcm){if(c.pcm->source_key!=request.logical_key||!format||c.pcm->client_format!=*format||c.audio_source_rate!=c.pcm->source_sample_rate)return policy_error(f,"movie service actual client/source PCM profile mismatch");MoviePcmBindingFailure why;pcm=NativeMoviePcmBinding::create(*c.pcm,why);if(!pcm){f.control={MovieControlError::native_service,why.operation,static_cast<std::int64_t>(why.kind)};return policy_error(f,"movie service PCM binding");}}
            MovieServiceClockFailure clock_failure;auto clock=NativeMovieServiceClock::create(c.service_clock,clock_failure);if(!clock){f.control={MovieControlError::native_service,"movie service source clock configuration",static_cast<std::int64_t>(clock_failure.kind)};return policy_error(f,"movie service source clock");}
            MovieControlFailure control;auto binding=MovieEligibilityServiceBinding::create(request.logical_key,std::move(*clock),control);if(!binding){f.control=control;return policy_error(f,"movie service shared source clock");}
            auto context=NativeMoviePlaybackContext::create({request,identity,binding,c.filter,c.target_route,c.completion,c.video_enabled,c.audio_enabled,c.preserve_queued_temporal,c.target_offset,c.audio_source_origin,c.audio_reset_epoch,c.audio_source_rate},control);
            if(!context){f.control=control;return policy_error(f,"movie service concrete source context");}auto* raw=context.get();
            auto kernel=NativeMovieKernelPolicies::create(request,c.audio,std::move(pcm),c.eligibility,std::move(context),binding,control);
            if(!kernel){f.control=control;return policy_error(f,"movie service concrete kernel policies");}
            MovieOutputPositionFailure position_failure;auto position=NativeMovieOutputPosition::create(request.logical_key,identity,c.position_profile,position_failure);
            if(!position){f.control={MovieControlError::native_service,position_failure.operation,static_cast<std::int64_t>(position_failure.kind)};return policy_error(f,"movie service output-position profile");}
            MovieAdmissionFeedFailure feed_failure;auto feed=NativeMovieAdmissionFeed::create(request.logical_key,identity,0,feed_failure);
            if(!feed){f.control={MovieControlError::native_service,"movie service source feed creation",static_cast<std::int64_t>(feed_failure.kind)};return policy_error(f,"movie service source feed creation");}
            auto guarded=std::unique_ptr<MoviePlaybackPolicies>(new LeasedPolicies(std::move(kernel),raw,port));
            port->identity=std::move(identity);port->clock=std::move(binding);port->position=std::move(position);port->feed=std::move(feed);return guarded;
        }catch(const std::bad_alloc&){return policy_error(f,"movie service policy allocation",MovieSessionError::allocation);}
         catch(...){return policy_error(f,"movie service profile factory exception");}
    };
}
}
NativeMovieService::NativeMovieService(float volume,MovieServiceProfileFactory profile,MovieServiceSessionFactory opener):settings_(volume),profile_factory_(std::move(profile)),session_factory_(std::move(opener)){state_.base_volume=volume;}
std::unique_ptr<NativeMovieService> NativeMovieService::create(float base,MovieServiceProfileFactory profile,MovieServiceSessionFactory opener,MovieServiceFailure& f) noexcept {
    f={};if(!std::isfinite(base)){fail(f,MovieServiceError::profile,"movie service nonfinite base setting");return {};}
    if(!profile||!opener){fail(f,MovieServiceError::missing_factory,"movie service requires actual source/profile factories");return {};}
    try{return std::unique_ptr<NativeMovieService>(new NativeMovieService(base,std::move(profile),std::move(opener)));}catch(const std::bad_alloc&){fail(f,MovieServiceError::allocation,"movie service allocation");return {};}
}
bool NativeMovieService::access(MovieServiceFailure& f) const noexcept {f={};if(std::this_thread::get_id()!=owner_)return fail(f,MovieServiceError::wrong_thread,"movie service owner thread");if(in_call_)return fail(f,MovieServiceError::invalid_state,"movie service reentry");return true;}
bool NativeMovieService::event_access(const MovieServiceRequestIdentity& id,MovieServiceFailure& f) const noexcept {
    if(!access(f))return false;
    if(!session_||!state_.active||!port_||!port_->context)return fail(f,MovieServiceError::invalid_state,"movie service has no active source context");
    if(id.generation!=state_.generation||id.source_key!=port_->key||!id.source||id.source!=port_->identity)return fail(f,MovieServiceError::foreign_request,"movie service stale/foreign request event");return true;
}
bool NativeMovieService::set_base_volume(float volume,MovieServiceFailure& f) noexcept {if(!access(f))return false;Guard call(in_call_);if(!std::isfinite(volume))return fail(f,MovieServiceError::profile,"movie service nonfinite base setting");settings_.set_base_volume(volume);state_.base_volume=volume;return true;}
bool NativeMovieService::install_skip_if_present(std::function<std::int64_t()> callback,MovieServiceFailure& f) noexcept {if(!access(f))return false;Guard call(in_call_);settings_.install_skip_if_present(std::move(callback));return true;}
bool NativeMovieService::start(std::int64_t id,MovieViewSelection view,std::int64_t language,std::uint64_t fade,MovieServiceFailure& f) noexcept {
    if(!access(f))return false;Guard call(in_call_);
    if(session_||state_.active)return fail(f,MovieServiceError::invalid_state,"movie service single active request");
    if(state_.generation==UINT64_MAX)return fail(f,MovieServiceError::overflow,"movie service request generation overflow");
    MovieCatalogFailure cf;auto request=RetailMovieCatalog{}.resolve(id,view,language,cf);if(!request||request->route!=MovieRoute::blocking)return fail(f,MovieServiceError::invalid_request,"movie service invalid or unsupported retail request");
    try {
        const auto generation=state_.generation+1;MovieServiceSnapshot staged;staged.generation=generation;staged.request=*request;staged.base_volume=settings_.current_base_volume();staged.activity=MovieServiceActivity::starting;
        auto port=std::make_shared<MovieServicePort>();port->generation=generation;port->key=request->logical_key;state_=std::move(staged);port_=port;
        auto factory=policies_for(port,*request,profile_factory_);
        session_=session_factory_(*request,language,std::move(factory),settings_,f.session);
        if(!session_||f.session.kind!=MovieSessionError::none||port->factory_calls!=1||!port->context||!port->identity||!port->position||!port->feed){fail(f,MovieServiceError::session,"movie service actual session/policy construction");terminal_failure(f);return false;}
        MovieSessionFailure query;const auto* actual=session_->request(query);
        if(!actual||query.kind!=MovieSessionError::none||!same_request(*actual,*request)){f.session=query;fail(f,MovieServiceError::session,"movie service actual session request mismatch");terminal_failure(f);return false;}
        port->opening=false;
        if(!session_->start(fade,{},f.session)){fail(f,MovieServiceError::session,"movie service actual session start");terminal_failure(f);return false;}
        state_.active=true;state_.activity=MovieServiceActivity::preparing;return true;
    }catch(const std::bad_alloc&){fail(f,MovieServiceError::allocation,"movie service request allocation");terminal_failure(f);return false;}
     catch(...){fail(f,MovieServiceError::session,"movie service acquisition factory exception");terminal_failure(f);return false;}
}
bool NativeMovieService::close_owned(MovieServiceFailure& f) noexcept {
    bool ok=true;
    if(session_){
        MoviePlaybackSnapshot actual;MovieSessionFailure query;
        state_.playback_retained=true;
        if(session_->snapshot(actual,query)){state_.playback=std::move(actual);state_.playback_snapshot_failure.reset();}
        else {state_.playback.reset();state_.playback_snapshot_failure=observation_error(query);f.observation=*state_.playback_snapshot_failure;f.session=std::move(query);fail(f,MovieServiceError::session,"movie service pre-teardown playback observation");ok=false;}
    }
    if(port_)port_->invalidate(); // Before any source/policy/context destruction.
    if(session_){MovieSessionFailure cleanup;const bool closed=session_->close(cleanup);
        if(!closed){f.cleanup=std::move(cleanup);if(ok)fail(f,MovieServiceError::session,"movie service resource cleanup");ok=false;}
        session_.reset();
    }
    port_.reset();state_.active=false;return ok;
}
void NativeMovieService::terminal_failure(MovieServiceFailure& f) noexcept {
    auto primary=control_error(f);MovieServiceFailure cleanup;
    close_owned(cleanup);f.observation=cleanup.observation;f.cleanup=std::move(cleanup.cleanup);
    state_.terminal=MovieControlResult{MovieControlProgress::error,MovieControlCompletion::none,primary,f.cleanup.kind==MovieSessionError::none?MovieControlFailure{}:observation_error(f.cleanup)};state_.activity=MovieServiceActivity::error;
}
MovieControlResult NativeMovieService::poll(MovieServiceFailure& f) noexcept {
    if(!access(f))return {MovieControlProgress::error,MovieControlCompletion::none,control_error(f),{}};Guard call(in_call_);
    if(!session_){if(state_.terminal)return *state_.terminal;fail(f,MovieServiceError::invalid_state,"movie service poll without request");return {MovieControlProgress::error,MovieControlCompletion::none,control_error(f),{}};}
    if(state_.polls==UINT64_MAX){fail(f,MovieServiceError::overflow,"movie service poll count overflow");terminal_failure(f);return *state_.terminal;}++state_.polls;
    auto result=session_->poll();if(result.progress==MovieControlProgress::ongoing)return result;
    if(port_&&port_->feed_failure.kind!=MovieAdmissionFeedError::none){f.kind=MovieServiceError::source_feed;f.operation="movie service source observation feed failure";f.feed=port_->feed_failure;f.control=result.failure;}
    MovieServiceFailure cleanup;close_owned(cleanup);
    if(cleanup.kind!=MovieServiceError::none){
        const auto observation=cleanup.observation;
        const bool actual_cleanup_failed=cleanup.cleanup.kind!=MovieSessionError::none;
        const auto actual_cleanup=actual_cleanup_failed?observation_error(cleanup.cleanup):MovieControlFailure{};
        const auto first=observation.kind!=MovieControlError::none?observation:actual_cleanup;
        if(f.kind==MovieServiceError::none)f=std::move(cleanup);
        else{f.observation=observation;f.cleanup=std::move(cleanup.cleanup);}
        if(actual_cleanup_failed)result.cleanup_failure=actual_cleanup;
        // Observation failure is never relabeled as actual cleanup, and cannot
        // replace an existing playback failure. A completed control operation
        // may fail its final observation while retaining its completion reason.
        if(result.progress!=MovieControlProgress::error){result.progress=MovieControlProgress::error;result.failure=first;}
    }
    state_.terminal=result;state_.activity=result.progress==MovieControlProgress::error?MovieServiceActivity::error:MovieServiceActivity::complete;
    if(result.progress==MovieControlProgress::error&&f.kind==MovieServiceError::none){f.kind=MovieServiceError::session;f.operation="movie service terminal playback error";f.control=result.failure;}
    return result;
}
bool NativeMovieService::stop(MovieServiceFailure& f) noexcept {if(!access(f))return false;Guard call(in_call_);if(!session_)return true;if(!session_->stop(f.session)){fail(f,MovieServiceError::session,"movie service source stop");terminal_failure(f);return false;}state_.activity=MovieServiceActivity::stopping;return true;}
bool NativeMovieService::close(MovieServiceFailure& f) noexcept {if(!access(f))return false;Guard call(in_call_);const bool ok=close_owned(f);state_.activity=ok?MovieServiceActivity::closed:MovieServiceActivity::error;return ok;}
NativeMovieService::~NativeMovieService(){if(std::this_thread::get_id()!=owner_||in_call_)std::terminate();in_call_=true;MovieServiceFailure ignored;close_owned(ignored);}
std::optional<MovieServiceRequestIdentity> NativeMovieService::request_identity(MovieServiceFailure& f) const noexcept {if(!access(f))return {};Guard call(in_call_);if(!state_.active||!port_||!port_->context){fail(f,MovieServiceError::invalid_state,"movie service no live request identity");return {};}try{return MovieServiceRequestIdentity{state_.generation,port_->key,port_->identity};}catch(const std::bad_alloc&){fail(f,MovieServiceError::allocation,"movie service identity allocation");return {};}}
std::optional<MovieServiceSnapshot> NativeMovieService::snapshot(MovieServiceFailure& f) const noexcept {if(!access(f))return {};Guard call(in_call_);try{auto result=state_;result.base_volume=settings_.current_base_volume();if(session_){MoviePlaybackSnapshot actual;if(!session_->snapshot(actual,f.session)){f.observation=observation_error(f.session);fail(f,MovieServiceError::session,"movie service live playback observation");return {};}result.playback=std::move(actual);result.playback_retained=false;result.playback_snapshot_failure.reset();}if(port_&&port_->context){MovieControlFailure why;result.context=port_->context->snapshot(why);if(!result.context){f.control=why;fail(f,MovieServiceError::context,"movie service context snapshot");return {};}}return result;}catch(const std::bad_alloc&){fail(f,MovieServiceError::allocation,"movie service snapshot allocation");return {};}}
std::optional<MovieAdmissionCandidate> NativeMovieService::parse_candidate(const MovieServiceRequestIdentity& id,MovieServiceFailure& f) noexcept {
    if(!event_access(id,f))return {};Guard call(in_call_);
    auto result=port_->feed->parse_next(f.feed);
    if(!result){fail(f,MovieServiceError::source_feed,"movie service parse source candidate");terminal_failure(f);}return result;
}
std::optional<MoviePictureBodyExtent> NativeMovieService::picture_body_extent(const MovieServiceRequestIdentity& id,MovieServiceFailure& f) noexcept {
    if(!event_access(id,f))return {};Guard call(in_call_);
    auto result=port_->feed->picture_body_extent(f.feed);
    if(!result){fail(f,MovieServiceError::source_feed,"movie service source picture body extent");terminal_failure(f);}return result;
}
std::optional<MovieHeaderBoundaryProposal> NativeMovieService::boundary_proposal(const MovieServiceRequestIdentity& id,MovieServiceFailure& f) noexcept {
    if(!event_access(id,f))return {};Guard call(in_call_);
    auto result=port_->feed->proposal(f.feed);
    if(!result){fail(f,MovieServiceError::source_feed,"movie service source boundary proposal");terminal_failure(f);}return result;
}
bool NativeMovieService::skip_source_bytes(const MovieServiceRequestIdentity& id,std::uint64_t amount,MovieServiceFailure& f) noexcept {
    if(!event_access(id,f))return false;Guard call(in_call_);
    if(!port_->feed->skip(amount,f.feed)){fail(f,MovieServiceError::source_feed,"movie service actual source byte skip");terminal_failure(f);return false;}return true;
}
std::optional<MovieAdmissionFeedSnapshot> NativeMovieService::source_feed_snapshot(const MovieServiceRequestIdentity& id,MovieServiceFailure& f) const noexcept {
    if(!event_access(id,f))return {};Guard call(in_call_);
    auto result=port_->feed->snapshot(f.feed);if(!result)fail(f,MovieServiceError::source_feed,"movie service source feed snapshot");return result;
}
bool NativeMovieService::admit_header(const MovieServiceRequestIdentity& id,const MovieNativeHeaderAdmission& event,MovieServiceFailure& f) noexcept {
    if(!event_access(id,f))return false;Guard call(in_call_);
    try{std::map<std::uint64_t,std::shared_ptr<const MovieAdmissionCurrent>> staged;staged.emplace(event.admission_id,event.current);
        if(!port_->context->admit_header(event,f.control)){fail(f,MovieServiceError::context,"movie service source header admission");terminal_failure(f);return false;}port_->admissions.merge(staged);return true;
    }catch(const std::bad_alloc&){fail(f,MovieServiceError::allocation,"movie service admission ownership allocation");terminal_failure(f);return false;}
}
std::optional<MovieOutputPositionUpdate> NativeMovieService::work_owned(const MovieOutputPositionWork& work,std::optional<std::uint64_t> timing,const std::optional<MoviePictureSource>& selected,bool flush,MovieServiceFailure& f) noexcept {
    auto header=port_->admissions.find(work.admission_id);
    if(header==port_->admissions.end()||header->second!=work.current_header||(selected&&(!timing||!port_->admissions.contains(*timing)))){fail(f,MovieServiceError::context,"movie service work/timing admission mismatch");terminal_failure(f);return {};}
    if(!port_->context->validate_work(*work.current_header,f.control)){fail(f,MovieServiceError::context,"movie service current work provenance");terminal_failure(f);return {};}
    auto update=flush?port_->position->flush(port_->key,port_->identity,work,selected,f.position):port_->position->complete_work(port_->key,port_->identity,work,selected,f.position);
    if(!update){fail(f,MovieServiceError::position,"movie service source work publication");terminal_failure(f);return {};}
    MovieOutputPositionFailure why;state_.position=port_->position->snapshot(why);
    if(update->publication&&!port_->context->publish_output({work.admission_id,*timing,update->publication->selected,update->publication->value},f.control)){fail(f,MovieServiceError::context,"movie service work committed but context publication failed");terminal_failure(f);return {};}
    state_.activity=MovieServiceActivity::work;return update;
}
std::optional<MovieOutputPositionUpdate> NativeMovieService::complete_work(const MovieServiceRequestIdentity& id,const MovieOutputPositionWork& work,std::optional<std::uint64_t> timing,const std::optional<MoviePictureSource>& selected,MovieServiceFailure& f) noexcept {if(!event_access(id,f))return {};Guard call(in_call_);return work_owned(work,timing,selected,false,f);}
std::optional<MovieOutputPositionUpdate> NativeMovieService::flush_work(const MovieServiceRequestIdentity& id,const MovieOutputPositionWork& work,std::optional<std::uint64_t> timing,const std::optional<MoviePictureSource>& selected,MovieServiceFailure& f) noexcept {if(!event_access(id,f))return {};Guard call(in_call_);return work_owned(work,timing,selected,true,f);}
#define CONTEXT_EVENT(name,args,callargs) bool NativeMovieService::name args noexcept {if(!event_access(id,f))return false;Guard call(in_call_);if(!port_->context->name callargs){fail(f,MovieServiceError::context,"movie service source " #name);terminal_failure(f);return false;}return true;}
CONTEXT_EVENT(advance_position_origin,(const MovieServiceRequestIdentity& id,std::uint64_t p,MovieServiceFailure& f),(p,f.control))
CONTEXT_EVENT(maintenance,(const MovieServiceRequestIdentity& id,MovieServiceMaintenanceGates gates,MovieServiceFailure& f),(gates,f.control))
CONTEXT_EVENT(increment_service,(const MovieServiceRequestIdentity& id,MovieClockRational r,MovieServiceFailure& f),(r,f.control))
CONTEXT_EVENT(sample_service,(const MovieServiceRequestIdentity& id,bool admitted,MovieServiceFailure& f),(admitted,f.control))
CONTEXT_EVENT(sample_audio,(const MovieServiceRequestIdentity& id,MovieClockRational audio,MovieClockRational timer,bool admitted,bool active,MovieServiceFailure& f),(audio,timer,admitted,active,f.control))
CONTEXT_EVENT(select_direct_control,(const MovieServiceRequestIdentity& id,MovieServiceFailure& f),(f.control))
CONTEXT_EVENT(command,(const MovieServiceRequestIdentity& id,MovieNativeCommand command,MovieServiceFailure& f),(command,f.control))
#undef CONTEXT_EVENT
} // namespace mka::video
