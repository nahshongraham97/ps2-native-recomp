#include "mka/retail_main.hpp"
#include "mka/retail_current_sound_required.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_sound_runtime.hpp"
#include "mka/retail_sound_status.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
using namespace mka::retail;
namespace sound=mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL assertion: %s\n",why);std::fflush(stderr);throw std::runtime_error(why);}}
enum class Scenario {normal,open_failure,allocation_failure,bad_format,poll_failure,start_failure,close_failure};
struct OutputState {
    std::mutex mutex;
    const std::thread::id main_thread{std::this_thread::get_id()};
    std::thread::id worker_thread;
    Scenario scenario{Scenario::normal};
    unsigned opened{},closed{},destroyed{},starts{},retry_count{};
    std::uint64_t submitted{};
    bool started{},nonzero{},reject_next{},replay_verified{},retired_during_gap{},force_full{};
    unsigned padding_observations{};
    std::uint32_t last_padding{};
    std::function<void()> on_reject;
    std::vector<float> rejected;
};
std::shared_ptr<OutputState> output_state;
thread_local std::int64_t fail_after=-1;
thread_local std::function<void()> disposal_probe;
std::mutex build_hook_mutex;
std::function<void()> before_runtime_build,after_runtime_build;
struct BuildGate {
    std::mutex mutex;std::condition_variable condition;bool entered=false,released=false;
    void block(){std::unique_lock lock(mutex);entered=true;condition.notify_all();
        check(condition.wait_for(lock,std::chrono::seconds(10),[&]{return released;}),"bounded actual resource-worker hold");}
    void release(){std::lock_guard lock(mutex);released=true;condition.notify_all();}
};
std::shared_ptr<BuildGate> block_next_build(bool after=false){
    auto gate=std::make_shared<BuildGate>();std::lock_guard lock(build_hook_mutex);
    (after?after_runtime_build:before_runtime_build)=[gate]{gate->block();};return gate;
}
class TestOutput final:public sound::NativeSoundOutput {
public:
    explicit TestOutput(std::shared_ptr<OutputState> s):state_(std::move(s)){}
    ~TestOutput() override {std::lock_guard lock(state_->mutex);check_thread();check(state_->closed==1,"fake output closed once before destruction");++state_->destroyed;}
    sound::AudioStreamFormat format() const noexcept override {
        return state_->scenario==Scenario::bad_format?sound::AudioStreamFormat{44100,1,4}:sound::AudioStreamFormat{44100,2,3};
    }
    std::optional<sound::AudioQueueStatus> queue_status(sound::AudioDeviceFailure& f) override {
        std::lock_guard lock(state_->mutex);check_thread();f={};
        if(state_->scenario==Scenario::poll_failure){f={sound::AudioDeviceError::device_lost,-73,"fixture queue failure"};return {};}
        const std::uint32_t padding=state_->force_full?128:(state_->padding_observations?64:0);
        if(state_->padding_observations)--state_->padding_observations;
        state_->last_padding=padding;
        return sound::AudioQueueStatus{128,padding,128-padding,state_->submitted,0,state_->started};
    }
    std::optional<sound::AudioSubmission> queue(const float* pcm,std::size_t count,sound::AudioDeviceFailure& f) override {
        std::lock_guard lock(state_->mutex);check_thread();f={};check(count&&count%2==0,"actual complete stereo PCM");
        if(state_->reject_next){state_->reject_next=false;state_->rejected.assign(pcm,pcm+count);++state_->retry_count;
            auto hook=std::move(state_->on_reject);
            if(hook){hook();state_->retired_during_gap=true;state_->padding_observations=200;}
            f={sound::AudioDeviceError::buffer_full,0,"fixture recoverable full queue"};return {};}
        if(!state_->rejected.empty()){
            check(state_->rejected.size()==count&&std::equal(state_->rejected.begin(),state_->rejected.end(),pcm),
                "actual mixed PCM is replayed exactly after buffer-full");state_->rejected.clear();state_->replay_verified=true;
        }
        for(std::size_t i=0;i<count;++i){check(std::isfinite(pcm[i]),"finite actual mixer output");if(pcm[i]!=0)state_->nonzero=true;}
        sound::AudioSubmission receipt{state_->submitted,count/2,0};state_->submitted+=count/2;return receipt;
    }
    bool start(sound::AudioDeviceFailure& f) override {
        std::lock_guard lock(state_->mutex);check_thread();f={};check(state_->submitted>0,"prequeue precedes start");
        if(state_->scenario==Scenario::start_failure){f={sound::AudioDeviceError::start,-81,"fixture start failure"};return false;}
        check(!state_->started,"one native start");state_->started=true;++state_->starts;return true;
    }
    bool close(sound::AudioDeviceFailure& f) noexcept override {
        std::lock_guard lock(state_->mutex);check_thread();check(state_->last_padding==0&&state_->padding_observations==0,
            "submitted client padding is retired before close");
        ++state_->closed;f={};
        if(state_->scenario==Scenario::close_failure){f={sound::AudioDeviceError::stop,-91,"fixture close failure"};return false;}return true;
    }
private:
    void check_thread() const {check(std::this_thread::get_id()==state_->worker_thread&&state_->worker_thread!=state_->main_thread,
        "all native output work stays on its actual worker thread");}
    std::shared_ptr<OutputState> state_;
};
struct Host {
    mka::input::ControllerInput samples;
    std::unique_ptr<NativeRetailStartupState> startup;
    std::unique_ptr<NativeRetailMainState> main;
    explicit Host(const char* root){
        mka::content::ContentVolumeFailure content_failure;
        auto volume=mka::content::NativeContentVolume::open(root,content_failure);check(bool(volume),"real native content root");
        startup=std::make_unique<NativeRetailStartupState>(std::move(*volume),std::vector<std::string>{});
        main=std::make_unique<NativeRetailMainState>(*startup,samples);
        RetailFilePathFailure path_failure;main->file_paths=NativeRetailFilePaths::create(path_failure);
        check(bool(main->file_paths),"real native file service");
    }
};
template<class Predicate> void await(Predicate ready,const char* reason){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!ready()){check(std::chrono::steady_clock::now()<until,reason);std::this_thread::sleep_for(std::chrono::milliseconds(2));}
}
void setup(Scenario scenario){output_state=std::make_shared<OutputState>();output_state->scenario=scenario;}
void exercise_pending(NativeRetailMainState& main,sound::AudioBankSession& session,
    const sound::AudioBankHandle& original,sound::NativeActivationParameters parameters){
    sound::PlaybackFailure pf;RetailSoundRuntimeFailure rf;StartupFailure f;
    auto& runtime=*main.sound_runtime;
    // Explicit software envelope solely for the lifetime/PCM observation.
    parameters.envelope.sustain={1,0,false};parameters.envelope.release={1,-64,false};
    auto keeper=main.playback->activate(*main.current_sound,original,0,parameters,pf);
    check(bool(keeper),"real native cached donor remains owned during queue selection");
    await([&]{auto o=main.playback->observe(*keeper,pf);return o&&o->active_voices>0;},"actual donor starts before resource hold");
    sound::AudioBanksFailure bf;sound::PreparationFailure prep;sound::BankPreparationReport report;
    auto first=session.load_bank("announcr",bf),second=session.load_bank("announcr",bf);
    check(first&&second&&!first->same_identity(*second),"same Session owns distinct actual bank instances");
    auto prepared_first=session.prepare_bank(*first,report,prep,bf),prepared_second=session.prepare_bank(*second,report,prep,bf);
    check(prepared_first&&prepared_second,"actual deferred banks prepared on same resource domain");
    const auto wait_gate=[&](const auto& gate){await([&]{std::lock_guard lock(gate->mutex);return gate->entered;},"actual selected worker reaches construction gate");};
    const auto poll=[&]{std::optional<RetailSoundPlayCompletion> result;
        await([&]{result=runtime.poll_play_completion(rf);check(rf.kind==RetailSoundRuntimeError::none,"completion poll admission");return bool(result);},"actual pending job finishes and retires");return *result;};
    auto gate=block_next_build();auto running=main_binding::submit_retail_queued_sound(main,*first,0,0xA1,parameters,f);
    check(bool(running),"Main publishes actual queued native identity");wait_gate(gate);
    check(!main.playback->observe(*running,pf)&&pf.kind==sound::PlaybackError::operation_active,
        "selected unbuilt native association remains pending");
    auto queued=main_binding::submit_retail_queued_sound(main,*second,0,0xA2,parameters,f);
    check(queued&&main.game_sound_references.size()==2,"Main owns both exact pending records");
    check(!runtime.shutdown(rf)&&rf.kind==RetailSoundRuntimeError::active_playback,"queued/running work refuses output shutdown");
    check(main_binding::retire_retail_game_sound_references(main,f)&&main.game_sound_references.empty(),
        "Main reverse removal cancels queued and requests running without early disposal");
    auto removed=poll();check(removed.handle.same_identity(*queued)&&removed.result.cancelled&&
        !removed.result.ordinary_callback_invoked&&!removed.result.runtime_ready&&removed.result.state==sound::NativePlayWorkState::retired,
        "queued target removal has no ordinary callback or runtime readiness");
    check(!main.playback->observe(*queued,pf)&&pf.kind==sound::PlaybackError::retired_handle&&
        !main.playback->observe(*running,pf)&&pf.kind==sound::PlaybackError::operation_active,
        "queued identity actually retires while running requested identity stays owned");
    check(!main.playback->unload(*first,pf)&&pf.kind==sound::PlaybackError::active_playback,
        "running cancellation acknowledgement cannot release actual bank ownership");
    std::uint64_t before=0;{std::lock_guard lock(output_state->mutex);before=output_state->submitted;output_state->nonzero=false;}
    await([&]{std::lock_guard lock(output_state->mutex);return output_state->submitted>before&&output_state->nonzero;},
        "actual PCM mixing continues while resource worker is blocked");
    gate->release();auto cancelled=poll();check(cancelled.handle.same_identity(*running)&&cancelled.result.cancelled&&
        !cancelled.result.runtime_ready&&!cancelled.result.ordinary_callback_invoked&&!cancelled.result.activated,
        "fresh running cancellation suppresses actual ordinary consumer");
    auto cache=prepared_first->runtime_cache(prep);check(cache&&!(*cache)[0].cached,"fresh cancellation publishes no cache");
    check(!main.playback->observe(*running,pf)&&pf.kind==sound::PlaybackError::retired_handle,"running identity retires after real cleanup");
    gate=block_next_build();auto primer=main_binding::submit_retail_queued_sound(main,*first,0,0xB1,parameters,f);
    check(bool(primer),"priority primer admitted");wait_gate(gate);
    auto fresh=main_binding::submit_retail_queued_sound(main,*second,0,0xB2,parameters,f);
    auto cached=main_binding::submit_retail_queued_sound(main,original,0,0xB3,parameters,f);
    check(fresh&&cached,"fresh then cached queue admitted in source comparison order");gate->release();
    auto a=poll(),b=poll(),c=poll();
    check(a.handle.same_identity(*primer)&&b.handle.same_identity(*cached)&&c.handle.same_identity(*fresh),
        "actual cached runtime wins ahead of earlier queued fresh runtime");
    for(const auto& result:{a,b,c})check(result.result.runtime_ready&&result.result.ordinary_callback_invoked&&result.result.activated&&
        !result.result.failure&&result.result.state==sound::NativePlayWorkState::retired,"actual runtime -> copy -> activation -> job retirement");
    check(main_binding::retire_retail_game_sound_references(main,f),"Main disposes actual priority scenario references");
    gate=block_next_build();auto cached_cancel=main_binding::submit_retail_queued_sound(main,original,0,0xC1,parameters,f);
    check(bool(cached_cancel),"cached cancellation candidate admitted");wait_gate(gate);
    check(main_binding::retire_retail_game_sound_references(main,f)&&main.game_sound_references.empty(),"cached running request clears game reference only");
    gate->release();auto accepted=poll();
    check(accepted.handle.same_identity(*cached_cancel)&&accepted.result.branch==sound::RuntimeProgramWorkBranch::cached&&
        accepted.result.cancellation_requested&&!accepted.result.cancelled&&accepted.result.runtime_ready&&
        accepted.result.ordinary_callback_invoked&&accepted.result.activated,
        "cached source branch bypasses requested fresh-cancel gate and activates actual copy");
    check(bool(main.playback->force_retire(*cached_cancel,pf)),"actual cached result still needs native resource disposal");
    // Hold the real construction lease after build, then dispose an actual
    // incumbent. Its tail must still mix while context cleanup is waiting.
    gate=block_next_build(true);auto gap=main_binding::submit_retail_queued_sound(main,*first,0,0xD1,parameters,f);
    check(bool(gap),"actual construction/cleanup gap job admitted");wait_gate(gate);
    {std::lock_guard lock(output_state->mutex);before=output_state->submitted;output_state->nonzero=false;}
    std::atomic<bool> disposing=false;disposal_probe=[&]{disposing=true;};
    std::thread observer([&]{await([&]{return disposing.load();},"real private-copy disposal begins");
        await([&]{std::lock_guard lock(output_state->mutex);return output_state->submitted>before&&output_state->nonzero;},
            "retained native PCM continues during context cleanup/resource exclusion");gate->release();});
    check(bool(main.playback->force_retire(*keeper,pf)),"actual incumbent cleanup waits for construction lease");
    disposal_probe={};observer.join();auto resumed=poll();
    check(resumed.handle.same_identity(*gap)&&resumed.result.activated&&!resumed.result.failure,
        "lower construction lease retires before callback and exact owned plan activates after cleanup");
    check(main_binding::retire_retail_game_sound_references(main,f),"gap job's actual game reference retires");
    // Source12C300/50 may arrive before construction. Promote the exact
    // queued association, preserve first-match game order, and defer start.
    auto third=session.load_bank("announcr",bf);check(bool(third),"third actual fresh bank for promotion ordering");
    auto prepared_third=session.prepare_bank(*third,report,prep,bf);check(bool(prepared_third),"third deferred bank prepared");
    auto held_parameters=parameters;held_parameters.caller_play_flags=9;
    gate=block_next_build();auto lead=main_binding::submit_retail_queued_sound(main,*first,0,0xF0,parameters,f);
    check(bool(lead),"actual blocked lead before pending resume");wait_gate(gate);
    auto earlier=main_binding::submit_retail_queued_sound(main,*third,0,0xA0,parameters,f);
    auto promoted=main_binding::submit_retail_queued_sound(main,*second,0,0xF1,held_parameters,f);
    auto same_identifier=main_binding::submit_retail_queued_sound(main,*second,0,0xF1,held_parameters,f);
    check(earlier&&promoted&&same_identifier&&main.game_sound_references.size()==4,"actual pending records include duplicate semantic identifier");
    check(main_binding::resume_retail_game_sound_reference(main,0xF1,f)&&main.game_sound_references.size()==3&&
        main.game_sound_references[0].playback.same_identity(*lead)&&main.game_sound_references[1].playback.same_identity(*earlier)&&
        main.game_sound_references[2].playback.same_identity(*same_identifier),"12C300 removes first match and preserves remaining actual record order");
    check(!main.playback->observe(*promoted,pf)&&pf.kind==sound::PlaybackError::operation_active,
        "pending resume request neither creates a voice nor retires public work early");
    check(main_binding::resume_retail_game_sound_reference(main,0xDEAD,f)&&main.game_sound_references.size()==3,
        "absent semantic identifier has no native control effect");
    check(runtime.request_resume(*lead,rf)&&runtime.request_resume(*lead,rf),"repeated running resume requests retain the exact current work");
    gate->release();auto lead_result=poll(),promoted_result=poll(),cached_result=poll(),earlier_result=poll();
    check(lead_result.resume_delivered&&!lead_result.resume_failure&&lead_result.handle.same_identity(*lead)&&promoted_result.handle.same_identity(*promoted)&&
        cached_result.handle.same_identity(*same_identifier)&&earlier_result.handle.same_identity(*earlier),
        "F980 promotion precedes earlier fresh work, then real cached priority wins again");
    check(promoted_result.resume_delivered&&!promoted_result.resume_failure&&promoted_result.result.activated&&
        promoted_result.result.runtime_ready&&!promoted_result.result.failure,"pending control is delivered after actual ready consumer and retirement");
    auto resumed_play=main.playback->observe(*promoted,pf),held_play=main.playback->observe(*same_identifier,pf);
    check(resumed_play&&resumed_play->dispatch_live&&!resumed_play->held&&resumed_play->voices==1&&held_play&&held_play->held&&
        !held_play->dispatch_live&&!held_play->deadline_bound&&held_play->voices==0&&!cached_result.resume_delivered,
        "only the exact promoted association starts; the equal-id suffix remains truly held");
    check(main_binding::resume_retail_game_sound_reference(main,0xF1,f)&&main.game_sound_references.size()==2,
        "ready held command50 removes the next exact matching record");
    held_play=main.playback->observe(*same_identifier,pf);check(held_play&&held_play->dispatch_live&&held_play->voices==1,
        "ready native resume starts exactly one real first-sample voice");
    check(main_binding::retire_retail_game_sound_references(main,f)&&main.playback->stop(*promoted,pf)&&
        main.playback->stop(*same_identifier,pf),"actual promoted/ready mode1 and remaining game references retire");
    auto unsupported=parameters;unsupported.caller_play_flags=9;unsupported.output.selector=-2;
    auto invalid=runtime.submit_queued_play(original,0,unsupported,rf);check(bool(invalid),"actual held readiness with unbound selector admitted as work");
    auto failure=poll();check(failure.handle.same_identity(*invalid)&&failure.result.runtime_ready&&failure.result.ordinary_callback_invoked&&
        !failure.result.activated&&failure.result.failure&&failure.result.failure->kind==sound::PlaybackError::unsupported_output_selector,
        "runtime-ready callback is not a voice or activation success acknowledgement");
    invalid=main_binding::submit_retail_queued_sound(main,original,0,0xE1,unsupported,f);check(bool(invalid),"Main owns named unsupported consumer job");
    await([&]{return !main.playback->observe(*invalid,pf)&&pf.kind==sound::PlaybackError::retired_handle;},"actual failed consumer disposes before Main status");
    await([&]{return !main_binding::pump_retail_current_sound(main,f);},"Main reports actual pending consumer failure");
    check(f.sound_catalog.runtime.playback.kind==sound::PlaybackError::unsupported_output_selector,
        "Main preserves exact unsupported consumer failure before status");
    check(main_binding::retire_retail_game_sound_references(main,f),"failed job's retired reference removed");
    for(std::int64_t cut:{0,1}){fail_after=cut;auto rejected=main_binding::submit_retail_queued_sound(main,original,0,0,parameters,f);fail_after=-1;
        check(!rejected&&main.game_sound_references.empty(),"admission allocation failure exposes no pending game reference");}
    bool refused=false;std::thread foreign([&]{RetailSoundRuntimeFailure other;
        refused=!runtime.submit_queued_play(original,0,parameters,other)&&other.kind==RetailSoundRuntimeError::wrong_thread&&
            !runtime.cancel_pending_play(*invalid,other)&&other.kind==RetailSoundRuntimeError::wrong_thread&&
            !runtime.poll_play_completion(other)&&other.kind==RetailSoundRuntimeError::wrong_thread&&
            !runtime.request_resume(*invalid,other)&&other.kind==RetailSoundRuntimeError::wrong_thread;});foreign.join();
    check(refused,"actual queue/poll/cancellation lifecycle stays on Main thread");
    await([&]{auto o=main.playback->observe_all_outputs(pf);return o&&!o->retired_groups&&!o->pending_preparations;},"pending scenarios retire real tails and jobs");
    check(main.playback->unload(*first,pf)&&main.playback->unload(*second,pf)&&main.playback->unload(*third,pf),"actual duplicate bank instances unload after true job retirement");
}

}
namespace {
void exercise_direct(NativeRetailMainState& main,sound::AudioBankSession& session,sound::NativeActivationParameters parameters){
    StartupFailure f;RetailSoundRuntimeFailure rf;sound::PlaybackFailure pf;sound::PreparationFailure prep;
    sound::AudioBanksFailure bf;sound::BankPreparationReport report;auto& runtime=*main.sound_runtime;
    parameters.caller_play_flags=9; // Actual source12C030 held/force1 request.
    auto a=session.load_bank("announcr",bf),b=session.load_bank("announcr",bf),c=session.load_bank("bg_armory",bf);
    check(a&&b&&c,"real same-Session banks for original direct/worker predicate");
    auto pa=session.prepare_bank(*a,report,prep,bf),pb=session.prepare_bank(*b,report,prep,bf),pc=session.prepare_bank(*c,report,prep,bf);
    check(pa&&pb&&pc,"actual deferred and eager bank preparation");
    auto samples=pb->prepare_deferred_normal(0,prep);check(bool(samples),"actual sample cache ready without a runtime");
    auto gate=block_next_build();auto pending=main_binding::submit_retail_sound(main,*a,0,0xA0,parameters,f);
    check(bool(pending),"uncached actual sample chooses real pending worker");
    await([&]{std::lock_guard lock(gate->mutex);return gate->entered;},"actual queued constructor is held before build");
    bool reentry_checked=false;
    {std::lock_guard lock(build_hook_mutex);before_runtime_build=[&]{
        check(std::this_thread::get_id()==output_state->main_thread,"sample-ready constructor executes directly on submitting thread");
        StartupFailure nested;RetailSoundRuntimeFailure nested_runtime;
        check(!main_binding::retire_retail_game_sound_references(main,nested)&&nested.sound_catalog.runtime.playback.kind==sound::PlaybackError::operation_active,
              "direct native construction reentry cannot erase reserved game records");
        check(!runtime.shutdown(nested_runtime)&&nested_runtime.kind==RetailSoundRuntimeError::active_playback&&
              !runtime.request_resume(*pending,nested_runtime)&&nested_runtime.playback.kind==sound::PlaybackError::operation_active,
              "direct admission rejects reentrant shutdown/resume before worker waits or publication locks");
        samples.reset();reentry_checked=true;
    };}
    auto direct=main_binding::submit_retail_sound(main,*b,0,0xB0,parameters,f);
    check(direct&&reentry_checked,"actual sample-ready Main request completes while the resource worker remains blocked");
    auto completion=runtime.poll_play_completion(rf);
    check(completion&&completion->handle.same_identity(*direct)&&completion->result.direct_attempted&&
        !completion->result.direct_retry_queued&&completion->result.branch==sound::RuntimeProgramWorkBranch::fresh&&completion->result.activated,
        "direct sample constructor delivers its real consumer and retirement before return");
    auto cached=main_binding::submit_retail_sound(main,*c,0,0xC0,parameters,f);
    completion=runtime.poll_play_completion(rf);
    check(cached&&completion&&completion->handle.same_identity(*cached)&&completion->result.direct_attempted&&
        completion->result.branch==sound::RuntimeProgramWorkBranch::cached&&completion->result.activated,
        "existing runtime bypasses unrelated queued work and acquires its actual private copy");
    auto held=main.playback->observe(*direct,pf);check(held&&held->held&&!held->deadline_bound&&!held->voices,
        "actual source flags9 direct readiness remains held without key-on");
    auto unsupported=parameters;unsupported.output.selector=-2;
    auto rejected=runtime.submit_play(*b,0,unsupported,rf);completion=runtime.poll_play_completion(rf);
    check(rejected&&completion&&completion->handle.same_identity(*rejected)&&completion->result.direct_attempted&&
        !completion->result.direct_retry_queued&&completion->result.runtime_ready&&!completion->result.activated&&
        completion->result.failure&&completion->result.failure->kind==sound::PlaybackError::unsupported_output_selector,
        "direct consumer failure is reported without a fresh constructor retry");
    gate->release();
    await([&]{completion=runtime.poll_play_completion(rf);return bool(completion);},"uncached worker completes after actual release");
    check(completion->handle.same_identity(*pending)&&!completion->result.direct_attempted&&completion->result.activated,
        "uncached construction keeps the real asynchronous branch");
    check(main_binding::retire_retail_game_sound_references(main,f),"all direct/queued held game references retire actual resources");
    samples=pb->prepare_deferred_normal(0,prep);check(bool(samples),"re-prime sample-only direct retry scenario");
    bool retried_on_worker=false;
    {std::lock_guard lock(build_hook_mutex);
     before_runtime_build=[]{fail_after=0;};
     after_runtime_build=[&]{fail_after=-1;std::lock_guard lock(build_hook_mutex);before_runtime_build=[&]{
        check(std::this_thread::get_id()!=output_state->main_thread,"failed direct constructor is retried by the actual resource worker");retried_on_worker=true;};};}
    auto retry=main_binding::submit_retail_sound(main,*b,0,0xD0,parameters,f);check(bool(retry),"fresh direct constructor failure retains the same public association for queued retry");
    await([&]{completion=runtime.poll_play_completion(rf);return bool(completion);},"real queued retry completes");
    check(completion->handle.same_identity(*retry)&&retried_on_worker&&completion->result.direct_retry_queued&&
        completion->result.direct_constructor_failure&&completion->result.runtime_ready&&completion->result.activated,
        "constructor failure is preserved separately from actual retried readiness/consumer success");
    {std::lock_guard lock(build_hook_mutex);check(!before_runtime_build&&!after_runtime_build,"one-shot actual build fault hooks are fully consumed");}
    check(main_binding::retire_retail_game_sound_references(main,f),"retried actual association retires");samples.reset();
    check(main.playback->unload(*a,pf)&&main.playback->unload(*b,pf)&&main.playback->unload(*c,pf),"all direct/worker bank owners truly unload");
}
}
// Wrap only the physical output leaf. The actual production Main/context/Session,
// QPC worker, program copy, voice and mixer still execute. No endpoint is opened.
std::unique_ptr<sound::NativeSoundOutput> wrapped_game_output(sound::AudioDeviceFailure& f)
    asm("__wrap__ZN3mka5sound29open_native_game_sound_outputERNS0_18AudioDeviceFailureE");
std::unique_ptr<sound::NativeSoundOutput> wrapped_game_output(sound::AudioDeviceFailure& f){
    std::lock_guard lock(output_state->mutex);output_state->worker_thread=std::this_thread::get_id();++output_state->opened;f={};
    if(output_state->scenario==Scenario::open_failure){f={sound::AudioDeviceError::endpoint_unavailable,-41,"fixture unavailable endpoint"};return {};}
    if(output_state->scenario==Scenario::allocation_failure)throw std::bad_alloc();
    return std::make_unique<TestOutput>(output_state);
}
extern "C" [[noreturn]] void __wrap__ZN3mka5sound17NativeAudioDevice4openERKNS0_17AudioDeviceConfigERNS0_18AudioDeviceFailureE(){
    std::fputs("FAIL physical NativeAudioDevice open reached endpoint-free context test\n",stderr);std::abort();
}
extern "C" void* real_new(std::size_t) __asm__("__real__Znwy");
extern "C" void* wrapped_new(std::size_t) __asm__("__wrap__Znwy");
extern "C" void* wrapped_new(std::size_t size){
    if(fail_after==0)throw std::bad_alloc();
    if(fail_after>0)--fail_after;
    return real_new(size);
}
extern "C" void real_copy_dispose(sound::NativeProgramCopy*) __asm__("__real__ZN3mka5sound17NativeProgramCopyD1Ev");
extern "C" void wrapped_copy_dispose(sound::NativeProgramCopy*) __asm__("__wrap__ZN3mka5sound17NativeProgramCopyD1Ev");
extern "C" void wrapped_copy_dispose(sound::NativeProgramCopy* copy){
    if(disposal_probe)disposal_probe();
    real_copy_dispose(copy);
}
extern "C" bool real_runtime_build(sound::RuntimeProgramWork*,sound::PreparationFailure&) __asm__("__real__ZN3mka5sound18RuntimeProgramWork5buildERNS0_18PreparationFailureE");
extern "C" bool wrapped_runtime_build(sound::RuntimeProgramWork*,sound::PreparationFailure&) __asm__("__wrap__ZN3mka5sound18RuntimeProgramWork5buildERNS0_18PreparationFailureE");
extern "C" bool wrapped_runtime_build(sound::RuntimeProgramWork* work,sound::PreparationFailure& f){
    std::function<void()> before,after;{std::lock_guard lock(build_hook_mutex);before=std::exchange(before_runtime_build,{});after=std::exchange(after_runtime_build,{});}
    if(before)before();const bool result=real_runtime_build(work,f);if(after)after();return result;
}
int main(int argc,char** argv) try {
    check(argc==2,"expected original native retail content root");
    const auto neutral=retail_sound_status_controls({16384,0,4096,16384,0,4096});
    check(neutral==std::array<float,6>{1,0,1,1,0,1},"original314F30 six neutral controls");
    const auto edges=retail_sound_status_controls({16384,-16383,4096,-1,16383,-1});
    check(edges==std::array<float,6>{1,-2,1,262144,2,1048576},
        "source signed pan endpoints and high-bit unsigned gain/pitch conversion");
    for(auto scenario:{Scenario::open_failure,Scenario::allocation_failure,Scenario::bad_format}){
        setup(scenario);Host host(argv[1]);StartupFailure f;
        check(!main_binding::initialize_retail_current_sound(*host.main,f)&&
            host.main->current_sound_phase==RetailCurrentSoundPhase::failed&&!host.main->playback&&
            !host.main->current_sound&&!host.main->sound_runtime,"failed real construction publishes no context owners");
        if(scenario==Scenario::open_failure)check(f.sound_catalog.runtime.output.native_code==-41,"actual open error detail retained");
        if(scenario==Scenario::allocation_failure)check(f.kind==StartupError::allocation,"worker allocation error retains its category");
        if(scenario==Scenario::bad_format)check(f.sound_catalog.runtime.kind==RetailSoundRuntimeError::invalid_output_format,"bad actual client format refused");
        host.main.reset();
    }
    for(auto scenario:{Scenario::poll_failure,Scenario::start_failure}){
        setup(scenario);Host host(argv[1]);StartupFailure f;
        check(main_binding::initialize_retail_current_sound(*host.main,f),"real context initializes before asynchronous host failure");
        RetailSoundRuntimeFailure runtime_failure;
        await([&]{return !host.main->sound_runtime->observe(runtime_failure);},"worker reports actual host failure");
        const auto code=scenario==Scenario::poll_failure?-73:-81;
        check(runtime_failure.output.native_code==code,"asynchronous host detail retained");
        check(!main_binding::pump_retail_current_sound(*host.main,f)&&f.sound_catalog.runtime.output.native_code==code&&
            host.main->sound_frame_counter==1&&!host.main->sound_status,"real Main propagates worker failure before status publication");
        host.main.reset();
        check(output_state->closed==1&&output_state->destroyed==1,"failed worker releases output on owning thread");
    }
    setup(Scenario::normal);
    {
        Host host(argv[1]);auto& main=*host.main;StartupFailure f;
        check(main_binding::initialize_retail_current_sound(main,f),"real context for direct output-revocation gate");
        RetailSoundCatalogFailure cf;auto session=main.sound_bank_catalog->share_bank_session(cf);
        sound::AudioBanksFailure bf;sound::PreparationFailure prep;sound::BankPreparationReport report;
        auto bank=session->load_bank("announcr",bf);auto prepared=bank?session->prepare_bank(*bank,report,prep,bf):std::nullopt;
        check(bank&&prepared,"actual native bank for direct publication revocation");
        auto samples=prepared->prepare_deferred_normal(0,prep);check(bool(samples),"actual cached sample enters direct publication test");
        sound::NativeActivationParameters parameters;parameters.caller_play_flags=9;
        auto gate=block_next_build();
        std::thread revoke([&]{
            await([&]{std::lock_guard lock(gate->mutex);return gate->entered;},"actual direct constructor reaches publication gap");
            {std::lock_guard lock(output_state->mutex);output_state->scenario=Scenario::poll_failure;}
            await([&]{std::lock_guard lock(output_state->mutex);return output_state->closed==1;},"actual native output failure records before direct publication");
            gate->release();
        });
        auto play=main_binding::submit_retail_sound(main,*bank,0,0xFF,parameters,f);revoke.join();
        RetailSoundRuntimeFailure rf;auto completion=main.sound_runtime->poll_play_completion(rf);
        check(play&&completion&&completion->handle.same_identity(*play)&&completion->result.direct_attempted&&
            completion->result.runtime_ready&&!completion->result.activated&&completion->result.failure&&
            completion->result.failure->kind==sound::PlaybackError::closed,
            "output failure during real direct construction revokes publication and reports same-identity closed completion");
        sound::PlaybackFailure pf;check(!main.playback->observe(*play,pf)&&pf.kind==sound::PlaybackError::retired_handle&&
            main_binding::retire_retail_game_sound_references(main,f),"revoked direct output exposes no play and actual game reference retires");
        samples.reset();check(main.playback->unload(*bank,pf),"revoked direct bank has no leaked work/resource owners");
        check(main.sound_runtime->shutdown(rf),"failed output with all real resources retired closes cleanly");
    }
    setup(Scenario::normal);
    {
        Host host(argv[1]);auto& main=*host.main;StartupFailure f;
        check(main_binding::initialize_retail_current_sound(main,f)&&main.sound_runtime&&main.playback&&main.current_sound,
            "actual316320 publishes native output/worker/playback/context owners");
        check(!main_binding::initialize_retail_current_sound(main,f)&&f.kind==StartupError::already_initialized,
            "repeated initialization cannot replace the live native context");
        RetailSoundRuntimeFailure runtime_failure;
        auto observed=main.sound_runtime->observe(runtime_failure);
        check(observed&&observed->format==sound::AudioStreamFormat{44100,2,3},"actual opened format reaches runtime");
        RetailSoundCatalogFailure catalog_failure;auto session=main.sound_bank_catalog->share_bank_session(catalog_failure);
        check(session&&session->resource_domain()==main.playback->resource_domain(),"production worker shares actual catalog Session domain");
        sound::PlaybackFailure pf;
        for(unsigned i=0;i<2000;++i)check(bool(main.playback->observe_sound_context(*main.current_sound,pf)),
            "native worker clock sampling does not spuriously reject Main observers");
        bool foreign_refused=false;
        std::thread foreign([&]{RetailSoundRuntimeFailure other;foreign_refused=!main.sound_runtime->observe(other)&&
            other.kind==RetailSoundRuntimeError::wrong_thread&&!main.sound_runtime->snapshot_status(other)&&
            other.kind==RetailSoundRuntimeError::wrong_thread&&!main.sound_runtime->shutdown(other)&&other.kind==RetailSoundRuntimeError::wrong_thread;});
        foreign.join();check(foreign_refused,"runtime lifecycle stays on Main's thread");
        check(initial_reset_required::request_mode_one_banks_12a4f0(main,f),"real source four-bank requests");
        for(unsigned i=0;i<4;++i)check(main_binding::pump_retail_current_sound(main,f),"real Main callback queue before status request");
        check(main.sound_status&&main.sound_status->controls==neutral&&main.sound_frame_counter==1&&
            main.sound_status->native.format==sound::AudioStreamFormat{44100,2,3},
            "actual typed status delivery resets attempts before each outer frame increment");
        auto announcr=main.sound_bank_catalog->observe(8,catalog_failure);check(announcr&&announcr->bank,"actual announcr callback handle");
        // Explicit software envelope/caller inputs. This verifies the production
        // ownership/worker connection, not the still-unbound retail envelope.
        sound::NativeActivationParameters parameters{NativeRetailSoundRuntime::initial_context_controls(),
            sound::initial_program_control_factors(),{{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},
            NativeRetailSoundRuntime::initial_output_mode(),0,{-1,0}};
        auto other_context=main.playback->create_sound_context({1},pf);
        check(bool(other_context),"independent native context for global ownership guard");
        sound::NativePlayParameters other_parameters{parameters.context,parameters.program,parameters.envelope,
            {0},parameters.source_output_mode,parameters.caller_play_flags,parameters.output};
        auto other_play=main.playback->play(*other_context,*announcr->bank,0,other_parameters,pf);
        std::vector<sound::NativePlayHandle> future;
        check(bool(other_play),"native future play for actual status association");future.push_back(*other_play);
        for(unsigned i=1;i<70;++i){auto next=main.playback->play(*other_context,*announcr->bank,0,other_parameters,pf);
            check(bool(next),"actual native association storage grows");future.push_back(*next);}
        auto* previous=main.sound_status.get();
        for(std::int64_t cut:{0,1}){
            main.sound_frame_counter=17;fail_after=cut;
            const bool delivered=current_sound_required::update_programs_after_bank_results_3152c0(main,f);fail_after=-1;
            check(!delivered&&main.sound_status.get()==previous&&main.sound_frame_counter==17,
                "snapshot/publication allocation failure retains prior status and undelivered attempt count");
            if(cut==0)check(f.sound_catalog.runtime.playback.kind==sound::PlaybackError::allocation_failed,
                "actual registry snapshot allocation failure is named");
            else check(f.kind==StartupError::allocation,"actual publication allocation failure is named");
        }
        check(current_sound_required::update_programs_after_bank_results_3152c0(main,f)&&
            main.sound_frame_counter==0&&main.sound_status->active_plays.size()==70,
            "actual typed status has all70 owner-wide associations without the source transport cap");
        for(unsigned i=0;i<70;++i)check(main.sound_status->active_plays[i].same_identity(future[i]),
            "status carries actual native play identities in registry order");
        previous=main.sound_status.get();auto current=*main.current_sound;main.current_sound=*other_context;main.sound_frame_counter=19;
        check(!current_sound_required::update_programs_after_bank_results_3152c0(main,f)&&main.sound_status.get()==previous&&
            main.sound_frame_counter==19,"foreign request target does not publish/reset a fabricated reply");main.current_sound=current;
        check(other_context&&other_play&&!main.sound_runtime->shutdown(runtime_failure)&&
            runtime_failure.kind==RetailSoundRuntimeError::active_playback,
            "output shutdown checks actual ownership in every context, not only current");
        for(const auto& handle:future)check(main.playback->stop(handle,pf)&&main.playback->release(handle,pf),
            "other context explicitly retires each native program");
        check(main.playback->release_sound_context(*other_context,pf),"idle other context retires");
        check(!main.playback->snapshot_active_plays(*other_context,pf)&&pf.kind==sound::PlaybackError::retired_context,
            "snapshot rejects an actually retired target context");
        check(main_binding::pump_retail_current_sound(main,f)&&main.sound_status->active_plays.empty()&&
            main.sound_frame_counter==1,"actual status removes stopped/retired associations after typed delivery");
        exercise_pending(main,*session,*announcr->bank,parameters);
        exercise_direct(main,*session,parameters);
        auto public_parameters=parameters;public_parameters.envelope.release={4,-64,false};
        auto play=main.playback->activate(*main.current_sound,*announcr->bank,0,public_parameters,pf);
        check(bool(play),"actual callback handle enters the real native worker playback");
        {std::lock_guard lock(output_state->mutex);output_state->reject_next=true;}
        await([&]{std::lock_guard lock(output_state->mutex);return output_state->nonzero&&output_state->replay_verified;},
            "actual worker drives announcr/copy/voice/mixer with exact pending-buffer retry");
        check(!main.sound_runtime->shutdown(runtime_failure)&&runtime_failure.kind==RetailSoundRuntimeError::active_playback,
            "idle shutdown refuses live native program/reference ownership");
        {std::lock_guard lock(output_state->mutex);output_state->force_full=true;}
        await([&]{auto o=main.sound_runtime->observe(runtime_failure);return o&&o->observed_padding_frames==128&&
            o->pending_pcm_frames==0;},"actual full output queue excludes further mixing for the lifetime check");
        main.game_sound_references.push_back({*play,*announcr->bank,0x1234});
        main.game_sound_references.push_back({{},{},0x5678});
        bool retirement_foreign_refused=false;
        std::thread foreign_retirement([&]{StartupFailure other;retirement_foreign_refused=
            !main_binding::retire_retail_game_sound_references(main,other)&&other.sound_catalog.kind==RetailSoundCatalogError::wrong_owner;});
        foreign_retirement.join();check(retirement_foreign_refused&&main.game_sound_references.size()==2,
            "foreign-thread retirement leaves actual game references untouched");
        fail_after=0;const bool retired_failure=main_binding::retire_retail_game_sound_references(main,f);fail_after=-1;
        check(!retired_failure&&f.kind==StartupError::allocation&&main.game_sound_references.size()==1&&
            main.game_sound_references.back().sound_identifier==0x1234&&main.playback->observe(*play,pf),
            "reverse removal retains the failing live reference after removing the absent suffix");
        unsigned disposal_probes=0;
        disposal_probe=[&]{
            ++disposal_probes;sound::PlaybackFailure during;StartupFailure reentrant;
            check(!main.playback->observe(*play,during)&&during.kind==sound::PlaybackError::operation_active,
                "actual native handle remains busy before private-copy disposal completes");
            check(!main_binding::retire_retail_game_sound_references(main,reentrant)&&
                reentrant.sound_catalog.runtime.playback.kind==sound::PlaybackError::operation_active&&
                main.game_sound_references.size()==1,"reentrant actual cleanup cannot erase an unfinished reference");
        };
        check(main_binding::retire_retail_game_sound_references(main,f),"actual Main bulk reference retirement");
        disposal_probe={};
        auto tails=main.playback->observe_all_outputs(pf);
        check(disposal_probes>0&&main.game_sound_references.empty()&&tails&&tails->programs==0&&
            tails->retired_groups==1&&tails->active_retired_voices>0,
            "real destructor/semantic disposal precedes record removal while active PCM tails stay owned");
        check(!main.playback->observe(*play,pf)&&pf.kind==sound::PlaybackError::retired_handle,
            "the actual native identity retires only after resource cleanup");
        check(main_binding::pump_retail_current_sound(main,f)&&main.sound_status->active_plays.empty(),
            "real status observes completed dispatch cleanup without fabricating a transport callback");
        {std::lock_guard lock(output_state->mutex);output_state->nonzero=false;output_state->force_full=false;}
        await([&]{auto o=main.playback->observe_all_outputs(pf);return o&&o->retired_groups==0;},
            "actual worker drains and retires real reference-cleanup tails");
        {std::lock_guard lock(output_state->mutex);check(output_state->nonzero,"new nonzero PCM is submitted from the retained tail alone");}
        check(main_binding::retire_retail_game_sound_references(main,f),"empty Main reference retirement is idempotent");
        auto final_play=main.playback->activate(*main.current_sound,*announcr->bank,0,parameters,pf);
        check(bool(final_play),"second actual native play for pending-output lifetime");
        std::uint64_t before_gap=0;
        {std::lock_guard lock(output_state->mutex);
         before_gap=output_state->submitted;output_state->reject_next=true;output_state->replay_verified=false;
         output_state->on_reject=[owner=main.playback,handle=*final_play]{
             sound::PlaybackFailure f;
             check(owner->stop(handle,f)&&owner->mix(32,f)&&owner->release(handle,f),
                 "explicit software release retires copy ownership while mixed PCM is still pending");
         };}
        await([&]{std::lock_guard lock(output_state->mutex);return output_state->retired_during_gap;},
            "actual ownership retires inside the injected output gap");
        await([&]{auto o=main.sound_runtime->observe(runtime_failure);return o&&o->pending_pcm_frames==128;},
            "real worker retains the actual mixed block after program ownership is gone");
        {std::lock_guard lock(output_state->mutex);output_state->padding_observations=6;}
        check(main.sound_runtime->shutdown(runtime_failure)&&main.sound_runtime->shutdown(runtime_failure),"idle worker joins and shutdown is idempotent");
        main.sound_frame_counter=23;previous=main.sound_status.get();
        check(!current_sound_required::update_programs_after_bank_results_3152c0(main,f)&&
            f.sound_catalog.runtime.kind==RetailSoundRuntimeError::closed&&main.sound_status.get()==previous&&
            main.sound_frame_counter==23,"closed native output cannot publish a successful status");
        check(output_state->starts==1&&output_state->closed==1&&output_state->destroyed==1&&output_state->retry_count==2&&
            output_state->replay_verified&&output_state->submitted==before_gap+128&&output_state->last_padding==0,
            "shutdown submits retained PCM exactly once and waits for actual client padding before close");
    }
    setup(Scenario::close_failure);
    {
        Host host(argv[1]);StartupFailure f;check(main_binding::initialize_retail_current_sound(*host.main,f),"close-error context");
        RetailSoundRuntimeFailure failure;
        check(!host.main->sound_runtime->shutdown(failure)&&failure.output.native_code==-91,
            "actual output-close failure is returned after worker-thread release");
        check(host.main->sound_runtime->shutdown(failure),"already released failed output is not closed again");
    }
    std::puts("PASS production Main direct/sample/cache/worker-retry/output-revocation/consumer-failure/held/resume/pending/cancellation/reference/context/status/workers/PCM tails/70 identities/allocation/disposal gap/error/lifetime; physical output and fault probes wrapped; no game/endpoint");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL production sound context: %s\n",e.what());return 1;}
