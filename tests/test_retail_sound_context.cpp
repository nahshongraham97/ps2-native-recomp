#include "mka/retail_main.hpp"
#include "mka/retail_current_sound_required.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_sound_runtime.hpp"
#include "mka/retail_sound_status.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
namespace sound=mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
enum class Scenario {normal,open_failure,allocation_failure,bad_format,poll_failure,start_failure,close_failure};
struct OutputState {
    std::mutex mutex;
    const std::thread::id main_thread{std::this_thread::get_id()};
    std::thread::id worker_thread;
    Scenario scenario{Scenario::normal};
    unsigned opened{},closed{},destroyed{},starts{},retry_count{};
    std::uint64_t submitted{};
    bool started{},nonzero{},reject_next{},replay_verified{},retired_during_gap{};
    unsigned padding_observations{};
    std::uint32_t last_padding{};
    std::function<void()> on_reject;
    std::vector<float> rejected;
};
std::shared_ptr<OutputState> output_state;
thread_local std::int64_t fail_after=-1;
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
        const std::uint32_t padding=state_->padding_observations?64:0;
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
        auto play=main.playback->activate(*main.current_sound,*announcr->bank,0,parameters,pf);
        check(bool(play),"actual callback handle enters the real native worker playback");
        {std::lock_guard lock(output_state->mutex);output_state->reject_next=true;}
        await([&]{std::lock_guard lock(output_state->mutex);return output_state->nonzero&&output_state->replay_verified;},
            "actual worker drives announcr/copy/voice/mixer with exact pending-buffer retry");
        check(!main.sound_runtime->shutdown(runtime_failure)&&runtime_failure.kind==RetailSoundRuntimeError::active_playback,
            "idle shutdown refuses live native program/reference ownership");
        check(main.playback->stop(*play,pf),"explicit native program stop");
        await([&]{auto o=main.playback->observe(*play,pf);return o&&o->active_voices==0;},"actual worker drains the explicit software release");
        check(main.playback->release(*play,pf),"native program reference retired before shutdown");
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
    std::puts("PASS production native sound context/status/Session/worker/PCM/70 native identities/allocation/error/idle lifetime; only output fixture; no game/endpoint");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL production sound context: %s\n",e.what());return 1;}
