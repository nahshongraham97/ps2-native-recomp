#include "mka/playback_owner.hpp"
#include <mutex>
#include <map>
#include <algorithm>
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
        std::shared_ptr<const void> context,identity;
        std::optional<std::size_t> output_role;
        bool dispatch_live=false,replaced=false;
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
    std::shared_ptr<NativeProgramHostClock> clock;
    bool closed=false,closing=false,close_unknown=false,clock_sampling=false;
    std::size_t operations=0;
    std::vector<Slot> slots;
    struct Context {
        std::shared_ptr<const void> identity;
        bool live=true;
        NativeSoundContextConfiguration configuration;
        std::map<std::size_t,std::shared_ptr<const void>> roles;
        std::vector<std::shared_ptr<const void>> ordinary;
    };
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
    State(AudioBankService&& s,std::uint32_t r,std::shared_ptr<const void> id,std::shared_ptr<NativeProgramHostClock> timer)
        :service(std::move(s)),identity(std::move(id)),rate(r),clock(std::move(timer)){}
    Context* context_identity(const std::shared_ptr<const void>& id){for(auto& c:contexts)if(c.identity==id)return &c;return nullptr;}
    Entry* entry_identity(const std::shared_ptr<const void>& id){for(auto& s:slots)if(s.identity==id)return s.entry.get();return nullptr;}
    void detach_dispatch(Entry& e){
        if(auto* c=context_identity(e.context))std::erase(c->ordinary,e.identity);
        e.dispatch_live=false;
    }
    bool owns_output(const Entry& e){
        auto* c=context_identity(e.context);if(!c||!e.output_role)return false;
        auto found=c->roles.find(*e.output_role);return found!=c->roles.end()&&found->second==e.identity;
    }
    void detach_entry(Entry& e){
        detach_dispatch(e);
        if(auto* c=context_identity(e.context);c&&e.output_role){auto found=c->roles.find(*e.output_role);
            if(found!=c->roles.end()&&found->second==e.identity)c->roles.erase(found);
        }
    }
    std::optional<std::size_t> select(Context& c,const NativeProgramOutputRequest& request,PlaybackFailure& f){
        if(request.selector==-2){f.kind=PlaybackError::unsupported_output_selector;return {};}
        if(request.selector<-1){f.kind=PlaybackError::invalid_output_selector;return {};}
        if(request.selector>=0){auto role=static_cast<std::size_t>(request.selector);
            if(role>=c.configuration.explicit_role_count){f.kind=PlaybackError::invalid_output_selector;return {};}return role;
        }
        auto role=c.configuration.explicit_role_count;
        while(c.roles.contains(role)){
            if(role==std::numeric_limits<std::size_t>::max()){f.kind=PlaybackError::host_capacity;return {};}++role;
        }
        return role;
    }
    std::optional<ProgramStep> step_entry(Entry& e,ProgramClockWord now,PlaybackFailure& f){
        if(e.terminal){f=*e.terminal;return {};}
        auto event=e.executor->step(now,f.program);if(!event){f.kind=PlaybackError::program_failed;e.terminal=f;return {};}
        if(event->kind==ProgramStepKind::sample_start){
            NormalOutputParameters p{e.parameters.context,e.parameters.program,e.parameters.envelope,e.parameters.source_output_mode,rate};
            if(!e.copy||!e.copy->start_output(*event,p,f.copy)){
                f.kind=f.copy.kind==ProgramCopyError::voice_failed?PlaybackError::voice_failed:PlaybackError::copy_failed;
                f.voice=f.copy.voice;e.executor->acknowledge_start(false,f.program);e.terminal=f;return {};
            }
            if(!e.executor->acknowledge_start(true,f.program)){f.kind=PlaybackError::program_failed;e.terminal=f;return {};}
        }else if(event->kind==ProgramStepKind::program_stop){
            if(!e.stop_voices(*event,f)){e.terminal=f;return {};}e.finished=true;detach_dispatch(e);
        }else if(event->kind==ProgramStepKind::structural_end||event->kind==ProgramStepKind::finished){e.finished=true;detach_dispatch(e);}
        return event;
    }
    bool available(PlaybackFailure& f)const {if(clock_sampling){f.kind=PlaybackError::operation_active;return false;}if(closed){f.kind=PlaybackError::closed;return false;}if(close_unknown){f.kind=PlaybackError::close_state_unknown;return false;}if(closing){f.kind=PlaybackError::closing;return false;}return true;}
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
    f={};if(!rate){f.kind=PlaybackError::invalid_output_rate;return {};}
    auto clock=NativeProgramHostClock::create(f.clock);if(!clock){f.kind=PlaybackError::clock_failed;return {};}
    return create_with_clock(std::move(s),rate,std::move(clock),f);
}
std::optional<NativePlaybackOwner> NativePlaybackOwner::create_with_clock(AudioBankService&& s,std::uint32_t rate,
    std::shared_ptr<NativeProgramHostClock> clock,PlaybackFailure& f){
    f={};if(!rate){f.kind=PlaybackError::invalid_output_rate;return {};}
    if(!clock){f.kind=PlaybackError::invalid_clock;return {};}
    try{auto id=std::make_shared<int>(0);return NativePlaybackOwner(std::make_shared<State>(std::move(s),rate,std::move(id),std::move(clock)));}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return {};
}
std::shared_ptr<AudioResourceDomain> NativePlaybackOwner::resource_domain() const noexcept {
    return state_?state_->service.resource_domain():nullptr;
}
std::optional<NativeSoundContext> NativePlaybackOwner::create_sound_context(NativeSoundContextConfiguration configuration,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f))return {};
    try {
        auto id=std::make_shared<int>(0);state->contexts.push_back({id,true,configuration,{},{}});
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
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_){state->detach_entry(*slot.entry);retired.push_back(std::move(slot.entry));}
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
    const AudioBankCommand& c,const NativePlayParameters& p,PlaybackFailure& f){return play_impl(context,c,p,false,f);}
std::optional<NativePlayHandle> NativePlaybackOwner::activate(const NativeSoundContext& context,
    const AudioBankCommand& c,const NativeActivationParameters& p,PlaybackFailure& f){
    // This private placeholder is never a public activated deadline. The real
    // shared clock is sampled at commit after preparation/incumbent cleanup.
    NativePlayParameters pending(p.context,p.program,p.envelope,{0},p.source_output_mode,p.caller_play_flags,p.output);
    return play_impl(context,c,pending,true,f);
}
std::optional<NativePlayHandle> NativePlaybackOwner::play_impl(const NativeSoundContext& context,
    const AudioBankCommand& c,const NativePlayParameters& p,bool sample_activation_clock,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;std::shared_ptr<State::Entry> entry;NativePlayHandle handle;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f)||!state->context(context,f))return {};
         if(state->unloading.contains(c.bank_token.value)){f.kind=state->unloading.at(c.bank_token.value)?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return {};}
         if(!state->admit(state,work,f))return {};
         State::Slot slot;slot.identity=std::make_shared<int>(0);slot.entry=std::make_shared<State::Entry>(c.bank_token,p,context.identity_);
         entry=slot.entry;entry->identity=slot.identity;handle.owner_=state->identity;handle.identity_=slot.identity;
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
        {std::unique_lock lock(state->mutex);
         // No vector position is retained across resource construction.
         auto* destination=state->context(context,f);if(!state->available(f)||!destination)return {};
         auto role=state->select(*destination,p.output,f);if(!role)return {};
         auto found=destination->roles.find(*role);State::Entry* incumbent=nullptr;
         if(found!=destination->roles.end()&&found->second){incumbent=state->entry_identity(found->second);
             if(!incumbent){f.kind=PlaybackError::unknown_handle;return {};}
             const auto old_priority=incumbent->parameters.output.priority;
             if(p.output.priority<old_priority||(p.output.priority==old_priority&&(flags&0x80u))){f.kind=PlaybackError::output_priority_refused;return {};}
         }
         // Allocate all registry/list storage before an irreversible output stop.
         if(destination->ordinary.size()==destination->ordinary.max_size()){f.kind=PlaybackError::host_capacity;return {};}
         destination->ordinary.reserve(destination->ordinary.size()+1);
         const bool inserted=found==destination->roles.end();
         if(inserted)found=destination->roles.emplace(*role,std::shared_ptr<const void>{}).first;
         struct RoleGuard {State::Context* context;std::size_t role;bool erase;
             ~RoleGuard(){if(erase)context->roles.erase(role);}} role_guard{destination,*role,inserted};
         if(incumbent){
             auto stop=incumbent->executor->stop_from_owner(f.program);
             if(!stop){f.kind=PlaybackError::program_failed;return {};}
             if(!incumbent->stop_voices(*stop,f))return {};
             incumbent->stopped=true;incumbent->finished=true;state->detach_dispatch(*incumbent);
         }
         auto deadline=p.initial_deadline;
         if(sample_activation_clock){
             // Seal registry mutation, then release its lock around the host
             // counter port. Reentrant callers get a named busy result, not a
             // deadlock; pending preparations cannot commit across this phase.
             state->clock_sampling=true;lock.unlock();auto now=state->clock->read(f.clock);lock.lock();state->clock_sampling=false;
             if(!now){f.kind=PlaybackError::clock_failed;return {};}
             if(!state->available(f)||!state->context(context,f))return {};
             deadline=ordinary_initial_deadline(copy->commands().front(),*now);
         }
         // Copy/reset preflight established nonnull, nonempty immutable data.
         // This local construction allocates no storage and publishes no callback.
         auto executor=ProgramExecutor::create(schedule->data,deadline,f.program);
         if(!executor){f.kind=PlaybackError::program_failed;return {};}
         entry->executor.emplace(std::move(*executor));entry->copy=std::move(copy);entry->effective_play_flags=flags;
         entry->output_role=*role;entry->dispatch_live=true;entry->preparing=false;
         destination->ordinary.push_back(entry->identity);found->second=entry->identity;
         if(incumbent)incumbent->replaced=true;
         role_guard.erase=false;work.reservation.reset();
        }
        return handle;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
std::optional<ProgramStep> NativePlaybackOwner::tick(const NativePlayHandle& h,ProgramClockWord now,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return {};}std::lock_guard lock(state_->mutex);if(!state_->available(f))return {};
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return {};return state_->step_entry(*slot->entry,now,f);
}
bool NativePlaybackOwner::update_ordinary_context(const NativeSoundContext& context,SoundContextUpdate& output,PlaybackFailure& f){
    f={};output={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    std::unique_lock lock(state->mutex);if(!state->available(f))return false;
    auto* c=state->context(context,f);if(!c)return false;
    try {
        // Snapshot original membership order before successful stops unlink it.
        // Actual command extent bounds reporting storage, never a device cap.
        auto identities=c->ordinary;std::size_t capacity=0;
        for(const auto& id:identities){auto* e=state->entry_identity(id);if(!e||!e->dispatch_live)continue;
            if(!e->copy){f.kind=PlaybackError::copy_failed;f.copy.kind=ProgramCopyError::absent_runtime_reference;return false;}
            const auto count=e->copy->commands().size(),cursor=e->executor->command_index();
            const auto remaining=count>cursor?count-cursor:0;
            if(remaining>=output.steps.max_size()-capacity){f.kind=PlaybackError::host_capacity;return false;}
            capacity+=remaining+1;
        }
        output.steps.reserve(capacity);
        state->clock_sampling=true;lock.unlock();auto now=state->clock->read(f.clock);lock.lock();state->clock_sampling=false;
        if(!now){f.kind=PlaybackError::clock_failed;return false;}
        if(!state->available(f)||!state->context(context,f))return false;output.observed=*now;
        for(const auto& id:identities){auto* e=state->entry_identity(id);if(!e||!e->dispatch_live)continue;
            while(e->dispatch_live){
                SoundContextStep step;step.handle.owner_=state->identity;step.handle.identity_=id;
                step.event=state->step_entry(*e,*now,f);if(!step.event)step.failure=f;
                output.steps.push_back(std::move(step));const auto& result=output.steps.back();
                if(!result.event)return false;
                const auto kind=result.event->kind;
                // Original2C5B0/2C5C4 re-enter the dispatcher for every due
                // next command, using THIS SAME sampled clock word.
                if(kind!=ProgramStepKind::sample_start)break;
                const auto cursor=e->executor->command_index();const auto& commands=e->copy->commands();
                if(cursor<commands.size()&&now->milliseconds<e->executor->deadline().milliseconds&&!(commands[cursor].bytes[8]&0x80u))break;
            }
        }
        return true;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return false;
}
bool NativePlaybackOwner::stop(const NativePlayHandle& h,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return false;auto& e=*slot->entry;
    auto event=e.executor->stop_from_owner(f.program);if(!event){f.kind=PlaybackError::program_failed;return false;}
    if(!e.stop_voices(*event,f))return false;e.stopped=true;state_->detach_dispatch(e);return true;
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
    o.private_copy_bound=bool(e.copy);o.resources_retired=e.resources_retired;o.effective_play_flags=e.effective_play_flags;
    o.output_role=e.output_role;o.output_owned=state_->owns_output(e);o.dispatch_live=e.dispatch_live;o.replaced=e.replaced;o.priority=e.parameters.output.priority;return o;
}
bool NativePlaybackOwner::release(const NativePlayHandle& h,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;std::shared_ptr<State::Entry> retired;
    {std::lock_guard lock(state->mutex);if(!state->available(f))return false;
     auto* slot=state->find(h.owner_,h.identity_,f);if(!slot)return false;
     if(slot->entry->busy()){f.kind=PlaybackError::active_playback;return false;}
     if(!state->admit(state,work,f))return false;state->detach_entry(*slot->entry);retired=std::move(slot->entry);
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
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value){state->detach_entry(*slot.entry);retired.push_back(std::move(slot.entry));}
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
         for(auto& slot:state->slots)if(slot.entry){state->detach_entry(*slot.entry);retired.push_back(std::move(slot.entry));}
        }
        retired.clear();return true;
    }catch(const std::bad_alloc&){work.uncertain=downstream_started;allocation_failure(f);}catch(const std::length_error&){work.uncertain=downstream_started;f.kind=PlaybackError::host_capacity;}
    catch(...){work.uncertain=downstream_started;f.kind=PlaybackError::native_exception;}return false;
}
static_assert(std::is_nothrow_move_constructible_v<ProgramExecutor>);
} // namespace mka::sound
