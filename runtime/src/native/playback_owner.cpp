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
        std::unique_ptr<NativeProgramCopy> copy;
        NativeProgramOutputObservation retired_outputs;
        std::uint16_t effective_play_flags=0;
        bool finished=false,stopped=false,resources_retired=false;
        std::optional<PlaybackFailure> terminal;
        Entry(BankWireToken b,const NativePlayParameters& p,std::shared_ptr<const void> c):bank(b),parameters(p),context(std::move(c)){}
        ~Entry(){copy.reset();executor.reset();}
        NativeProgramOutputObservation outputs()const noexcept{return copy?copy->output_observation():retired_outputs;}
        bool busy()const {return preparing||(!finished&&!stopped)||outputs().active_voices!=0;}
        bool stop_voices(const ProgramStep& event,PlaybackFailure& f){
            if(!copy)return resources_retired&&(finished||stopped);
            if(copy->stop_outputs(event,f.copy))return true;
            // The Owner API remains idempotent after an actual successful
            // mode0 stop. The Copy API still diagnoses duplicate cleanup.
            if(f.copy.kind==ProgramCopyError::stopped_copy&&(finished||stopped)){f.copy={};return true;}
            f.kind=f.copy.kind==ProgramCopyError::voice_failed?PlaybackError::voice_failed:PlaybackError::copy_failed;
            f.voice=f.copy.voice;return false;
        }
        std::unique_ptr<NativeProgramCopy> retire_copy(){
            if(copy){retired_outputs=copy->output_observation();resources_retired=true;}
            return std::move(copy);
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
        result.active_voices+=e.outputs().active_voices;
    }
    return result;
}
bool NativePlaybackOwner::release_sound_context(const NativeSoundContext& c,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;std::vector<std::shared_ptr<State::Entry>> retired;
    try {
        {std::lock_guard lock(state->mutex);if(!state->available(f))return false;
         auto* context=state->context(c,f);if(!context)return false;
         std::size_t count=0;
         for(const auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_){
             if(slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}++count;
         }
         retired.reserve(count);if(!state->admit(state,work,f))return false;
         context->live=false;
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_)retired.push_back(std::move(slot.entry));
        }
        // Service keepers are released outside the registry lock, while the
        // Activity prevents close from crossing this real disposal boundary.
        retired.clear();return true;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    return false;
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
        auto reference=state->service.acquire_runtime_program(c,f.preparation,f.service);
        if(!reference){f.kind=PlaybackError::service_failed;return {};}
        const auto flags=static_cast<std::uint16_t>(reference->preparation()->runtime_flags()|p.caller_play_flags);
        // Source mode1 cleanup and alternate activation have distinct unbound
        // producers. Reject before public publication rather than strand output.
        if(flags&9u){f.kind=PlaybackError::unsupported_play_flags;return {};}
        auto copy=NativeProgramCopy::clone(reference,f.copy);
        if(!copy){f.kind=PlaybackError::copy_failed;return {};}
        auto schedule=copy->bind_preactivated_schedule(p.initial_deadline,flags,f.copy);
        if(!schedule){f.kind=PlaybackError::copy_failed;return {};}
        auto executor=ProgramExecutor::create(schedule->data,schedule->deadline,f.program);
        if(!executor){f.kind=PlaybackError::program_failed;return {};}
        {std::lock_guard lock(state->mutex);
         // Re-find no vector location: the strong reservation is the commit owner.
         if(!state->available(f)||!state->context(context,f))return {};
         entry->executor.emplace(std::move(*executor));entry->copy=std::move(copy);entry->effective_play_flags=flags;entry->preparing=false;work.reservation.reset();
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
        NormalOutputParameters p{e.parameters.context,e.parameters.program,e.parameters.envelope,e.parameters.source_output_mode,state_->rate};
        if(!e.copy||!e.copy->start_output(*event,p,f.copy)){
            f.kind=f.copy.kind==ProgramCopyError::voice_failed?PlaybackError::voice_failed:PlaybackError::copy_failed;
            f.voice=f.copy.voice;e.executor->acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;
        }
        if(!e.executor->acknowledge_start(true,f.program)){f.kind=PlaybackError::program_failed;e.terminal=f;return std::nullopt;}
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
            if(e.copy)e.copy->append_outputs(voices);
        }
        auto output=mix_host_voices(voices.data(),voices.size(),frames,f.voice);if(!output)f.kind=PlaybackError::voice_failed;return output;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return std::nullopt;
}
std::optional<PlaybackObservation> NativePlaybackOwner::observe(const NativePlayHandle& h,PlaybackFailure& f)const{
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return std::nullopt;const auto& e=*slot->entry;
    const auto outputs=e.outputs();PlaybackObservation o;
    o.deadline=e.executor->deadline();o.command_index=e.executor->command_index();o.voices=outputs.voices;
    o.first_voice_source_index=outputs.first_voice_source_index;o.active_voices=outputs.active_voices;
    o.executor_finished=e.finished;o.explicitly_stopped=e.stopped;o.terminal_failure=e.terminal;
    o.private_copy_bound=bool(e.copy);o.resources_retired=e.resources_retired;o.effective_play_flags=e.effective_play_flags;return o;
}
bool NativePlaybackOwner::release(const NativePlayHandle& h,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;std::shared_ptr<State::Entry> retired;
    {std::lock_guard lock(state->mutex);if(!state->available(f))return false;
     auto* slot=state->find(h.owner_,h.identity_,f);if(!slot)return false;
     if(slot->entry->busy()){f.kind=PlaybackError::active_playback;return false;}
     if(!state->admit(state,work,f))return false;retired=std::move(slot->entry);
    }
    retired.reset();return true;
}
bool NativePlaybackOwner::unload(const AudioBankCommand& c,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    if(c.opcode!=AudioBankOpcode::unload){f.kind=PlaybackError::service_failed;f.service.kind=BankServiceError::wrong_command;return false;}
    State::Activity work;std::vector<std::unique_ptr<NativeProgramCopy>> copies;
    std::vector<std::shared_ptr<State::Entry>> retired;bool downstream_started=false;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f))return false;
         if(state->unloading.contains(c.bank_token.value)){f.kind=state->unloading.at(c.bank_token.value)?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return false;}
         std::size_t count=0;
         for(const auto& slot:state->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value){
             if(slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}++count;
         }
         copies.reserve(count);retired.reserve(count);
         if(!state->admit(state,work,f))return false;
         state->unloading.emplace(c.bank_token.value,false);work.bank=c.bank_token.value;
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value&&slot.entry->copy)copies.push_back(slot.entry->retire_copy());
        }
        copies.clear();downstream_started=true;
        if(!state->service.unload(c,f.service)){
            work.uncertain=f.service.kind==BankServiceError::native_exception||f.service.kind==BankServiceError::bank_state_unknown;
            f.kind=PlaybackError::service_failed;return false;
        }
        {std::lock_guard lock(state->mutex);
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value)retired.push_back(std::move(slot.entry));
        }
        retired.clear();return true;
    }catch(const std::bad_alloc&){work.uncertain=downstream_started;allocation_failure(f);}catch(const std::length_error&){work.uncertain=downstream_started;f.kind=PlaybackError::host_capacity;}
    catch(...){work.uncertain=downstream_started;f.kind=PlaybackError::native_exception;}return false;
}
bool NativePlaybackOwner::close_when_idle(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;std::vector<std::unique_ptr<NativeProgramCopy>> copies;
    std::vector<std::shared_ptr<State::Entry>> retired;bool downstream_started=false;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f))return false;
         if(state->operations){f.kind=PlaybackError::operation_active;return false;}
         std::size_t count=0;
         for(const auto& slot:state->slots)if(slot.entry){if(slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}++count;}
         copies.reserve(count);retired.reserve(count);
         if(!state->admit(state,work,f))return false;
         state->closing=true;work.closes=true;
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->copy)copies.push_back(slot.entry->retire_copy());
        }
        copies.clear();downstream_started=true;
        if(!state->service.close_when_idle(f.service)){f.kind=PlaybackError::service_failed;return false;}
        {std::lock_guard lock(state->mutex);
         state->closed=true;state->closing=false;
         for(auto& slot:state->slots)if(slot.entry)retired.push_back(std::move(slot.entry));
        }
        retired.clear();return true;
    }catch(const std::bad_alloc&){work.uncertain=downstream_started;allocation_failure(f);}catch(const std::length_error&){work.uncertain=downstream_started;f.kind=PlaybackError::host_capacity;}
    catch(...){work.uncertain=downstream_started;f.kind=PlaybackError::native_exception;}return false;
}
static_assert(std::is_nothrow_move_constructible_v<ProgramExecutor>);
} // namespace mka::sound
