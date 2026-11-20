#include "mka/movie_service_host_clock.hpp"
#include <limits>
#include <type_traits>
namespace mka::video {
namespace {
struct Guard {bool& b;explicit Guard(bool& v):b(v){b=true;}~Guard(){b=false;}};
bool fail(MovieServiceHostClockFailure& f,MovieServiceHostClockError e) noexcept {f.kind=e;return false;}
bool same(const MovieServiceRequestIdentity& a,const MovieServiceRequestIdentity& b) noexcept{return a.generation==b.generation&&a.source_key==b.source_key&&a.source==b.source;}
static_assert(std::is_nothrow_move_assignable_v<MovieServiceFailure>);
}
NativeMovieServiceHostClock::NativeMovieServiceHostClock(NativeMovieService& s,MovieServiceRequestIdentity id,MovieServiceHostClockPolicy p,bool a,std::unique_ptr<NativeMovieHostClock> c):service_(s),identity_(std::move(id)),policy_(p),admitted_(a),clock_(std::move(c)){}
NativeMovieServiceHostClock::~NativeMovieServiceHostClock(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();}
bool NativeMovieServiceHostClock::access(MovieServiceHostClockFailure& f)const noexcept{f={};if(owner_!=std::this_thread::get_id())return fail(f,MovieServiceHostClockError::wrong_thread);if(busy_)return fail(f,MovieServiceHostClockError::reentry);if(state_.failed)return fail(f,MovieServiceHostClockError::invalid_state);return true;}
bool NativeMovieServiceHostClock::terminate(MovieServiceHostClockFailure& f) noexcept{state_.failed=true;MovieServiceFailure query;auto actual=service_.request_identity(query);if(actual&&same(*actual,identity_)){MovieServiceFailure cleanup;service_.close(cleanup);f.cleanup=std::move(cleanup);}return false;}
std::optional<MovieServiceHostClockSnapshot> NativeMovieServiceHostClock::snapshot(MovieServiceHostClockFailure& f) const noexcept{if(!access(f))return {};return state_;}
std::unique_ptr<NativeMovieServiceHostClock> NativeMovieServiceHostClock::create(NativeMovieService& s,MovieServiceRequestIdentity id,MovieServiceHostClockPolicy p,bool a,MovieServiceHostClockFailure& f) noexcept{
    f={};auto clock=NativeMovieHostClock::create(id.source_key,id.source,f.host);if(!clock){fail(f,MovieServiceHostClockError::host);return {};}
    return create_with_clock(s,std::move(id),p,a,std::move(clock),f);
}
std::unique_ptr<NativeMovieServiceHostClock> NativeMovieServiceHostClock::create_with_clock(NativeMovieService& s,MovieServiceRequestIdentity id,MovieServiceHostClockPolicy p,bool a,std::unique_ptr<NativeMovieHostClock> c,MovieServiceHostClockFailure& f) noexcept{
    f={};if(!c||id.source_key.empty()||!id.source||(p!=MovieServiceHostClockPolicy::elapsed_service&&p!=MovieServiceHostClockPolicy::anchored_audio)){fail(f,MovieServiceHostClockError::invalid_owner);return {};}
    try{return std::unique_ptr<NativeMovieServiceHostClock>(new NativeMovieServiceHostClock(s,std::move(id),p,a,std::move(c)));}catch(const std::bad_alloc&){fail(f,MovieServiceHostClockError::allocation);return {};}
}
bool NativeMovieServiceHostClock::begin(MovieServiceHostClockFailure& f) noexcept{
    if(!access(f))return false;Guard guard(busy_);if(state_.begun)return fail(f,MovieServiceHostClockError::invalid_state);
    auto actual=service_.request_identity(f.service);if(!actual||!same(*actual,identity_)){fail(f,MovieServiceHostClockError::foreign_source);return terminate(f);}
    auto s=service_.snapshot(f.service);if(!s||!s->active||!s->context||!s->playback||s->playback_retained){fail(f,MovieServiceHostClockError::unavailable);return terminate(f);}
    frequency_=s->context->service.frequency;baseline_=expected_=s->context->service.counter;
    if(frequency_<=0){fail(f,MovieServiceHostClockError::baseline_changed);return terminate(f);}
    if(!clock_->begin(identity_.source_key,identity_.source,f.host)){fail(f,MovieServiceHostClockError::host);return terminate(f);}
    paused_=s->playback->paused;if(paused_&&!clock_->pause(identity_.source_key,identity_.source,f.host)){fail(f,MovieServiceHostClockError::host);return terminate(f);}
    state_.service_counter=baseline_;state_.begun=true;return true;
}
bool NativeMovieServiceHostClock::update(const std::optional<MovieAnchoredAudioClockSample>& audio,MovieServiceHostClockFailure& f) noexcept{
    if(!access(f))return false;Guard guard(busy_);return update_owned(audio,f);
}
bool NativeMovieServiceHostClock::update_from_output(MovieServiceHostClockFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    auto actual=service_.snapshot(f.service);if(!actual){fail(f,MovieServiceHostClockError::service);return terminate(f);}
    std::optional<MovieAnchoredAudioClockSample> sample;
    try {if(actual->playback)sample=actual->playback->audio_time;}catch(const std::bad_alloc&){fail(f,MovieServiceHostClockError::allocation);return terminate(f);}
    if(policy_==MovieServiceHostClockPolicy::anchored_audio&&actual->active&&sample&&
       (sample->cancelled||!sample->output_clock||!actual->playback->audio_queue||sample->output_clock->reset_epoch!=actual->playback->audio_queue->reset_epoch)){
        fail(f,MovieServiceHostClockError::invalid_sample);return terminate(f);
    }
    return update_owned(sample,f);
}
bool NativeMovieServiceHostClock::update_owned(const std::optional<MovieAnchoredAudioClockSample>& audio,MovieServiceHostClockFailure& f) noexcept {
    if(!state_.begun||state_.stopped)return fail(f,MovieServiceHostClockError::invalid_state);
    auto s=service_.snapshot(f.service);if(!s){fail(f,MovieServiceHostClockError::service);return terminate(f);}
    if(s->generation!=identity_.generation||!s->request||s->request->logical_key!=identity_.source_key){fail(f,MovieServiceHostClockError::foreign_source);return terminate(f);}
    if(!s->active||s->playback_retained){if(!clock_->stop(identity_.source_key,identity_.source,f.host)){fail(f,MovieServiceHostClockError::host);return terminate(f);}auto final=clock_->read(identity_.source_key,identity_.source,f.host);if(!final){fail(f,MovieServiceHostClockError::host);return terminate(f);}state_.elapsed=final->elapsed;state_.stopped=true;return true;}
    auto actual=service_.request_identity(f.service);if(!actual||!same(*actual,identity_)){fail(f,MovieServiceHostClockError::foreign_source);return terminate(f);}
    if(!s->playback||!s->context){fail(f,MovieServiceHostClockError::unavailable);return terminate(f);}
    if(s->context->service.frequency!=frequency_||s->context->service.counter!=expected_){fail(f,MovieServiceHostClockError::baseline_changed);return terminate(f);}
    if(s->playback->paused!=paused_){const bool ok=s->playback->paused?clock_->pause(identity_.source_key,identity_.source,f.host):clock_->resume(identity_.source_key,identity_.source,f.host);if(!ok){fail(f,MovieServiceHostClockError::host);return terminate(f);}paused_=s->playback->paused;}
    auto timer=clock_->read(identity_.source_key,identity_.source,f.host);if(!timer){fail(f,MovieServiceHostClockError::host);return terminate(f);}
    const auto elapsed=timer->elapsed;
    if(policy_==MovieServiceHostClockPolicy::elapsed_service){
        const __int128 projected=(__int128(frequency_)*elapsed.numerator)/elapsed.denominator;
        const __int128 next=__int128(baseline_)+projected,delta=projected-projected_;
        if(projected<0||next<INT64_MIN||next>INT64_MAX||delta<0){fail(f,MovieServiceHostClockError::overflow);return terminate(f);}
        if(delta&&!service_.increment_service_ticks(identity_,expected_,delta,f.service)){fail(f,MovieServiceHostClockError::service);return terminate(f);}
        if(!service_.sample_service(identity_,admitted_,f.service)){fail(f,MovieServiceHostClockError::service);return terminate(f);}
        expected_=std::int64_t(next);projected_=projected;state_.service_counter=expected_;
    }else{
        if(!audio){fail(f,MovieServiceHostClockError::unavailable);return terminate(f);}
        if(audio->source_key!=identity_.source_key||audio->source!=identity_.source||!audio->anchor||(anchor_&&anchor_!=audio->anchor)){fail(f,MovieServiceHostClockError::foreign_source);return terminate(f);}
        if(audio->cancelled||audio->elapsed.denominator<=0||audio->elapsed.numerator<0){fail(f,MovieServiceHostClockError::invalid_sample);return terminate(f);}
        if(last_audio_&&__int128(audio->elapsed.numerator)*last_audio_->denominator<__int128(last_audio_->numerator)*audio->elapsed.denominator){fail(f,MovieServiceHostClockError::regression);return terminate(f);}
        if(!service_.sample_audio(identity_,audio->elapsed,elapsed,audio->query_admitted,audio->active,f.service)){fail(f,MovieServiceHostClockError::service);return terminate(f);}
        anchor_=audio->anchor;last_audio_=audio->elapsed;
    }
    state_.elapsed=elapsed;return true;
}
} // namespace mka::video
