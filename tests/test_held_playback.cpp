#include "mka/playback_owner.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL held native playback: %s\n",why);std::abort();}}
thread_local std::int64_t fail_after=-1;
thread_local std::function<void()> disposal_probe;
struct Fixture {
    std::int64_t ticks=10000;std::size_t reads=0;bool fail_clock=false;
    std::function<void()> on_read;
    std::optional<AudioBankSession> session;
    AudioBankHandle bank;NativeSoundContext context;
    std::optional<NativePlaybackOwner> owner;
    explicit Fixture(const char* path){
        AudioBanksFailure bf;PreparationFailure pf;BankPreparationReport report;
        session=AudioBankSession::open(path,std::nullopt,bf);check(bool(session),"actual archive Session");
        auto loaded=session->load_bank("announcr",bf);check(bool(loaded),"actual announcr native bank");bank=*loaded;
        check(bool(session->prepare_bank(bank,report,pf,bf)),"actual bank preparation");
        ProgramHostClockFailure cf;
        auto clock=NativeProgramHostClock::create_with_reader(1000,[this](ProgramHostClockFailure& f)->std::optional<std::int64_t>{
            ++reads;if(on_read)on_read();if(fail_clock){f.kind=ProgramHostClockError::query;return {};}return ticks;
        },cf);check(bool(clock),"actual retained host clock with explicit reader");
        auto view=session->share_native_services(bf);check(bool(view),"same Session native view");
        PlaybackFailure f;owner=NativePlaybackOwner::create_with_clock(std::move(*view),48000,AudioControlCurve::startup_linear,clock,f);
        check(bool(owner),"native Session Owner, no compatibility ingress");
        auto c=owner->create_sound_context({2},f);check(bool(c),"actual role context");context=*c;
    }
    NativeActivationParameters parameters(std::uint16_t flags=9,std::int32_t role=-1,std::int16_t priority=0){
        return {initial_program_control_factors(),initial_program_control_factors(),
            {{1,14336,false},{1,-16384,true},{1,0,false},{1,-64,false},2048},3,flags,{role,priority}};
    }
    NativePlayHandle activate(std::uint16_t flags=9,std::int32_t role=-1,std::int16_t priority=0){
        PlaybackFailure f;auto h=owner->activate(context,bank,0,parameters(flags,role,priority),f);check(bool(h),"actual source-flags activation");return *h;
    }
    PlaybackObservation observe(const NativePlayHandle& h){PlaybackFailure f;auto o=owner->observe(h,f);check(bool(o),"actual owned play lookup");return *o;}
    void finish(){
        on_read={};PlaybackFailure f;check(bool(owner->force_retire_context_outputs(context,f)),"explicit actual resource disposal");
        check(bool(owner->mix(8192,f)),"real software tails drain");auto outputs=owner->observe_all_outputs(f);
        check(outputs&&outputs->programs==0&&outputs->pending_preparations==0&&outputs->retired_groups==0,"all actual ownership retired");
        check(owner->unload(bank,f)&&owner->release_sound_context(context,f)&&owner->close_when_idle(f),"native bank/context/Session closure");
    }
};
void arithmetic(){
    CommandRecord c;const auto set=[&](std::uint32_t delay,bool immediate=false){
        c.bytes={};c.bytes[8]=immediate?0x81:1;for(unsigned i=0;i<4;++i)c.bytes[24+i]=static_cast<std::uint8_t>(delay>>(i*8));};
    set(0);check(prebound_resume_next_deadline({0xffffffffu},c).milliseconds==0xffffffffu,"zero delay at full word does not wrap");
    set(1);check(prebound_resume_next_deadline({0xfffffffdu},c).milliseconds==0xfffffffeu,"strict unsigned below complement adds");
    check(prebound_resume_next_deadline({0xfffffffeu},c).milliseconds==0xfffffffeu,"equal complement leaves original deadline");
    set(0xffffffffu);check(prebound_resume_next_deadline({0},c).milliseconds==0,"source negative-one delay fails strict unsigned gate");
    set(0x80000000u);check(prebound_resume_next_deadline({0},c).milliseconds==0x80000000u,"source high-bit delay is unsigned in resume");
    set(500,true);check(prebound_resume_next_deadline({10},c).milliseconds==10,"signed kind highbit suppresses next-delay addition");
}
void held_and_pcm(const char* path){
    Fixture held(path),ordinary(path);PlaybackFailure f;auto before=held.reads;auto h=held.activate();auto state=held.observe(h);
    check(held.reads==before&&state.held&&!state.deadline_bound&&!state.dispatch_live&&state.private_copy_bound&&state.voices==0,
        "flags9 prebinds without clock, ordinary membership, voice or guessed deadline");
    auto active=held.owner->snapshot_active_plays(held.context,f);check(active&&active->empty(),"held association is not source active status");
    auto silence=held.owner->mix(64,f);check(silence&&std::all_of(silence->begin(),silence->end(),[](float x){return x==0;}),"no sample start before resume");
    check(!held.owner->unload(held.bank,f)&&f.kind==PlaybackError::active_playback,"held actual semantic reference blocks unload");
    check(!held.owner->tick(h,{0xffffffffu},f)&&f.kind==PlaybackError::held_playback_requires_resume,"time alone cannot activate held sample");
    held.ticks=10500;bool reentrant=false;held.on_read=[&]{PlaybackFailure during;reentrant=!held.owner->observe(h,during)&&during.kind==PlaybackError::operation_active;};
    check(held.owner->resume(h,f),"actual native command50 resume");held.on_read={};state=held.observe(h);
    check(reentrant&&held.reads==before+1&&!state.held&&state.deadline_bound&&state.dispatch_live&&state.command_index==1&&
        state.deadline.milliseconds==2071&&state.voices==1&&state.active_voices==1&&!(state.effective_play_flags&8u),
        "resume starts first sample, clears8 and adds real next1571 delay after sampled500ms");
    before=held.reads;check(held.owner->resume(h,f)&&held.reads==before&&held.observe(h).voices==1,"already-live resume neither resamples nor duplicates voice");
    auto normal=ordinary.activate(0);auto normal_start=ordinary.owner->tick(normal,{1},f);check(normal_start&&normal_start->kind==ProgramStepKind::sample_start,"ordinary actual sample starts at native nonzero clock word");
    auto a=held.owner->mix(512,f),b=ordinary.owner->mix(512,f);
    check(a&&b&&*a==*b&&std::any_of(a->begin(),a->end(),[](float x){return x!=0;}),"same actual source PCM/control path after held resume");
    auto stop=held.owner->tick(h,{2071},f);check(stop&&stop->kind==ProgramStepKind::program_stop&&stop->stop_mode_bit==1,
        "real kind7 carries source force1 mode");
    check(!held.owner->observe(h,f)&&f.kind==PlaybackError::retired_handle,"kind7 force1 completes actual private/semantic disposal");
    auto tails=held.owner->observe_all_outputs(f);check(tails&&tails->active_retired_voices>0,"force1 keeps actual releasing PCM in mixer");
    held.finish();ordinary.finish();
}
void priority_and_cleanup(const char* path){
    Fixture x(path);PlaybackFailure f;auto incumbent=x.activate(1,0,5);auto old_start=x.owner->tick(incumbent,{1},f);check(old_start&&old_start->kind==ProgramStepKind::sample_start&&bool(x.owner->mix(64,f)),"actual force1 incumbent plays");
    auto reads=x.reads;auto low=x.activate(9,0,4),tie=x.activate(0x89,0,5);
    check(x.reads==reads&&!x.observe(low).output_owned&&!x.observe(tie).output_owned&&x.observe(incumbent).dispatch_live,
        "explicit held admission defers incumbent arbitration and clock");
    check(!x.owner->resume(low,f)&&f.kind==PlaybackError::output_priority_refused&&x.observe(low).held,"lower-priority refusal preserves exact held copy");
    check(!x.owner->resume(tie,f)&&f.kind==PlaybackError::output_priority_refused&&x.observe(tie).held,"flag80 equal-priority refusal occurs at resume");
    auto sibling=x.activate(0);check(x.owner->stop(sibling,f),"actual idle sibling retains private resources");
    auto high=x.activate(9,0,6);std::vector<NativeSoundContext> extra;unsigned probes=0;
    disposal_probe=[&]{++probes;PlaybackFailure during;
        check(!x.owner->observe(incumbent,during)&&during.kind==PlaybackError::operation_active,"force1 incumbent stays busy during actual disposal");
        check(!x.owner->resume(high,during)&&during.kind==PlaybackError::operation_active,"reentrant resume cannot publish an unfinished copy");
        check(!x.owner->release(sibling,during)&&during.kind==PlaybackError::operation_active,"owned context cleanup also excludes idle sibling disposal");
        check(bool(x.owner->mix(32,during)),"actual retired tails mix during context exclusion");
        for(unsigned i=0;i<96;++i){auto c=x.owner->create_sound_context({0},during);check(bool(c),"other native context storage grows during real disposal");extra.push_back(*c);}
    };
    check(x.owner->resume(high,f),"higher-priority resume force-retires actual incumbent");disposal_probe={};
    check(probes>0&&!x.owner->observe(incumbent,f)&&f.kind==PlaybackError::retired_handle&&x.observe(high).output_owned,
        "actual force1 cleanup precedes new role publication across vector relocation");
    for(const auto& c:extra)check(x.owner->release_sound_context(c,f),"independent context cleanup");
    x.finish();
}
void retirement_claims(const char* path){
    Fixture x(path);PlaybackFailure f;auto held=x.activate();unsigned probes=0;
    disposal_probe=[&]{++probes;PlaybackFailure during;
        check(!x.owner->observe(held,during)&&during.kind==PlaybackError::operation_active,"zero-voice held identity stays busy during actual disposal");
        check(!x.owner->unload(x.bank,during)&&during.kind==PlaybackError::operation_active,"bank unload cannot pass private-resource disposal");
        check(bool(x.owner->mix(0,during)),"zero-voice native mix stays independent of semantic disposal");
        auto live=x.owner->observe_all_outputs(during);check(live&&live->retired_groups==1,"unfinished actual retirement receipt is not reclaimed early");
    };
    check(bool(x.owner->force_retire(held,f)),"zero-voice actual held resource cleanup");disposal_probe={};
    check(probes>0&&!x.owner->observe(held,f)&&f.kind==PlaybackError::retired_handle,"held identity retires after private disposal completes");x.finish();
    Fixture y(path);auto ordinary=y.activate(1);auto start=y.owner->tick(ordinary,{1},f);
    check(start&&start->kind==ProgramStepKind::sample_start&&bool(y.owner->mix(64,f)),"actual force1 program before injected retirement failure");
    fail_after=0;auto stopped=y.owner->tick(ordinary,{1572},f);fail_after=-1;
    check(!stopped&&f.kind==PlaybackError::allocation_failed,"automatic mode1 cleanup allocation failure is named");
    auto retained=y.observe(ordinary);auto scope=y.owner->observe_sound_context(y.context,f);
    check(retained.private_copy_bound&&retained.executor_finished&&scope&&scope->pending_preparations==0,
        "failed automatic retirement retains real resources and releases its native claim");
    check(y.owner->stop(ordinary,f)&&!y.owner->observe(ordinary,f)&&f.kind==PlaybackError::retired_handle,"same actual identity explicitly retries resource cleanup");y.finish();
}
void revocation(const char* path){
    Fixture x(path);PlaybackFailure f;auto h=x.activate();x.ticks=10500;
    x.on_read=[&]{AudioBanksFailure bf;check(x.session->close(bf),"actual shared Session revokes during resume clock callback");};
    check(!x.owner->resume(h,f)&&f.kind==PlaybackError::native_bank_failed,"known Session revocation prevents resumed voice publication");x.on_read={};
    auto held=x.observe(h);check(held.held&&held.private_copy_bound&&held.voices==0&&!held.deadline_bound,"revoked resume retains owned copy without key-on");
    check(bool(x.owner->force_retire_context_outputs(x.context,f))&&bool(x.owner->mix(0,f))&&x.owner->release_sound_context(x.context,f),
        "revoked actual private reference retires before context release");
    check(!x.owner->close_when_idle(f)&&f.kind==PlaybackError::native_bank_failed&&f.native_bank.kind==AudioBanksError::closed_session,
        "already revoked Session close reports the actual native state");
}
void failures(const char* path){
    Fixture x(path);PlaybackFailure f;auto h=x.activate();x.fail_clock=true;
    check(!x.owner->resume(h,f)&&f.kind==PlaybackError::clock_failed&&x.observe(h).held&&x.observe(h).voices==0,"clock failure preserves actual held ownership");
    x.fail_clock=false;check(x.owner->resume(h,f),"same actual copy retries after clock failure");x.finish();
    unsigned failed=0;bool reached=false;
    for(std::int64_t cut=0;cut<24;++cut){
        Fixture y(path);auto old=y.activate(1,0,0);auto start=y.owner->tick(old,{1},f);check(start&&start->kind==ProgramStepKind::sample_start&&bool(y.owner->mix(64,f)),"actual allocator-gap incumbent");
        auto candidate=y.activate(9,0,1);fail_after=cut;const bool resumed=y.owner->resume(candidate,f);fail_after=-1;
        if(!resumed){++failed;check(f.kind!=PlaybackError::none,"failed actual resume has a named error");
            auto o=y.owner->observe(candidate,f);check(o&&o->private_copy_bound,"failed resume retains its actual private copy");
            auto c=y.owner->observe_sound_context(y.context,f);check(c&&c->pending_preparations==0,"failed resume releases context/preparing exclusion");
        }else reached=true;
        y.finish();if(resumed)break;
    }
    check(failed>=8&&reached,"allocation cuts cover preflight/actual cleanup/start and reach real success");
}
}
extern "C" void* real_new(std::size_t) __asm__("__real__Znwy");
extern "C" void* wrapped_new(std::size_t) __asm__("__wrap__Znwy");
extern "C" void* wrapped_new(std::size_t size){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;return real_new(size);}
extern "C" void real_dispose(NativeProgramCopy*) __asm__("__real__ZN3mka5sound17NativeProgramCopyD1Ev");
extern "C" void wrapped_dispose(NativeProgramCopy*) __asm__("__wrap__ZN3mka5sound17NativeProgramCopyD1Ev");
extern "C" void wrapped_dispose(NativeProgramCopy* copy){if(disposal_probe)disposal_probe();real_dispose(copy);}
int main(int argc,char** argv){if(argc!=2)return 2;arithmetic();held_and_pcm(argv[1]);priority_and_cleanup(argv[1]);retirement_claims(argv[1]);revocation(argv[1]);failures(argv[1]);
    std::puts("PASS native held flags9/prebind/no key-on/shared-clock resume/strict next-delay/priority/force1/PCM/lifetime/allocation; no endpoint/game");return 0;}
