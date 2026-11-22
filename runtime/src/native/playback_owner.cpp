#include "mka/playback_owner.hpp"
#include "mka/pending_play.hpp"
#include <thread>
#include <mutex>
#include <map>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <type_traits>
#include <new>
#include <stdexcept>
#include <variant>
namespace mka::sound {
struct NativePlaybackOwner::State {
    using BankReference=std::variant<BankWireToken,AudioBankHandle>;
    static bool same_bank(const BankReference& a,const BankReference& b) noexcept {
        if(a.index()!=b.index())return false;
        if(const auto* native=std::get_if<AudioBankHandle>(&a))
            return native->same_identity(std::get<AudioBankHandle>(b));
        return std::get<BankWireToken>(a).value==std::get<BankWireToken>(b).value;
    }
    struct Entry {
        BankReference bank;
        NativePlayParameters parameters;
        std::shared_ptr<const void> context,identity,bank_identity;
        std::optional<std::size_t> output_role;
        bool dispatch_live=false,replaced=false;
        std::optional<ProgramExecutor> executor;
        bool preparing=true;
        std::unique_ptr<NativeProgramCopy> copy;
        NativeProgramOutputObservation retired_outputs;
        std::uint16_t effective_play_flags=0;
        bool finished=false,stopped=false,resources_retired=false;
        std::optional<PlaybackFailure> terminal;
        Entry(BankReference b,const NativePlayParameters& p,std::shared_ptr<const void> c):bank(std::move(b)),parameters(p),context(std::move(c)){}
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
    struct Slot {std::shared_ptr<const void> identity;std::shared_ptr<Entry> entry;bool retiring=false;};
    struct Retired {
        explicit Retired(BankReference b):bank(std::move(b)){}
        BankReference bank;
        std::shared_ptr<const void> identity,bank_identity,context,origin;
        std::vector<HostVoice> voices;
        bool resources_released=false;
        std::size_t active()const noexcept{std::size_t n=0;for(const auto& voice:voices)if(voice.active())++n;return n;}
    };
    mutable std::mutex mutex;
    std::optional<AudioBankService> service;
    std::optional<AudioBankSession> native_session;
    std::shared_ptr<const void> identity;
    std::uint32_t rate;
    const AudioControlCurve control_curve;
    std::shared_ptr<NativeProgramHostClock> clock;
    bool closed=false,closing=false,close_unknown=false,clock_sampling=false;
    std::size_t operations=0;
    std::vector<Slot> slots;
    std::vector<std::unique_ptr<Retired>> retired;
    struct Context {
        std::shared_ptr<const void> identity;
        bool live=true;
        NativeSoundContextConfiguration configuration;
        std::map<std::size_t,std::shared_ptr<const void>> roles;
        std::vector<std::shared_ptr<const void>> ordinary;
        bool cleaning=false;
    };
    std::vector<Context> contexts;
    struct UnloadGate {BankReference bank;bool uncertain=false;};
    std::vector<UnloadGate> unloading;
    std::optional<bool> unloading_state(const BankReference& bank) const noexcept {
        for(const auto& gate:unloading)if(same_bank(gate.bank,bank))return gate.uncertain;
        return {};
    }
    struct Activity {
        std::shared_ptr<State> state;
        std::optional<BankReference> bank;
        std::shared_ptr<const void> reservation,clearing_context;
        bool closes=false,uncertain=false,public_reservation=false;
        Activity()=default;
        Activity(const Activity&)=delete;
        Activity& operator=(const Activity&)=delete;
        ~Activity(){
            if(!state)return;
            std::lock_guard lock(state->mutex);
            if(reservation)for(auto it=state->slots.begin();it!=state->slots.end();++it)if(it->identity==reservation&&it->entry&&it->entry->preparing){if(public_reservation)it->entry.reset();else state->slots.erase(it);break;}
            if(bank)for(auto it=state->unloading.begin();it!=state->unloading.end();++it)
                if(same_bank(it->bank,*bank)){if(uncertain)it->uncertain=true;else state->unloading.erase(it);break;}
            if(closes&&!state->closed){if(uncertain)state->close_unknown=true;else state->closing=false;}
            if(clearing_context)if(auto* c=state->context_identity(clearing_context))c->cleaning=false;
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
            if(!item.live){f.kind=PlaybackError::retired_context;return nullptr;}
            if(item.cleaning){f.kind=PlaybackError::operation_active;return nullptr;}return &item;
        }
        f.kind=PlaybackError::unknown_context;return nullptr;
    }
    State(AudioBankService&& s,std::uint32_t r,std::shared_ptr<const void> id,std::shared_ptr<NativeProgramHostClock> timer)
        :service(std::move(s)),identity(std::move(id)),rate(r),control_curve(AudioControlCurve::initial_nonlinear),clock(std::move(timer)){}
    State(AudioBankSession&& s,std::uint32_t r,AudioControlCurve curve,std::shared_ptr<const void> id,std::shared_ptr<NativeProgramHostClock> timer)
        :native_session(std::move(s)),identity(std::move(id)),rate(r),control_curve(curve),clock(std::move(timer)){}
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
            NormalOutputParameters p{e.parameters.context,e.parameters.program,e.parameters.envelope,e.parameters.source_output_mode,rate,control_curve};
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
    std::optional<ProgramClockWord> read_clock(std::unique_lock<std::mutex>& lock,PlaybackFailure& f){
        // A fixed QPC/steady native counter has no game callback. Keeping this
        // tiny platform query serialized avoids false busy admissions from the
        // real worker. Explicit reader callouts keep their prior unlocked gap.
        if(clock->uses_native_counter())return clock->read(f.clock);
        clock_sampling=true;lock.unlock();auto now=clock->read(f.clock);lock.lock();clock_sampling=false;return now;
    }
    SoundContextObservation observe_outputs(const std::shared_ptr<const void>* filter=nullptr) const {
        SoundContextObservation result;
        for(const auto& slot:slots)if(slot.entry&&(!filter||slot.entry->context==*filter)){
            const auto& e=*slot.entry;
            if(e.preparing)++result.pending_preparations;else ++result.programs;
            result.active_voices+=e.outputs().active_voices;
        }
        for(const auto& group:retired)if(!filter||group->context==*filter){
            ++result.retired_groups;result.retired_voices+=group->voices.size();result.active_retired_voices+=group->active();
        }
        result.active_voices+=result.active_retired_voices;return result;
    }
    Slot* find(const std::shared_ptr<const void>& owner,const std::shared_ptr<const void>& id,PlaybackFailure& f){
        if(!owner||!id){f.kind=PlaybackError::unknown_handle;return nullptr;}
        if(owner!=identity){f.kind=PlaybackError::foreign_owner;return nullptr;}
        for(auto& s:slots)if(s.identity==id){
            if(s.retiring||(s.entry&&s.entry->preparing)){f.kind=PlaybackError::operation_active;return nullptr;}
            if(!s.entry){f.kind=PlaybackError::retired_handle;return nullptr;}return &s;
        }
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
    if(!state_)return nullptr;
    return state_->native_session?state_->native_session->resource_domain():state_->service->resource_domain();
}
std::optional<NativePlaybackOwner> NativePlaybackOwner::create(AudioBankSession&& s,std::uint32_t rate,AudioControlCurve curve,PlaybackFailure& f){
    f={};if(!rate){f.kind=PlaybackError::invalid_output_rate;return {};}
    if(!valid_audio_control_curve(curve)){f.kind=PlaybackError::invalid_control_curve;return {};}
    auto clock=NativeProgramHostClock::create(f.clock);if(!clock){f.kind=PlaybackError::clock_failed;return {};}
    return create_with_clock(std::move(s),rate,curve,std::move(clock),f);
}
std::optional<NativePlaybackOwner> NativePlaybackOwner::create_with_clock(AudioBankSession&& s,std::uint32_t rate,
    AudioControlCurve curve,std::shared_ptr<NativeProgramHostClock> clock,PlaybackFailure& f){
    f={};if(!rate){f.kind=PlaybackError::invalid_output_rate;return {};}
    if(!valid_audio_control_curve(curve)){f.kind=PlaybackError::invalid_control_curve;return {};}
    if(!clock){f.kind=PlaybackError::invalid_clock;return {};}
    if(!s.share_native_services(f.native_bank)){f.kind=PlaybackError::native_bank_failed;return {};}
    try{auto id=std::make_shared<int>(0);return NativePlaybackOwner(std::make_shared<State>(std::move(s),rate,curve,std::move(id),std::move(clock)));}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return {};
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
    return state->observe_outputs(&c.identity_);
}
std::optional<SoundContextObservation> NativePlaybackOwner::observe_all_outputs(PlaybackFailure& f)const {
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f))return {};
    return state->observe_outputs();
}
std::optional<std::vector<NativePlayHandle>> NativePlaybackOwner::snapshot_active_plays(const NativeSoundContext& c,PlaybackFailure& f) const {
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f)||!state->context(c,f))return {};
    try {
        std::size_t count=0;
        for(const auto& slot:state->slots)if(slot.entry&&!slot.entry->preparing&&slot.entry->dispatch_live)++count;
        std::vector<NativePlayHandle> result;result.reserve(count);
        for(const auto& slot:state->slots)if(slot.entry&&!slot.entry->preparing&&slot.entry->dispatch_live){
            NativePlayHandle handle;handle.owner_=state->identity;handle.identity_=slot.identity;result.push_back(std::move(handle));
        }
        return result;
    }catch(const std::bad_alloc&){f.kind=PlaybackError::allocation_failed;}
     catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    return {};
}
bool NativePlaybackOwner::release_sound_context(const NativeSoundContext& c,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;std::vector<std::shared_ptr<State::Entry>> retired;
    std::vector<std::unique_ptr<State::Retired>> outputs;
    try {
        {std::lock_guard lock(state->mutex);if(!state->available(f))return false;
         auto* context=state->context(c,f);if(!context)return false;
         std::size_t count=0,groups=0;
         for(const auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_){
             if(slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}++count;
         }
         for(const auto& group:state->retired)if(group->context==c.identity_){
             if(group->active()){f.kind=PlaybackError::active_playback;return false;}++groups;
         }
         retired.reserve(count);outputs.reserve(groups);if(!state->admit(state,work,f))return false;
         context->live=false;
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->context==c.identity_){state->detach_entry(*slot.entry);retired.push_back(std::move(slot.entry));}
         for(auto it=state->retired.begin();it!=state->retired.end();)if((*it)->context==c.identity_){outputs.push_back(std::move(*it));it=state->retired.erase(it);}else ++it;
        }
        retired.clear();outputs.clear();return true;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return false;
}
std::optional<BankLoadOutcome> NativePlaybackOwner::load_sync(const AudioBankCommand& c,BankWireToken token,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    if(!state->service){f.kind=PlaybackError::wrong_bank_interface;return {};}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return {};}
    try {auto r=state->service->load_sync(c,token,f.service);if(!r)f.kind=PlaybackError::service_failed;return r;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
std::optional<AsyncBankSubmission> NativePlaybackOwner::submit_async(const AudioBankCommand& c,BankWireToken token,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    if(!state->service){f.kind=PlaybackError::wrong_bank_interface;return {};}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return {};}
    try {auto r=state->service->submit_async(c,token,f.service);if(!r)f.kind=PlaybackError::service_failed;return r;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
bool NativePlaybackOwner::run_one(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    if(!state->service){f.kind=PlaybackError::wrong_bank_interface;return false;}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return false;}
    try {if(!state->service->run_one(f.service)){f.kind=PlaybackError::service_failed;return false;}return true;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return false;
}
std::optional<BankLoadCompletion> NativePlaybackOwner::poll_completion(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    if(!state->service){f.kind=PlaybackError::wrong_bank_interface;return {};}
    State::Activity work;
    {std::lock_guard lock(state->mutex);if(!state->admit(state,work,f))return {};}
    try {auto r=state->service->poll_completion(f.service);if(!r&&f.service.kind!=BankServiceError::none)f.kind=PlaybackError::service_failed;return r;}
    catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}
std::optional<NativePlayHandle> NativePlaybackOwner::play(const NativeSoundContext& context,
    const AudioBankCommand& c,const NativePlayParameters& p,PlaybackFailure& f){return play_impl(context,&c,nullptr,c.selector,p,false,f);}
std::optional<NativePlayHandle> NativePlaybackOwner::activate(const NativeSoundContext& context,
    const AudioBankCommand& c,const NativeActivationParameters& p,PlaybackFailure& f){
    // This private placeholder is never a public activated deadline. The real
    // shared clock is sampled at commit after preparation/incumbent cleanup.
    NativePlayParameters pending(p.context,p.program,p.envelope,{0},p.source_output_mode,p.caller_play_flags,p.output);
    return play_impl(context,&c,nullptr,c.selector,pending,true,f);
}
std::optional<NativePlayHandle> NativePlaybackOwner::play(const NativeSoundContext& context,
    const AudioBankHandle& bank,std::int32_t selector,const NativePlayParameters& p,PlaybackFailure& f){
    return play_impl(context,nullptr,&bank,selector,p,false,f);
}
std::optional<NativePlayHandle> NativePlaybackOwner::activate(const NativeSoundContext& context,
    const AudioBankHandle& bank,std::int32_t selector,const NativeActivationParameters& p,PlaybackFailure& f){
    NativePlayParameters pending(p.context,p.program,p.envelope,{0},p.source_output_mode,p.caller_play_flags,p.output);
    return play_impl(context,nullptr,&bank,selector,pending,true,f);
}
struct NativePreparedPlay {
    std::unique_ptr<NativeProgramCopy> copy;
    OrdinaryProgramSchedule schedule;
    std::uint16_t flags=0;
};
std::unique_ptr<NativePreparedPlay> NativePlaybackOwner::prepare_play_plan(
    std::unique_ptr<RuntimeProgramReference>& reference,const NativePlayParameters& p,PlaybackFailure& f){
        const auto flags=static_cast<std::uint16_t>(reference->preparation()->runtime_flags()|p.caller_play_flags);
        // Source mode1 cleanup and alternate activation have distinct unbound
        // producers. Reject before public publication rather than strand output.
        if(flags&9u){f.kind=PlaybackError::unsupported_play_flags;return {};}
        auto copy=NativeProgramCopy::clone(reference,f.copy);
        if(!copy){f.kind=PlaybackError::copy_failed;return {};}
        auto schedule=copy->bind_preactivated_schedule(p.initial_deadline,flags,f.copy);
        if(!schedule){f.kind=PlaybackError::copy_failed;return {};}
        // Semantic coverage must precede incumbent stop and host-clock sampling.
        if(!validate_program_coverage(*schedule->data,f.program)){f.kind=PlaybackError::program_failed;return {};}
        auto plan=std::make_unique<NativePreparedPlay>();plan->copy=std::move(copy);plan->schedule=std::move(*schedule);plan->flags=flags;return plan;
 }
bool NativePlaybackOwner::commit_play_plan(const std::shared_ptr<State>& state,const NativeSoundContext& context,
    const NativePlayHandle& handle,NativePreparedPlay& plan,bool sample_activation_clock,PlaybackFailure& f){
    // The reservation is owned throughout this call, including unlocked clock
    // callbacks. Re-fetch by identity after every resource/concurrency gap.
    std::shared_ptr<State::Entry> entry;
    {std::lock_guard lock(state->mutex);
     for(auto& slot:state->slots)if(slot.identity==handle.identity_){entry=slot.entry;break;}}
    if(!entry||!entry->preparing){f.kind=PlaybackError::invalid_work_state;return false;}
    const auto& p=entry->parameters;
    const auto* native=std::get_if<AudioBankHandle>(&entry->bank);
        {std::unique_lock lock(state->mutex);
         // No vector position is retained across resource construction.
         auto* destination=state->context(context,f);if(!state->available(f)||!destination)return false;
         if(native&&!state->native_session->load_state(*native,f.native_bank)){f.kind=PlaybackError::native_bank_failed;return false;}
         auto role=state->select(*destination,p.output,f);if(!role)return false;
         auto found=destination->roles.find(*role);State::Entry* incumbent=nullptr;
         if(found!=destination->roles.end()&&found->second){incumbent=state->entry_identity(found->second);
             if(!incumbent){f.kind=PlaybackError::unknown_handle;return false;}
             const auto old_priority=incumbent->parameters.output.priority;
             if(p.output.priority<old_priority||(p.output.priority==old_priority&&(plan.flags&0x80u))){f.kind=PlaybackError::output_priority_refused;return false;}
         }
         // Allocate all registry/list storage before an irreversible output stop.
         if(destination->ordinary.size()==destination->ordinary.max_size()){f.kind=PlaybackError::host_capacity;return false;}
         destination->ordinary.reserve(destination->ordinary.size()+1);
         const bool inserted=found==destination->roles.end();
         if(inserted)found=destination->roles.emplace(*role,std::shared_ptr<const void>{}).first;
         struct RoleGuard {State::Context* context;std::size_t role;bool erase;
             ~RoleGuard(){if(erase)context->roles.erase(role);}} role_guard{destination,*role,inserted};
         if(incumbent){
             auto stop=incumbent->executor->stop_from_owner(f.program);
             if(!stop){f.kind=PlaybackError::program_failed;return false;}
             if(!incumbent->stop_voices(*stop,f))return false;
             incumbent->stopped=true;incumbent->finished=true;state->detach_dispatch(*incumbent);
         }
         auto deadline=p.initial_deadline;
         if(sample_activation_clock){
             // Seal registry mutation, then release its lock around the host
             // counter port. Reentrant callers get a named busy result, not a
             // deadlock; pending preparations cannot commit across this phase.
             auto now=state->read_clock(lock,f);
             if(!now){f.kind=PlaybackError::clock_failed;return false;}
             if(!state->available(f)||!state->context(context,f))return false;
             // A native host-clock callback can revoke the shared Session.
             // Do not publish a prepared play after that known revocation.
             if(native&&!state->native_session->load_state(*native,f.native_bank)){f.kind=PlaybackError::native_bank_failed;return false;}
             deadline=ordinary_initial_deadline(plan.copy->commands().front(),*now);
         }
         // Copy/reset preflight established nonnull, nonempty immutable data.
         // This local construction allocates no storage and publishes no callback.
         auto executor=ProgramExecutor::create(plan.schedule.data,deadline,f.program);
         if(!executor){f.kind=PlaybackError::program_failed;return false;}
         entry->executor.emplace(std::move(*executor));entry->bank_identity=plan.copy->bank_identity();entry->copy=std::move(plan.copy);entry->effective_play_flags=plan.flags;
         entry->output_role=*role;entry->dispatch_live=true;entry->preparing=false;
         destination->ordinary.push_back(entry->identity);found->second=entry->identity;
         if(incumbent)incumbent->replaced=true;
         role_guard.erase=false;
        }
    return true;
}
std::optional<NativePlayHandle> NativePlaybackOwner::play_impl(const NativeSoundContext& context,
    const AudioBankCommand* c,const AudioBankHandle* native,std::int32_t selector,
    const NativePlayParameters& p,bool sample_activation_clock,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    if((native&&!state->native_session)||(!native&&!state->service)){f.kind=PlaybackError::wrong_bank_interface;return {};}
    const State::BankReference bank=native?State::BankReference(*native):State::BankReference(c->bank_token);
    State::Activity work;std::shared_ptr<State::Entry> entry;NativePlayHandle handle;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f)||!state->context(context,f))return {};
         if(const auto gate=state->unloading_state(bank)){f.kind=*gate?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return {};}
         if(!state->admit(state,work,f))return {};
         State::Slot slot;slot.identity=std::make_shared<int>(0);slot.entry=std::make_shared<State::Entry>(bank,p,context.identity_);
         entry=slot.entry;entry->identity=slot.identity;handle.owner_=state->identity;handle.identity_=slot.identity;
         state->slots.push_back(std::move(slot));work.reservation=handle.identity_;
        }
        // No registry lock spans resource construction/publication or callbacks.
        auto reference=native?state->native_session->acquire_runtime_program(*native,selector,f.preparation,f.native_bank):
            state->service->acquire_runtime_program(*c,f.preparation,f.service);
        if(!reference){f.kind=native?PlaybackError::native_bank_failed:PlaybackError::service_failed;return {};}
        auto plan=prepare_play_plan(reference,p,f);if(!plan)return {};
        if(!commit_play_plan(state,context,handle,*plan,sample_activation_clock,f))return {};
        work.reservation.reset();
        return handle;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
    catch(...){f.kind=PlaybackError::native_exception;}return {};
}

struct NativePlayWork::Impl {
    // Release the public reservation only after all actual private resources.
    std::unique_ptr<NativePlaybackOwner::State::Activity> admission;
    std::shared_ptr<RuntimeProgramWork> runtime;
    std::unique_ptr<RuntimeProgramReference> reference;
    std::unique_ptr<NativePreparedPlay> plan;
    NativePlayHandle handle;
    NativeSoundContext context;
    NativePlayParameters parameters;
    mutable std::mutex mutex;
    std::thread::id worker;
    NativePlayWorkObservation observation;
    bool body_started=false,completing=false;
    explicit Impl(const NativePlayParameters& p):parameters(p){}
    ~Impl(){plan.reset();reference.reset();runtime.reset();admission.reset();}
    bool worker_access(PlaybackFailure& f)const {
        if(worker==std::thread::id{}||worker==std::this_thread::get_id())return true;
        f.kind=PlaybackError::wrong_thread;return false;
    }
};
NativePlayWork::~NativePlayWork()=default;
std::shared_ptr<NativePlayWork> NativePlaybackOwner::admit_queued_play(const NativeSoundContext& context,
    const AudioBankHandle& bank,std::int32_t selector,const NativeActivationParameters& p,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    if(!state->native_session){f.kind=PlaybackError::wrong_bank_interface;return {};}
    try {
        NativePlayParameters parameters{p.context,p.program,p.envelope,{0},p.source_output_mode,p.caller_play_flags,p.output};
        auto data=std::make_shared<NativePlayWork::Impl>(parameters);
        auto result=std::shared_ptr<NativePlayWork>(new NativePlayWork(data));
        data->admission=std::make_unique<State::Activity>();data->context=context;
        {std::lock_guard lock(state->mutex);
         if(!state->available(f)||!state->context(context,f))return {};
         if(const auto gate=state->unloading_state(bank)){f.kind=*gate?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return {};}
         if(!state->admit(state,*data->admission,f))return {};
         State::Slot slot;slot.identity=std::make_shared<int>(0);slot.entry=std::make_shared<State::Entry>(bank,parameters,context.identity_);
         slot.entry->identity=slot.identity;data->handle.owner_=state->identity;data->handle.identity_=slot.identity;
         state->slots.push_back(std::move(slot));data->admission->reservation=data->handle.identity_;
        }
        data->runtime=state->native_session->admit_runtime_program(bank,selector,f.preparation,f.native_bank);
        if(!data->runtime){f.kind=PlaybackError::native_bank_failed;return {};}
        // All fallible admission allocations precede public identity exposure.
        data->admission->public_reservation=true;return result;
    }catch(const std::bad_alloc&){allocation_failure(f);}
     catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
     catch(...){f.kind=PlaybackError::native_exception;}return {};
}
NativePlayHandle NativePlayWork::handle()const {return state_->handle;}
bool NativePlayWork::cached_runtime_available()const {
    std::lock_guard lock(state_->mutex);return state_->runtime&&state_->runtime->cached_runtime_available();
}
bool NativePlayWork::begin_build(PlaybackFailure& f){
    f={};auto s=state_;std::lock_guard lock(s->mutex);
    if(s->observation.state!=NativePlayWorkState::queued){f.kind=PlaybackError::invalid_work_state;return false;}
    if(!s->runtime->begin_build(f.preparation)){f.kind=PlaybackError::native_bank_failed;return false;}
    s->worker=std::this_thread::get_id();s->observation.state=NativePlayWorkState::building;return true;
}
bool NativePlayWork::build(PlaybackFailure& f){
    f={};auto s=state_;
    {std::lock_guard lock(s->mutex);if(!s->worker_access(f))return false;
     if(s->observation.state!=NativePlayWorkState::building||s->body_started){f.kind=PlaybackError::invalid_work_state;return false;}s->body_started=true;}
    s->runtime->build(f.preparation);PreparationFailure decision;
    if(s->runtime->finish_build(decision))s->reference=s->runtime->take_result(f.preparation);
    else f.preparation=decision;
    const auto runtime=s->runtime->observe();
    std::lock_guard lock(s->mutex);s->observation.branch=runtime.branch;
    s->observation.runtime_ready=bool(s->reference);s->observation.cancelled=runtime.state==RuntimeProgramWorkState::cancelled;
    s->observation.state=NativePlayWorkState::runtime_built;
    if(!s->reference&&!s->observation.cancelled){f.kind=PlaybackError::native_bank_failed;s->observation.failure=f;}
    return bool(s->reference);
}
bool NativePlayWork::complete(bool admitted,PlaybackFailure& f){
    f={};auto s=state_;
    {std::lock_guard lock(s->mutex);if(!s->worker_access(f))return false;
     if(s->observation.state!=NativePlayWorkState::runtime_built||s->completing){f.kind=PlaybackError::invalid_work_state;return false;}s->completing=true;}
    // Actual construction resources must leave before any context consumer.
    // The outer bank/association reservation remains held across this gap.
    bool done=true,activated=false,callback=false;
    try {
        if(s->runtime->observe().state!=RuntimeProgramWorkState::retired&&!s->runtime->retire(f.preparation)){
            f.kind=PlaybackError::native_bank_failed;done=false;
        }else {
            {std::lock_guard lock(s->mutex);callback=!s->observation.cancelled;
             if(callback)s->observation.ordinary_callback_invoked=true;}
            if(callback){
                if(!admitted){f.kind=PlaybackError::closed;}
                else if(!s->reference&&!s->plan){std::lock_guard lock(s->mutex);if(s->observation.failure)f=*s->observation.failure;}
                else {
                    if(!s->plan)s->plan=NativePlaybackOwner::prepare_play_plan(s->reference,s->parameters,f);
                    if(s->plan){activated=NativePlaybackOwner::commit_play_plan(s->admission->state,s->context,s->handle,*s->plan,true,f);
                        if(!activated&&f.kind==PlaybackError::operation_active)done=false;}
                }
            }
        }
    }catch(const std::bad_alloc&){f.kind=PlaybackError::allocation_failed;}
     catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}
     catch(...){f.kind=PlaybackError::native_exception;}
    std::lock_guard lock(s->mutex);s->completing=false;
    if(done){s->observation.activated=activated;s->observation.pending=false;s->observation.state=NativePlayWorkState::callback_complete;
        if(f.kind!=PlaybackError::none)s->observation.failure=f;}
    return done&&f.kind==PlaybackError::none;
}
bool NativePlayWork::retire(PlaybackFailure& f){
    f={};auto s=state_;
    {std::lock_guard lock(s->mutex);if(!s->worker_access(f))return false;
     if(s->observation.state!=NativePlayWorkState::callback_complete||s->completing){f.kind=PlaybackError::invalid_work_state;return false;}s->completing=true;}
    std::shared_ptr<RuntimeProgramWork> runtime;
    {std::lock_guard lock(s->mutex);runtime=std::move(s->runtime);}
    s->plan.reset();s->reference.reset();runtime.reset();s->admission.reset();
    std::lock_guard lock(s->mutex);s->completing=false;s->observation.state=NativePlayWorkState::retired;return true;
}
ProgramWorkCancellation NativePlayWork::cancel(){
    auto s=state_;std::lock_guard lock(s->mutex);
    if(s->completing&&s->observation.branch==RuntimeProgramWorkBranch::cached){
        s->observation.cancellation_requested=true;s->observation.pending=false;return ProgramWorkCancellation::running_requested;
    }
    if(!s->runtime||s->completing||s->observation.state==NativePlayWorkState::callback_complete||
        s->observation.state==NativePlayWorkState::retired)return ProgramWorkCancellation::not_cancellable;
    auto answer=s->runtime->cancel();
    if(answer==ProgramWorkCancellation::not_cancellable&&s->runtime->observe().branch==RuntimeProgramWorkBranch::cached)
        answer=ProgramWorkCancellation::running_requested;
    if(answer!=ProgramWorkCancellation::not_cancellable){s->observation.cancellation_requested=true;s->observation.pending=false;}
    if(answer==ProgramWorkCancellation::queued_removed){s->observation.cancelled=true;s->observation.state=NativePlayWorkState::runtime_built;}
    return answer;
}
NativePlayWorkObservation NativePlayWork::observe()const {
    std::lock_guard lock(state_->mutex);return state_->observation;
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
        auto now=state->read_clock(lock,f);
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
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;std::vector<std::unique_ptr<State::Retired>> finished;
    std::unique_lock lock(state->mutex);if(!state->available(f))return {};
    try{
        // All reclamation storage is allocated before the aggregate mix commits.
        finished.reserve(state->retired.size());
        if(!state->retired.empty()&&!state->admit(state,work,f))return {};
        std::vector<HostVoice*> voices;
        for(auto& slot:state->slots)if(slot.entry){auto& e=*slot.entry;if(e.preparing)continue;
            if(e.terminal&&!e.stopped){f=*e.terminal;f.kind=PlaybackError::failed_playback_requires_stop;return {};}
            if(e.copy)e.copy->append_outputs(voices);
        }
        for(auto& group:state->retired)for(auto& voice:group->voices)voices.push_back(&voice);
        auto output=mix_host_voices(voices.data(),voices.size(),frames,f.voice);
        if(!output){f.kind=PlaybackError::voice_failed;return {};}
        for(auto it=state->retired.begin();it!=state->retired.end();)if((*it)->resources_released&&!(*it)->active()){
            finished.push_back(std::move(*it));it=state->retired.erase(it);
        }else ++it;
        lock.unlock();finished.clear();return output;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return {};
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
     if(!state->admit(state,work,f))return false;state->detach_entry(*slot->entry);
     slot->retiring=true;retired=std::move(slot->entry);
    }
    retired.reset();
    {std::lock_guard lock(state->mutex);for(auto& slot:state->slots)if(slot.identity==h.identity_){slot.retiring=false;break;}}
    return true;
}
std::optional<ForcedPlaybackRetirement> NativePlaybackOwner::force_retire(const NativePlayHandle& handle,PlaybackFailure& f){
    auto result=force_retire_impl(&handle,nullptr,f);if(!result)return {};return std::move(result->front());
}
std::optional<std::vector<ForcedPlaybackRetirement>> NativePlaybackOwner::force_retire_context_outputs(const NativeSoundContext& context,PlaybackFailure& f){
    return force_retire_impl(nullptr,&context,f);
}
std::optional<std::vector<ForcedPlaybackRetirement>> NativePlaybackOwner::force_retire_impl(
    const NativePlayHandle* handle,const NativeSoundContext* context,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    State::Activity work;std::vector<std::shared_ptr<State::Entry>> targets,disposed_entries;
    std::vector<std::unique_ptr<NativeProgramCopy>> copies;
    std::vector<std::unique_ptr<State::Retired>> groups;
    std::vector<ForcedPlaybackRetirement> receipts;
    try {
        {std::lock_guard lock(state->mutex);if(!state->available(f))return {};
         State::Context* scope=nullptr;
         if(handle){auto* slot=state->find(handle->owner_,handle->identity_,f);if(!slot)return {};
             scope=state->context_identity(slot->entry->context);
             if(!scope||!scope->live){f.kind=PlaybackError::retired_context;return {};}
             if(scope->cleaning){f.kind=PlaybackError::operation_active;return {};}
         }else {scope=state->context(*context,f);if(!scope)return {};}
         std::size_t count=0;
         for(const auto& slot:state->slots)if(slot.entry&&slot.entry->context==scope->identity&&(!handle||slot.identity==handle->identity_)){
             if(slot.entry->preparing){f.kind=PlaybackError::operation_active;return {};}
             if(const auto gate=state->unloading_state(slot.entry->bank)){f.kind=*gate?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return {};}++count;
         }
         targets.reserve(count);disposed_entries.reserve(count);copies.reserve(count);groups.reserve(count);receipts.reserve(count);
         if(count>state->retired.max_size()-state->retired.size()){f.kind=PlaybackError::host_capacity;return {};}
         state->retired.reserve(state->retired.size()+count);
         for(const auto& slot:state->slots)if(slot.entry&&slot.entry->context==scope->identity&&(!handle||slot.identity==handle->identity_)){
             const auto& entry=slot.entry;auto group=std::make_unique<State::Retired>(entry->bank);
             group->identity=std::make_shared<int>(0);group->bank_identity=entry->bank_identity;
             group->context=entry->context;group->origin=entry->identity;group->voices.reserve(entry->outputs().voices);
             if(!group->bank_identity){f.kind=PlaybackError::copy_failed;f.copy.kind=ProgramCopyError::invalid_transfer_provenance;return {};}
             if(entry->copy&&!entry->copy->validate_forced_output_transfer(group->voices,f.copy)){f.kind=PlaybackError::copy_failed;return {};}
             targets.push_back(entry);groups.push_back(std::move(group));
         }
         if(!state->admit(state,work,f))return {};
         scope->cleaning=true;work.clearing_context=scope->identity;
         // All validation/allocation precedes output mutation. Each transfer is
         // serialized and nonallocating; all group owners survive outside copies.
         for(std::size_t n=0;n<targets.size();++n){auto& entry=*targets[n];auto& group=groups[n];
             if(entry.copy&&!entry.copy->transfer_forced_outputs(group->voices,f.copy)){f.kind=PlaybackError::copy_failed;return {};}
             ForcedPlaybackRetirement receipt;receipt.outputs.owner_=state->identity;receipt.outputs.identity_=group->identity;
             receipt.voices=group->voices.size();receipts.push_back(std::move(receipt));
             state->retired.push_back(std::move(group));
             state->detach_entry(entry);entry.stopped=true;entry.finished=true;
             copies.push_back(std::move(entry.copy));
             for(auto& slot:state->slots)if(slot.identity==entry.identity){
                 slot.retiring=true;disposed_entries.push_back(std::move(slot.entry));break;
             }
         }
        }
        // Semantic/runtime/Service cleanup must finish after actual clone
        // disposal and outside the registry mutex. Pool owns no such reference.
        copies.clear();disposed_entries.clear();targets.clear();
        {std::lock_guard lock(state->mutex);
         for(auto& receipt:receipts){for(auto& group:state->retired)if(group->identity==receipt.outputs.identity_){
                 group->resources_released=true;
                 for(auto& slot:state->slots)if(slot.identity==group->origin){slot.retiring=false;break;}
                 break;
             }
             receipt.native_resources_released=true;
         }
        }
        return std::optional<std::vector<ForcedPlaybackRetirement>>(std::move(receipts));
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return {};
}
std::optional<RetiredOutputObservation> NativePlaybackOwner::observe_retired(const NativeRetiredOutputHandle& handle,PlaybackFailure& f)const{
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f))return {};
    if(!handle.owner_||!handle.identity_){f.kind=PlaybackError::unknown_retired_output;return {};}
    if(handle.owner_!=state->identity){f.kind=PlaybackError::foreign_owner;return {};}
    for(const auto& group:state->retired)if(group->identity==handle.identity_){
        RetiredOutputObservation result;result.voices=group->voices.size();result.active_voices=group->active();
        if(!group->voices.empty())result.first_voice_source_index=group->voices.front().source_index();
        const auto* wire=std::get_if<BankWireToken>(&group->bank);
        if(!wire){f.kind=PlaybackError::wrong_bank_interface;return {};}
        result.original_bank=*wire;result.native_resources_released=group->resources_released;return result;
    }
    f.kind=PlaybackError::retired_output;return {};
}
std::optional<NativeRetiredOutputObservation> NativePlaybackOwner::observe_native_retired(const NativeRetiredOutputHandle& handle,PlaybackFailure& f)const{
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return {};}
    std::lock_guard lock(state->mutex);if(!state->available(f))return {};
    if(!handle.owner_||!handle.identity_){f.kind=PlaybackError::unknown_retired_output;return {};}
    if(handle.owner_!=state->identity){f.kind=PlaybackError::foreign_owner;return {};}
    for(const auto& group:state->retired)if(group->identity==handle.identity_){
        const auto* bank=std::get_if<AudioBankHandle>(&group->bank);
        if(!bank){f.kind=PlaybackError::wrong_bank_interface;return {};}
        NativeRetiredOutputObservation result;result.voices=group->voices.size();result.active_voices=group->active();
        if(!group->voices.empty())result.first_voice_source_index=group->voices.front().source_index();
        result.original_bank=*bank;result.native_resources_released=group->resources_released;return result;
    }
    f.kind=PlaybackError::retired_output;return {};
}
bool NativePlaybackOwner::unload(const AudioBankCommand& c,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}
    if(c.opcode!=AudioBankOpcode::unload){f.kind=PlaybackError::service_failed;f.service.kind=BankServiceError::wrong_command;return false;}
    return unload_impl(&c,nullptr,f);
}
bool NativePlaybackOwner::unload(const AudioBankHandle& bank,PlaybackFailure& f){
    return unload_impl(nullptr,&bank,f);
}
bool NativePlaybackOwner::unload_impl(const AudioBankCommand* c,const AudioBankHandle* native,PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    if((native&&!state->native_session)||(!native&&!state->service)){f.kind=PlaybackError::wrong_bank_interface;return false;}
    const State::BankReference bank=native?State::BankReference(*native):State::BankReference(c->bank_token);
    State::Activity work;std::vector<std::unique_ptr<NativeProgramCopy>> copies;
    std::vector<std::shared_ptr<State::Entry>> retired;bool downstream_started=false;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f))return false;
         if(const auto gate=state->unloading_state(bank)){f.kind=*gate?PlaybackError::bank_state_unknown:PlaybackError::bank_unloading;return false;}
         std::size_t count=0;
         for(const auto& slot:state->slots)if(slot.entry&&State::same_bank(slot.entry->bank,bank)){
             if(slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}++count;
         }
         copies.reserve(count);retired.reserve(count);
         if(!state->admit(state,work,f))return false;
         state->unloading.push_back({bank,false});work.bank=bank;
         for(auto& slot:state->slots)if(slot.entry&&State::same_bank(slot.entry->bank,bank)&&slot.entry->copy)copies.push_back(slot.entry->retire_copy());
        }
        copies.clear();downstream_started=true;
        if(native?!state->native_session->unload_bank(*native,f.native_bank):!state->service->unload(*c,f.service)){
            work.uncertain=!native&&(f.service.kind==BankServiceError::native_exception||f.service.kind==BankServiceError::bank_state_unknown);
            f.kind=native?PlaybackError::native_bank_failed:PlaybackError::service_failed;return false;
        }
        {std::lock_guard lock(state->mutex);
         for(auto& slot:state->slots)if(slot.entry&&State::same_bank(slot.entry->bank,bank)){state->detach_entry(*slot.entry);retired.push_back(std::move(slot.entry));}
        }
        retired.clear();return true;
    }catch(const std::bad_alloc&){work.uncertain=downstream_started;allocation_failure(f);}catch(const std::length_error&){work.uncertain=downstream_started;f.kind=PlaybackError::host_capacity;}
    catch(...){work.uncertain=downstream_started;f.kind=PlaybackError::native_exception;}return false;
}
bool NativePlaybackOwner::close_when_idle(PlaybackFailure& f){
    f={};auto state=state_;if(!state){f.kind=PlaybackError::moved_owner;return false;}
    State::Activity work;std::vector<std::unique_ptr<NativeProgramCopy>> copies;
    std::vector<std::shared_ptr<State::Entry>> retired;std::vector<std::unique_ptr<State::Retired>> outputs;bool downstream_started=false;
    try {
        {std::lock_guard lock(state->mutex);
         if(!state->available(f))return false;
         if(state->operations){f.kind=PlaybackError::operation_active;return false;}
         std::size_t count=0;
         for(const auto& slot:state->slots)if(slot.entry){if(slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}++count;}
         for(const auto& group:state->retired)if(group->active()){f.kind=PlaybackError::active_playback;return false;}
         copies.reserve(count);retired.reserve(count);outputs.reserve(state->retired.size());
         if(!state->admit(state,work,f))return false;
         state->closing=true;work.closes=true;
         for(auto& group:state->retired)outputs.push_back(std::move(group));state->retired.clear();
         for(auto& slot:state->slots)if(slot.entry&&slot.entry->copy)copies.push_back(slot.entry->retire_copy());
        }
        copies.clear();outputs.clear();downstream_started=true;
        if(state->native_session?!state->native_session->close(f.native_bank):!state->service->close_when_idle(f.service)){
            f.kind=state->native_session?PlaybackError::native_bank_failed:PlaybackError::service_failed;return false;
        }
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
