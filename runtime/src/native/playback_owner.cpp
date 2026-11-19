#include "mka/playback_owner.hpp"
#include <mutex>
#include <new>
#include <stdexcept>
namespace mka::sound {
struct NativePlaybackOwner::State {
    struct Entry {
        BankWireToken bank;
        NativePlayParameters parameters;
        ProgramExecutor executor;
        std::vector<HostVoice> voices;
        bool finished=false,stopped=false;
        std::optional<PlaybackFailure> terminal;
        Entry(BankWireToken b,const NativePlayParameters& p,ProgramExecutor&& e):bank(b),parameters(p),executor(std::move(e)){}
        bool busy()const {if(!finished&&!stopped)return true;for(const auto& v:voices)if(v.active())return true;return false;}
        bool stop_voices(const ProgramStep& event,PlaybackFailure& f){
            for(auto& v:voices)if(!v.stop_from_program(event,f.voice)){f.kind=PlaybackError::voice_failed;return false;}
            return true;
        }
    };
    struct Slot {std::shared_ptr<const void> identity;std::unique_ptr<Entry> entry;};
    mutable std::mutex mutex;
    AudioBankService service;
    std::shared_ptr<const void> identity;
    std::uint32_t rate;
    bool closed=false;
    std::vector<Slot> slots;
    State(AudioBankService&& s,std::uint32_t r,std::shared_ptr<const void> id):service(std::move(s)),identity(std::move(id)),rate(r){}
    bool available(PlaybackFailure& f)const {if(closed){f.kind=PlaybackError::closed;return false;}return true;}
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
std::optional<BankLoadOutcome> NativePlaybackOwner::load_sync(const AudioBankCommand& c,BankWireToken token,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto r=state_->service.load_sync(c,token,f.service);if(!r)f.kind=PlaybackError::service_failed;return r;
}
std::optional<AsyncBankSubmission> NativePlaybackOwner::submit_async(const AudioBankCommand& c,BankWireToken token,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto r=state_->service.submit_async(c,token,f.service);if(!r)f.kind=PlaybackError::service_failed;return r;
}
bool NativePlaybackOwner::run_one(PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    if(!state_->service.run_one(f.service)){f.kind=PlaybackError::service_failed;return false;}return true;
}
std::optional<BankLoadCompletion> NativePlaybackOwner::poll_completion(PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto r=state_->service.poll_completion(f.service);if(!r&&f.service.kind!=BankServiceError::none)f.kind=PlaybackError::service_failed;return r;
}
std::optional<NativePlayHandle> NativePlaybackOwner::play(const AudioBankCommand& c,const NativePlayParameters& p,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto program=state_->service.prepare_program(c,f.preparation,f.service);if(!program){f.kind=PlaybackError::service_failed;return std::nullopt;}
    auto executor=ProgramExecutor::create(std::move(program),p.initial_deadline,f.program);if(!executor){f.kind=PlaybackError::program_failed;return std::nullopt;}
    try{State::Slot slot;slot.identity=std::make_shared<int>(0);slot.entry=std::make_unique<State::Entry>(c.bank_token,p,std::move(*executor));
        NativePlayHandle handle;handle.owner_=state_->identity;handle.identity_=slot.identity;state_->slots.push_back(std::move(slot));return handle;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return std::nullopt;
}
std::optional<ProgramStep> NativePlaybackOwner::tick(const NativePlayHandle& h,ProgramClockWord now,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return std::nullopt;auto& e=*slot->entry;
    if(e.terminal){f=*e.terminal;return std::nullopt;}
    auto event=e.executor.step(now,f.program);if(!event){f.kind=PlaybackError::program_failed;e.terminal=f;return std::nullopt;}
    if(event->kind==ProgramStepKind::sample_start){
        auto voice=HostVoice::start(*event,e.parameters.context,e.parameters.program,e.parameters.source_output_mode,e.parameters.envelope,state_->rate,f.voice);
        if(!voice){f.kind=PlaybackError::voice_failed;e.executor.acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;}
        try{e.voices.push_back(std::move(*voice));}
        catch(const std::bad_alloc&){allocation_failure(f);e.executor.acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;}
        catch(const std::length_error&){f.kind=PlaybackError::host_capacity;e.executor.acknowledge_start(false,f.program);e.terminal=f;return std::nullopt;}
        if(!e.executor.acknowledge_start(true,f.program)){e.voices.pop_back();f.kind=PlaybackError::program_failed;e.terminal=f;return std::nullopt;}
    }else if(event->kind==ProgramStepKind::program_stop){
        if(!e.stop_voices(*event,f)){e.terminal=f;return std::nullopt;}e.finished=true;
    }else if(event->kind==ProgramStepKind::structural_end||event->kind==ProgramStepKind::finished)e.finished=true;
    return event;
}
bool NativePlaybackOwner::stop(const NativePlayHandle& h,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return false;auto& e=*slot->entry;
    auto event=e.executor.stop_from_owner(f.program);if(!event){f.kind=PlaybackError::program_failed;return false;}
    if(!e.stop_voices(*event,f))return false;e.stopped=true;return true;
}
std::optional<std::vector<float>> NativePlaybackOwner::mix(std::size_t frames,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    try{std::vector<HostVoice*> voices;
        for(auto& slot:state_->slots)if(slot.entry){auto& e=*slot.entry;
            if(e.terminal&&!e.stopped){f=*e.terminal;f.kind=PlaybackError::failed_playback_requires_stop;return std::nullopt;}
            for(auto& voice:e.voices)voices.push_back(&voice);
        }
        auto output=mix_host_voices(voices.data(),voices.size(),frames,f.voice);if(!output)f.kind=PlaybackError::voice_failed;return output;
    }catch(const std::bad_alloc&){allocation_failure(f);}catch(const std::length_error&){f.kind=PlaybackError::host_capacity;}return std::nullopt;
}
std::optional<PlaybackObservation> NativePlaybackOwner::observe(const NativePlayHandle& h,PlaybackFailure& f)const{
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return std::nullopt;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return std::nullopt;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return std::nullopt;const auto& e=*slot->entry;
    PlaybackObservation o;o.deadline=e.executor.deadline();o.command_index=e.executor.command_index();o.voices=e.voices.size();if(!e.voices.empty())o.first_voice_source_index=e.voices[0].source_index();for(const auto& v:e.voices)if(v.active())++o.active_voices;o.executor_finished=e.finished;o.explicitly_stopped=e.stopped;o.terminal_failure=e.terminal;return o;
}
bool NativePlaybackOwner::release(const NativePlayHandle& h,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    auto* slot=state_->find(h.owner_,h.identity_,f);if(!slot)return false;if(slot->entry->busy()){f.kind=PlaybackError::active_playback;return false;}slot->entry.reset();return true;
}
bool NativePlaybackOwner::unload(const AudioBankCommand& c,PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    for(const auto& slot:state_->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value&&slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}
    if(!state_->service.unload(c,f.service)){f.kind=PlaybackError::service_failed;return false;}
    for(auto& slot:state_->slots)if(slot.entry&&slot.entry->bank.value==c.bank_token.value)slot.entry.reset();return true;
}
bool NativePlaybackOwner::close_when_idle(PlaybackFailure& f){
    f={};if(!state_){f.kind=PlaybackError::moved_owner;return false;}std::lock_guard<std::mutex> lock(state_->mutex);if(!state_->available(f))return false;
    for(const auto& slot:state_->slots)if(slot.entry&&slot.entry->busy()){f.kind=PlaybackError::active_playback;return false;}
    if(!state_->service.close_when_idle(f.service)){f.kind=PlaybackError::service_failed;return false;}
    state_->closed=true;return true;
}
} // namespace mka::sound
