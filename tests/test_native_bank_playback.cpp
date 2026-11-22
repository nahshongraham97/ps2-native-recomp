#include "mka/playback_owner.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
using namespace mka::sound;
// The Windows test link wraps these actual compatibility entry symbols.
// Any accidental construction/decoding on the native route fails immediately.
extern "C" [[noreturn]] void __wrap__ZN3mka5sound17AudioBankProtocol6createEONS0_16AudioBankSessionERNS0_20AudioProtocolFailureE(){
    std::fputs("FAIL native path constructed AudioBankProtocol\n",stderr);std::exit(3);
}
extern "C" [[noreturn]] void __wrap__ZN3mka5sound16AudioBankService6createEONS0_17AudioBankProtocolERNS0_18BankServiceFailureE(){
    std::fputs("FAIL native path constructed compatibility AudioBankService\n",stderr);std::exit(3);
}
extern "C" [[noreturn]] void __wrap__ZN3mka5sound23decode_audio_bank_frameEPKhyRNS0_20AudioProtocolFailureE(){
    std::fputs("FAIL native path decoded a compatibility packet\n",stderr);std::exit(3);
}
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
// Explicit existing software test profile, not recovered retail initialization.
NativePlayParameters parameters(){return {initial_program_control_factors(),initial_program_control_factors(),
    {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},
    {1000},3,0,NativeProgramOutputRequest{-1,0}};}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected original MS2 path");
    AudioBanksFailure bank_failure;
    auto session=AudioBankSession::open(argv[1],std::nullopt,bank_failure);
    auto foreign=AudioBankSession::open(argv[1],std::nullopt,bank_failure);
    check(session&&foreign,"two independent real native archives");
    auto bank=session->load_bank("announcr",bank_failure);
    auto foreign_bank=foreign->load_bank("announcr",bank_failure);
    check(bank&&foreign_bank&&!bank->same_identity(*foreign_bank),"native identity is not the same bank name/index");
    auto view=session->share_native_services(bank_failure);
    check(bool(view),"actual shared registry view");
    PlaybackFailure failure;
    check(!NativePlaybackOwner::create(std::move(*view),0,failure)&&failure.kind==PlaybackError::invalid_output_rate,
        "no guessed output rate and invalid factory does not consume Session");
    std::int64_t ticks=0;unsigned samples=0;
    ProgramHostClockFailure clock_failure;
    auto clock=NativeProgramHostClock::create_with_reader(1000,[&](ProgramHostClockFailure&){++samples;return std::optional<std::int64_t>{ticks};},clock_failure);
    check(bool(clock),"explicit shared native test clock");
    auto owner=NativePlaybackOwner::create_with_clock(std::move(*view),48000,clock,failure);
    check(bool(owner)&&owner->resource_domain()==session->resource_domain(),"native owner retains exact catalog domain");
    auto context=owner->create_sound_context({3},failure);
    check(bool(context),"source-proved explicit logical boundary3");
    const auto params=parameters();AudioBankHandle unknown;
    check(!owner->play(*context,unknown,0,params,failure)&&failure.kind==PlaybackError::native_bank_failed&&
        failure.native_bank.kind==AudioBanksError::unknown_handle,"unknown native handle is diagnosed");
    check(!owner->play(*context,*foreign_bank,0,params,failure)&&failure.native_bank.kind==AudioBanksError::foreign_session,
        "same-name foreign bank never aliases this registry");
    check(!owner->play(*context,*bank,0,params,failure)&&failure.native_bank.kind==AudioBanksError::bank_not_prepared,
        "loaded metadata is not playback readiness");
    BankPreparationReport report;PreparationFailure preparation;
    check(bool(session->prepare_bank(*bank,report,preparation,bank_failure)),"actual native bank preparation");
    check(!owner->play(*context,*bank,-1,params,failure),"negative program selector never defaults");
    auto play=owner->play(*context,*bank,0,params,failure);
    check(bool(play),"native handle enters actual private runtime/copy");
    check(!owner->unload(*bank,failure)&&failure.kind==PlaybackError::active_playback,"scheduled native play blocks unload");
    auto event=owner->tick(*play,{1000},failure);
    check(event&&event->kind==ProgramStepKind::sample_start&&event->clip&&event->clip->pcm[0].samples.size()==34720,
        "original announcr selector0 starts the known actual prepared sample");
    auto pcm=owner->mix(2048,failure);
    check(pcm&&pcm->size()==4096&&std::any_of(pcm->begin(),pcm->end(),[](float v){return v!=0;}),
        "actual sample reaches existing stereo software mixer");
    check(owner->stop(*play,failure)&&bool(owner->mix(32,failure))&&owner->release(*play,failure),"normal stop/tail/release");
    auto second=session->load_bank("announcr",bank_failure);
    check(second&&!second->same_identity(*bank)&&bool(session->prepare_bank(*second,report,preparation,bank_failure)),
        "same-session duplicate name has a separate native bank lifetime");
    ticks=2000;const auto prior_samples=samples;
    NativeActivationParameters activate{params.context,params.program,params.envelope,params.source_output_mode,params.caller_play_flags,{0,0}};
    auto live=owner->activate(*context,*second,0,activate,failure);
    check(live&&samples==prior_samples+1&&owner->observe(*live,failure)->output_role==0,
        "native activation reaches actual shared clock and explicit role0");
    auto live_state=owner->observe(*live,failure);
    check(bool(owner->tick(*live,live_state->deadline,failure)),"native activated program starts");
    check(owner->unload(*bank,failure)&&session->load_state(*second,bank_failure).has_value(),
        "unloading first native identity does not retire same-name second bank");
    auto retired=owner->force_retire(*live,failure);
    check(retired&&retired->voices==1&&retired->native_resources_released,"actual forced copy/output transfer");
    auto observed=owner->observe_native_retired(retired->outputs,failure);
    check(observed&&observed->original_bank.same_identity(*second)&&observed->active_voices==1,
        "retired output retains native bank identity without a fabricated token");
    check(!owner->observe_retired(retired->outputs,failure)&&failure.kind==PlaybackError::wrong_bank_interface,
        "compatibility observation cannot manufacture native bank words");
    check(owner->unload(*second,failure),"native bank can retire after exact semantic copy release");
    check(!owner->close_when_idle(failure)&&failure.kind==PlaybackError::active_playback,"real retired tail blocks close");
    check(bool(owner->mix(32,failure))&&owner->release_sound_context(*context,failure),"actual tail drains before context retirement");
    // Compatibility methods must not touch a missing service or construct a bridge.
    AudioBankCommand wire;
    check(!owner->load_sync(wire,{7},failure)&&failure.kind==PlaybackError::wrong_bank_interface,"wire load on native owner refused");
    check(!owner->submit_async(wire,{7},failure)&&failure.kind==PlaybackError::wrong_bank_interface,"wire submission refused");
    check(!owner->run_one(failure)&&failure.kind==PlaybackError::wrong_bank_interface,"wire worker refused");
    check(!owner->poll_completion(failure)&&failure.kind==PlaybackError::wrong_bank_interface,"wire completion refused");
    auto live_context=owner->create_sound_context({3},failure);
    check(live_context&&!owner->play(*live_context,wire,params,failure)&&failure.kind==PlaybackError::wrong_bank_interface,
        "wire play cannot silently select a native path");
    wire.opcode=AudioBankOpcode::unload;
    check(!owner->unload(wire,failure)&&failure.kind==PlaybackError::wrong_bank_interface,"wire unload refused");
    check(!owner->play(*live_context,*second,0,params,failure)&&failure.native_bank.kind==AudioBanksError::unloaded_handle,
        "native revoked handle never reloads or falls back");
    check(owner->release_sound_context(*live_context,failure)&&owner->close_when_idle(failure),"native idle Session closes");
    check(!session->load_state(*second,bank_failure)&&bank_failure.kind==AudioBanksError::closed_session,"same Session close revokes catalog view");
    check(!NativePlaybackOwner::create(std::move(*session),48000,failure)&&failure.native_bank.kind==AudioBanksError::closed_session,
        "closed Session cannot publish a new playback owner");
    {
        auto revoked=AudioBankSession::open(argv[1],std::nullopt,bank_failure);
        auto candidate=revoked->load_bank("announcr",bank_failure);
        check(candidate&&revoked->prepare_bank(*candidate,report,preparation,bank_failure),"revocation real candidate");
        auto shared=revoked->share_native_services(bank_failure);
        bool close_during_clock=false,closed_in_callback=false;
        auto revoking_clock=NativeProgramHostClock::create_with_reader(1000,[&](ProgramHostClockFailure&){
            if(close_during_clock){AudioBanksFailure f;closed_in_callback=revoked->close(f);}
            return std::optional<std::int64_t>{0};
        },clock_failure);
        auto guarded=NativePlaybackOwner::create_with_clock(std::move(*shared),48000,revoking_clock,failure);
        check(bool(guarded),"revocation native owner");
        auto guarded_context=guarded->create_sound_context({3},failure);
        close_during_clock=true;
        check(!guarded->activate(*guarded_context,*candidate,0,activate,failure)&&closed_in_callback&&
            failure.kind==PlaybackError::native_bank_failed&&failure.native_bank.kind==AudioBanksError::closed_session,
            "canonical Session revocation during native clock cannot publish false playback success");
        const auto state=guarded->observe_sound_context(*guarded_context,failure);
        check(state&&state->programs==0&&state->pending_preparations==0&&state->active_voices==0,
            "failed revoked admission retires its actual private reservation/copy");
        check(guarded->release_sound_context(*guarded_context,failure),"revoked empty context explicitly retires");
    }
    std::puts("PASS native bank handles -> runtime/copy/clock/PCM/mixer/unload/retired-tail; no packets/tokens/device/game");
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
