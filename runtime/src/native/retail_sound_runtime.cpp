#include "mka/retail_sound_runtime.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <system_error>
#include <thread>
namespace mka::retail {
struct NativeRetailSoundRuntime::State {
    explicit State(sound::AudioBankSession&& session):incoming(std::move(session)){}
    const std::thread::id owner{std::this_thread::get_id()};
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::thread worker;
    std::optional<sound::AudioBankSession> incoming;
    std::shared_ptr<sound::NativePlaybackOwner> playback;
    std::optional<sound::NativeSoundContext> context;
    RetailSoundRuntimeObservation observation;
    // Actual retained context values. Future source control effects must mutate
    // these under this mutex; they are not inferred from per-play parameters.
    const sound::AudioControlFactors controls{NativeRetailSoundRuntime::initial_context_controls()};
    RetailSoundRuntimeFailure failure;
    bool initialized{},stop_requested{},closed{},pause_requested{},paused{},worker_finished{},cleanup_failed{},drain_requested{},drained{};
    void fail(RetailSoundRuntimeFailure value) noexcept {
        std::lock_guard lock(mutex);
        if(failure.kind==RetailSoundRuntimeError::none)failure=std::move(value);
        else if(value.output.kind!=sound::AudioDeviceError::none)failure.cleanup=value.output;
        condition.notify_all();
    }
    bool owner_access(RetailSoundRuntimeFailure& f) const noexcept {
        f={};if(owner==std::this_thread::get_id())return true;
        f.kind=RetailSoundRuntimeError::wrong_thread;f.operation="native sound runtime Main thread";return false;
    }
    void run() noexcept {
        struct Finished {
            State& state;
            ~Finished(){std::lock_guard lock(state.mutex);state.worker_finished=true;state.condition.notify_all();}
        } finished{*this};
        std::unique_ptr<sound::NativeSoundOutput> output;
        RetailSoundRuntimeFailure f;
        try {
            output=sound::open_native_game_sound_output(f.output);
            if(!output){f.kind=RetailSoundRuntimeError::output;f.operation="open actual native game output";fail(f);return;}
            const auto format=output->format();
            if(!format.sample_rate||format.channels!=2||format.channel_mask!=3){
                f.kind=RetailSoundRuntimeError::invalid_output_format;f.operation="native mixer requires actual stereo float client format";fail(f);
            }else {
                auto created=sound::NativePlaybackOwner::create(std::move(*incoming),format.sample_rate,
                    sound::AudioControlCurve::startup_linear,f.playback);
                incoming.reset();
                if(!created){f.kind=RetailSoundRuntimeError::playback;f.operation="create source-curve native Session playback";fail(f);}
                else {
                    auto native_owner=std::make_shared<sound::NativePlaybackOwner>(std::move(*created));
                    auto native_context=native_owner->create_sound_context({3},f.playback);
                    if(!native_context){f.kind=RetailSoundRuntimeError::playback;f.operation="create source explicit-role context3";fail(f);}
                    else {
                        {std::lock_guard lock(mutex);playback=native_owner;context=*native_context;
                         observation.format=format;initialized=true;condition.notify_all();}
                        pump(*output,*native_owner,*native_context);
                    }
                }
            }
        }catch(const std::bad_alloc&){f.kind=RetailSoundRuntimeError::allocation;f.operation="native sound worker allocation";fail(f);}
         catch(const std::length_error&){f.kind=RetailSoundRuntimeError::host_capacity;f.operation="native sound worker capacity";fail(f);}
         catch(...){f.kind=RetailSoundRuntimeError::native_exception;f.operation="native sound worker exception";fail(f);}
        if(output){
            sound::AudioDeviceFailure cleanup;
            if(!output->close(cleanup)){RetailSoundRuntimeFailure failure;failure.kind=RetailSoundRuntimeError::output;
                failure.operation="close native worker output";failure.output=cleanup;
                {std::lock_guard lock(mutex);cleanup_failed=true;}
                fail(failure);}
            // The concrete output destroys all WASAPI interfaces and its STA
            // COM lease here, on their actual owning native thread.
            output.reset();
        }
    }
    std::optional<sound::AudioQueueStatus> query_output(sound::NativeSoundOutput& output,
        std::uint64_t submitted,bool started,RetailSoundRuntimeFailure& f){
        auto status=output.queue_status(f.output);
        if(!status){f.kind=RetailSoundRuntimeError::output;f.operation="native output queue observation";return {};}
        if(!status->capacity_frames||status->padding_frames>status->capacity_frames||
           status->writable_frames!=status->capacity_frames-status->padding_frames||
           status->submitted_frames!=submitted||status->reset_epoch!=0||status->started!=started){
            f.kind=RetailSoundRuntimeError::inconsistent_output;f.operation="native output queue ownership/epoch";return {};
        }
        return status;
    }
    bool submit_pending(sound::NativeSoundOutput& output,const sound::AudioQueueStatus& status,
        std::vector<float>& pending,std::uint64_t& submitted,bool& started,RetailSoundRuntimeFailure& f){
        if(pending.empty()||pending.size()/2>status.writable_frames)return true;
        f.mixed_unsubmitted_frames=pending.size()/2;
        auto receipt=output.queue(pending.data(),pending.size(),f.output);
        if(!receipt){
            if(f.output.kind==sound::AudioDeviceError::buffer_full)return true;
            f.kind=RetailSoundRuntimeError::output;f.operation="queue actual native mixed PCM";return false;
        }
        f.submission=*receipt;
        if(receipt->reset_epoch!=0||receipt->first_client_frame!=submitted||
           receipt->client_frames!=pending.size()/2||receipt->client_frames>std::numeric_limits<std::uint64_t>::max()-submitted){
            f.kind=RetailSoundRuntimeError::inconsistent_output;f.operation="native mixed PCM submission receipt";return false;
        }
        submitted+=receipt->client_frames;pending.clear();f.mixed_unsubmitted_frames=0;
        if(!started){
            if(!output.start(f.output)){f.kind=RetailSoundRuntimeError::output;f.operation="start prequeued native output";return false;}
            started=true;
        }
        return true;
    }
    void remember_output(const std::vector<float>& pending,const sound::AudioQueueStatus& status,
                         std::uint64_t submitted,bool started){
        // Caller holds the lifecycle mutex. Padding is an actual queried client
        // buffer observation; it is never converted into a heard-time claim.
        observation.pending_pcm_frames=pending.size()/2;observation.observed_padding_frames=status.padding_frames;
        observation.submitted_frames=submitted;observation.output_started=started;
    }
    void pump(sound::NativeSoundOutput& output,sound::NativePlaybackOwner& owner,
              const sound::NativeSoundContext& context){
        std::vector<float> pending;
        std::uint64_t submitted=0;
        bool started=false;
        for(;;){
            {std::unique_lock lock(mutex);
             condition.wait_for(lock,std::chrono::microseconds(sound::retail_sound_worker_microseconds),
                 [&]{return stop_requested||pause_requested;});
             if(stop_requested)return;
             if(pause_requested){
                 paused=true;condition.notify_all();
                 while(pause_requested&&!stop_requested){
                     condition.wait(lock,[&]{return stop_requested||!pause_requested||drain_requested;});
                     if(stop_requested||!pause_requested)break;
                     lock.unlock();
                     RetailSoundRuntimeFailure f;
                     auto status=query_output(output,submitted,started,f);
                     if(!status){fail(f);return;}
                     // A zero-padding observation only proves retirement when
                     // no mixed PCM remains. After a submission query again on
                     // the next pass, rather than reusing pre-submit padding.
                     const bool complete=pending.empty()&&status->padding_frames==0;
                     if(!complete&&!submit_pending(output,*status,pending,submitted,started,f)){fail(f);return;}
                     lock.lock();remember_output(pending,*status,submitted,started);
                     if(complete){drained=true;condition.notify_all();
                         condition.wait(lock,[&]{return stop_requested||!pause_requested;});}
                     else condition.wait_for(lock,std::chrono::microseconds(sound::retail_sound_worker_microseconds),
                         [&]{return stop_requested||!pause_requested;});
                 }
                 paused=false;
                 if(stop_requested)return;
                 continue;
             }}
            RetailSoundRuntimeFailure f;
            sound::SoundContextUpdate update;
            if(!owner.update_ordinary_context(context,update,f.playback)){
                // Real cleanup may own the context across an unlocked resource
                // gap. It is transient exclusion, not a successful empty pass
                // and not a permanent audio failure.
                if(f.playback.kind==sound::PlaybackError::operation_active)continue;
                f.kind=RetailSoundRuntimeError::playback;f.operation="native admitted ordinary program worker";fail(f);return;
            }
            auto ownership=owner.observe_all_outputs(f.playback);
            if(!ownership){f.kind=RetailSoundRuntimeError::playback;f.operation="native output ownership observation";fail(f);return;}
            if(!ownership->active_voices&&ownership->retired_groups){
                // Reclaim actually finished native tail groups without creating
                // an unnecessary silence block or advancing any voice.
                if(!owner.mix(0,f.playback)){f.kind=RetailSoundRuntimeError::playback;
                    f.operation="retire finished native output groups";fail(f);return;}
            }
            auto status=query_output(output,submitted,started,f);
            if(!status){fail(f);return;}
            if(pending.empty()&&status->writable_frames&&(!started||ownership->active_voices)){
                auto mixed=owner.mix(status->writable_frames,f.playback);
                if(!mixed){f.kind=RetailSoundRuntimeError::playback;f.operation="mix actual native program outputs";fail(f);return;}
                pending=std::move(*mixed);
            }
            if(!submit_pending(output,*status,pending,submitted,started,f)){fail(f);return;}
            {std::lock_guard lock(mutex);remember_output(pending,*status,submitted,started);
             if(observation.worker_passes==std::numeric_limits<std::uint64_t>::max()){
                 failure.kind=RetailSoundRuntimeError::host_capacity;failure.operation="native sound worker observation count";condition.notify_all();return;
             }
             ++observation.worker_passes;}
        }
    }
};
NativeRetailSoundRuntime::NativeRetailSoundRuntime(std::shared_ptr<State> state):state_(std::move(state)){}
sound::AudioControlFactors NativeRetailSoundRuntime::initial_context_controls() noexcept {
    return {0x4000,0,0x1000,0x4000,0,0x1000};
}
std::unique_ptr<NativeRetailSoundRuntime> NativeRetailSoundRuntime::create(sound::AudioBankSession&& session,RetailSoundRuntimeFailure& f){
    f={};
    try {
        auto state=std::make_shared<State>(std::move(session));
        auto runtime=std::unique_ptr<NativeRetailSoundRuntime>(new NativeRetailSoundRuntime(state));
        state->worker=std::thread([state]{state->run();});
        std::unique_lock lock(state->mutex);
        state->condition.wait(lock,[&]{return state->initialized||state->failure.kind!=RetailSoundRuntimeError::none;});
        if(state->failure.kind!=RetailSoundRuntimeError::none){
            lock.unlock();runtime.reset();
            std::lock_guard final_lock(state->mutex);f=state->failure;return {};
        }
        return runtime;
    }catch(const std::bad_alloc&){f.kind=RetailSoundRuntimeError::allocation;f.operation="native sound runtime allocation";}
     catch(const std::length_error&){f.kind=RetailSoundRuntimeError::host_capacity;f.operation="native sound runtime capacity";}
     catch(const std::system_error& e){f.kind=RetailSoundRuntimeError::native_exception;f.operation="native sound thread creation";f.native_code=e.code().value();}
     catch(...){f.kind=RetailSoundRuntimeError::native_exception;f.operation="native sound thread creation";}
    return {};
}
NativeRetailSoundRuntime::~NativeRetailSoundRuntime(){
    RetailSoundRuntimeFailure f;
    if(!shutdown(f)){
        std::fprintf(stderr,"native sound runtime teardown refused: %s (kind=%d)\n",f.operation,static_cast<int>(f.kind));
        std::lock_guard lock(state_->mutex);
        if(!state_->closed)std::terminate();
    }
}
std::shared_ptr<sound::NativePlaybackOwner> NativeRetailSoundRuntime::playback_owner() const {
    std::lock_guard lock(state_->mutex);return state_->playback;
}
sound::NativeSoundContext NativeRetailSoundRuntime::sound_context() const {
    std::lock_guard lock(state_->mutex);return state_->context.value();
}
std::optional<RetailSoundRuntimeObservation> NativeRetailSoundRuntime::observe(RetailSoundRuntimeFailure& f) const {
    if(!state_->owner_access(f))return {};
    std::lock_guard lock(state_->mutex);
    if(state_->failure.kind!=RetailSoundRuntimeError::none){f=state_->failure;return {};}
    if(state_->closed){f.kind=RetailSoundRuntimeError::closed;f.operation="native sound runtime closed";return {};}
    return state_->observation;
}
std::optional<RetailSoundRuntimeStatus> NativeRetailSoundRuntime::snapshot_status(RetailSoundRuntimeFailure& f) const {
    if(!state_->owner_access(f))return {};
    std::lock_guard lock(state_->mutex);
    if(state_->failure.kind!=RetailSoundRuntimeError::none){f=state_->failure;return {};}
    if(state_->closed){f.kind=RetailSoundRuntimeError::closed;f.operation="native sound status after shutdown";return {};}
    auto active=state_->playback->snapshot_active_plays(*state_->context,f.playback);
    if(!active){f.kind=RetailSoundRuntimeError::playback;f.operation="snapshot actual native sound associations";return {};}
    return RetailSoundRuntimeStatus{state_->controls,std::move(*active),state_->observation};
}
bool NativeRetailSoundRuntime::shutdown(RetailSoundRuntimeFailure& f){
    if(!state_->owner_access(f))return false;
    {std::unique_lock lock(state_->mutex);
     if(state_->closed)return true;
     // Wait for the actual worker to leave clock/mixer mutation before the
     // idle check. An observation during the unlocked QPC sample is not proof
     // of live playback and must not randomly make idle teardown fail.
     state_->pause_requested=true;state_->condition.notify_all();
     if(state_->worker.joinable())state_->condition.wait(lock,[&]{return state_->paused||state_->worker_finished;});}
    if(state_->playback){
        auto active=state_->playback->observe_all_outputs(f.playback);
        if(!active||active->pending_preparations||active->programs||active->active_voices||
           active->retired_groups||active->retired_voices||active->active_retired_voices){
            f.kind=RetailSoundRuntimeError::active_playback;f.operation="native sound shutdown requires actual idle program/output ownership";
            std::lock_guard lock(state_->mutex);state_->pause_requested=false;state_->condition.notify_all();return false;
        }
    }
    bool drain_failed=false;
    {std::unique_lock lock(state_->mutex);
     if(state_->worker.joinable()&&!state_->worker_finished){
         state_->drain_requested=true;state_->condition.notify_all();
         state_->condition.wait(lock,[&]{return state_->drained||state_->worker_finished;});
         drain_failed=!state_->drained;
     }
     state_->stop_requested=true;state_->condition.notify_all();}
    if(state_->worker.joinable())state_->worker.join();
    {std::lock_guard lock(state_->mutex);state_->closed=true;
     if(state_->cleanup_failed||drain_failed){f=state_->failure;return false;}}
    return true;
}
}
