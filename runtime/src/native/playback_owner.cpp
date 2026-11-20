#include "mka/playback_owner.hpp"
#include <mutex>
#include <limits>
#include <unordered_map>
#include <type_traits>
#include <new>
#include <stdexcept>
namespace mka::sound {
struct NativePlaybackOwner::State {
    struct Entry {
        BankWireToken bank;
        NativePlayParameters parameters;
        std::shared_ptr<const void> context;
        std::optional<ProgramExecutor> executor;
        bool preparing=true;
        std::vector<HostVoice> voices;
        bool finished=false,stopped=false;
        std::optional<PlaybackFailure> terminal;
        Entry(BankWireToken b,const NativePlayParameters& p,std::shared_ptr<const void> c):bank(b),parameters(p),context(std::move(c)){}
        bool busy()const {if(preparing||(!finished&&!stopped))return true;for(const auto& v:voices)if(v.active())return true;return false;}
        bool stop_voices(const ProgramStep& event,PlaybackFailure& f){
            for(auto& v:voices)if(!v.stop_from_program(event,f.voice)){f.kind=PlaybackError::voice_failed;return false;}
            return true;
        }
    };
    struct Slot {std::shared_ptr<const void> identity;std::shared_ptr<Entry> entry;};
    mutable std::mutex mutex;
    AudioBankService service;
    std::shared_ptr<const void> identity;
    std::uint32_t rate;
    bool closed=false,closing=false,close_unknown=false;
    std::size_t operations=0;
    std::vector<Slot> slots;
    struct Context {std::shared_ptr<const void> identity;bool live=true;};
    std::vector<Context> contexts;
    std::unordered_map<std::uint32_t,bool> unloading; // true: exceptional result, exact bank admission stays closed.
    struct Activity {
        std::shared_ptr<State> state;
        std::optional<std::uint32_t> bank;
        std::shared_ptr<const void> reservation;
        bool closes=false,uncertain=false;
        Activity()=default;
        Activity(const Activity&)=delete;
        Activity& operator=(const Activity&)=delete;
        ~Activity(){
            if(!state)return;
            std::lock_guard lock(state->mutex);
            if(reservation)for(auto it=state->slots.begin();it!=state->slots.end();++it)if(it->identity==reservation&&it->entry&&it->entry->preparing){state->slots.erase(it);break;}
            if(bank){auto found=state->unloading.find(*bank);if(found!=state->unloading.end()){
                if(uncertain)found->second=true;else state->unloading.erase(found);
            }}
            if(closes&&!state->closed){if(uncertain)state->close_unknown=true;else state->closing=false;}
            --state->operations;
        }
    };
    bool admit(const std::shared_ptr<State>& self,Activity& work,PlaybackFailure& f){
        if(!available(f))return false;
        if(operations==std::numeric_limits<std::size_t>::max()){f.kind=PlaybackError::host_capacity;return false;}
        ++operations;work.state=self;return true;
    }
    Context* context(const NativeSoundContext& c,PlaybackFailure& f){
        if(!c.owner_||!c.identity_){f.kind=PlaybackError::unknown_context;return nullptr;}
        if(c.owner_!=identity){f.kind=PlaybackError::foreign_context;return nullptr;}
        for(auto& item:contexts)if(item.identity==c.identity_){
            if(!item.live){f.kind=PlaybackError::retired_context;return nullptr;}return &item;
        }
        f.kind=PlaybackError::unknown_context;return nullptr;
    }
    State(AudioBankService&& s,std::uint32_t r,std::shared_ptr<const void> id):service(std::move(s)),identity(std::move(id)),rate(r){}
    bool available(PlaybackFailure& f)const {if(closed){f.kind=PlaybackError::closed;return false;}if(close_unknown){f.kind=PlaybackError::close_state_unknown;return false;}if(closing){f.kind=PlaybackError::closing;return false;}return true;}
    Slot* find(const std::shared_ptr<const void>& owner,const std::shared_ptr<const void>& id,PlaybackFailure& f){
        if(!owner||!id){f.kind=PlaybackError::unknown_handle;return nullptr;}
        if(owner!=identity){f.kind=PlaybackError::foreign_owner;return nullptr;}
        for(auto& s:slots)if(s.identity==id){if(!s.entry){f.kind=PlaybackError::retired_handle;return nullptr;}return &s;}
        f.kind=PlaybackError::unknown_handle;return nullptr;
    }
};
namespace {
void allocation_failure(PlaybackFailure& f){f.kind=PlaybackError::allocation_failed;}
}
std::optional<NativePlaybackOwner> NativePlaybackOwner::create(AudioBankService&& s,std::uint32_t rate,PlaybackFailure& f){
    f={};if(!rate){f.kind=PlaybackError::invalid_output_rate;return std::nullopt;}
    try{auto id=std::make_shared<int>(0);return NativePlaybackOwner(std::make_shared<State>(std::move(s),rate,std::move(id)));}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return std::nullopt;
}
std::shared_ptr<AudioResourceDomain> NativePlaybackOwner::resource_domain() const noexcept {
    return state_?state_->service.resource_domain():nullptr;
}
std::optional<NativeSoundContext> NativePlaybackOwner::create_sound_context(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f))return {};
    try {
        auto id=std::make_shared<int>(0);state->contexts.push_back({id,true});
        NativeSoundContext result;result.owner_=state->identity;result.identity_=std::move(id);return result;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    return {};
}
std::optional<SoundContextObservation> NativePlaybackOwner::observe_sound_context(const NativeSoundContext& c,PlaybackFailure& f)const {
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f)||!state->context(c,f))return {};
    SoundContextObservation result;
    for(const auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_){
        const auto& e=*slot.entry;
        if(e.preparing)++result.pending_preparations;else ++result.programs;
        for(const auto& voice:e.voices)if(voice.active())++result.active_voices;
    }
    return result;
}
bool NativePlaybackOwner::release_sound_context(const NativeSoundContext& c,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    std::lock_guard lock(state->mutex);if(!state->available(f))return false;
    auto* context=state->context(c,f);if(!context)return false;
    for(const auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_&&slot.entry->busy()){
        f.kind=PlaybackError::active_playback;return false;
    }
    context->live=false;
    for(auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_)slot.entry.reset();
    return true;
}
std::optional<BankLoadOutcome> NativePlaybackOwner::load_sync(const AudioBankCommand& c,BankWireToken token,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return {};}
    try {auto r=state->service.load_sync(c,token,f.service);if(!r)f.kind=PlaybackError::service_failed;return r;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
std::optional<AsyncBankSubmission> NativePlaybackOwner::submit_async(const AudioBankCommand& c,BankWireToken token,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return {};}
    try {auto r=state->service.submit_async(c,token,f.service);if(!r)f.kind=PlaybackError::service_failed;return r;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
bool NativePlaybackOwner::run_one(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return false;}
    try {if(!state->service.run_one(f.service)){f.kind=PlaybackError::service_failed;return false;}return true;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return false;
}
std::optional<BankLoadCompletion> NativePlaybackOwner::poll_completion(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return {};}
    try {auto r=state->service.poll_completion(f.service);if(!r&&f.service.kind!=BankServiceError::none)f.kind=PlaybackError::service_failed;return r;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
std::optional<NativePlayHandle> NativePlaybackOwner::play(const NativeSoundContext& context,
    const AudioBankCommand& c,const NativePlayParameters& p,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;std::shared_ptr<State::Entry> entry;NativePlayHandle handle;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f)||!state->context(context,f))return {};
         if(state->unloading.contains(c.bank_token.value)){f.kind=state->unloading.at(c.bank_token.value)?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return {};}
         if(!state->admit(state,work,f))return {};
         State::Slot slot;slot.identity=std::make_shared<int>(0);slot.entry=std::make_shared<State::Entry>(c.bank_token,p,context.identity_);
         entry=slot.entry;handle.owner_=state->identity;handle.identity_=slot.identity;
         state->slots.push_back(std::move(slot));work.reservation=handle.identity_;
        }
        // No registry lock spans resource construction/publication or callbacks.
        auto program=state->service.prepare_program(c,f.preparation,f.service);
        if(!program){f.kind=PlaybackError::service_failed;return {};}
        auto executor=ProgramExecutor::create(std::move(program),p.initial_deadline,f.program);
        if(!executor){f.kind=PlaybackError::program_failed;return {};}
        {std::lock_guard lock(state->mutex);
         // Re-find no vector location: the strong reservation is the commit owner.
         if(!state->available(f)||!state->context(context,f))return {};
         entry->executor.emplace(std::move(*executor));entry->preparing=false;work.reservation.reset();
        }
        return handle;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
std::optional<ProgramStep> NativePlaybackOwner::tick(const NativePlayHandle& h,ProgramClockWord now,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return std::nullopt;auto& e=*slot->entry;
    if(e.terminal){f=*e.terminal;return std::nullopt;}
    auto event=e.executor->step(now,f.program);if(!event){f.kind=PlaybackError::program_failed;e.terminal=f;return std::nullopt;}
    if(event->kind==ProgramStepKind::sample_start){
        auto voice=HostVoice::start(*event,e.parameters.context,e.parameters.program,e.parameters.source_output_mode,e.parameters.envelope,state_->rate,f.voice);
        if(!voice){f.kind=PlaybackError::voice_failed;e.executor->acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;}
        try{e.voices.push_back(std::move(*voice));}
        catch(const std::bad_alloc&){allocation_failure(f);e.executor->acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;}
        catch(const std::length_error&){f.kind=PlaybackError::host_capacity;e.executor->acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;}
        if(!e.executor->acknowledge_start(true,f.program)){e.voices.pop_back();f.kind=PlaybackError::program_failed;e.terminal=f;return std::nullopt;}
    }else if(event->kind==ProgramStepKind::program_stop){
        if(!e.stop_voices(*event,f)){e.terminal=f;return std::nullopt;}e.finished=true;
    }else if(event->kind==ProgramStepKind::structural_end||event->kind==ProgramStepKind::finished)e.finished=true;
    return event;
}
bool NativePlaybackOwner::stop(const NativePlayHandle& h,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return false;auto& e=*slot->entry;
    auto event=e.executor->stop_from_owner(f.program);if(!event){f.kind=PlaybackError::program_failed;return false;}
    if(!e.stop_voices(*event,f))return false;e.stopped=true;return true;
}
std::optional<std::vector<float>> NativePlaybackOwner::mix(std::size_t frames,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    try{std::vector<HostVoice*> voices;
        for(auto& slot:state_->slots)if(slot.entry){auto& e=*slot.entry;if(e.preparing)continue;
            if(e.terminal&&!e.stopped){f=*e.terminal;f.kind=PlaybackError::failed_playback_requires_stop;return std::nullopt;}
            for(auto& voice:e.voices)voices.push_back(&voice);
        }
        auto output=mix_host_voices(voices.data(),voices.size(),frames,f.voice);if(!output)f.kind=PlaybackError::voice_failed;return output;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return std::nullopt;
}
std::optional<PlaybackObservation> NativePlaybackOwner::observe(const NativePlayHandle& h,PlaybackFailure& f)const{
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return std::nullopt;const auto& e=*slot->entry;
    PlaybackObservation o;o.deadline=e.executor->deadline();o.command_index=e.executor->command_index();o.voices=e.voices.size();if(!e.voices.empty())o.first_voice_source_index=e.voices[0].source_index();for(const auto& v:e.voices)if(v.active())++o.active_voices;o.executor_finished=e.finished;o.explicitly_stopped=e.stopped;o.terminal_failure=e.terminal;return o;
}
bool NativePlaybackOwner::release(const NativePlayHandle& h,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return false;if(slot->entry->busy()){f.kind=PlaybackError::active_playback;return false;}slot->entry.reset();return true;
}
bool NativePlaybackOwner::unload(const AudioBankCommand& c,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f))return false;
         if(state->unloading.contains(c.bank_token.value)){f.kind=state->unloading.at(c.bank_token.value)?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return false;}
         for(const auto& slot:state->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value&&slot.entry->busy()){
             f.kind=PlaybackError::active_playback;return false;
         }
         if(!state->admit(state,work,f))return false;
         state->unloading.emplace(c.bank_token.value,false);work.bank=c.bank_token.value;
        }
        if(!state->service.unload(c,f.service)){f.kind=PlaybackError::service_failed;return false;}
        {std::lock_guard lock(state->mutex);
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value)slot.entry.reset();
        }
        return true;
    }catch(const std::bad_alloc&){work.uncertain=true;allocation_failure(f);}catch(const std::length_error&){work.uncertain=true;f.kind=PlaybackError::host_capacity;}
    catch(...){work.uncertain=true;f.kind=PlaybackError::native_exception;}return false;
}
bool NativePlaybackOwner::close_when_idle(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;
    {std::lock_guard lock(state->mutex);
     if(!state->available(f))return false;
     if(state->operations){f.kind=PlaybackError::operation_active;return false;}
     for(const auto& slot:state->slots)if(slot.entry&&slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}
     if(!state->admit(state,work,f))return false;
     state->closing=true;work.closes=true;
    }
    try {
        if(!state->service.close_when_idle(f.service)){f.kind=PlaybackError::service_failed;return false;}
        {std::lock_guard lock(state->mutex);state->closed=true;state->closing=false;}
        return true;
    }catch(const std::bad_alloc&){work.uncertain=true;allocation_failure(f);}catch(const std::length_error&){work.uncertain=true;f.kind=PlaybackError::host_capacity;}
    catch(...){work.uncertain=true;f.kind=PlaybackError::native_exception;}return false;
}
static_assert(std::is_nothrow_move_constructible_v<ProgramExecutor>);
} // namespace mka::sound
