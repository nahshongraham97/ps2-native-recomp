#pragma once
#include "mka/audio_bank_service.hpp"
#include "mka/program_copy.hpp"
#include "mka/program_host_clock.hpp"
namespace mka::sound {
class NativePlaybackOwner;
class NativePlayWork;
struct NativePreparedPlay;
// Explicit native ownership, unrelated to serialized context words or gain data.
class NativeSoundContext {
public:
    NativeSoundContext()=default;
    bool same_identity(const NativeSoundContext& other) const noexcept {
        return owner_&&identity_&&owner_==other.owner_&&identity_==other.identity_;
    }
private:
    std::shared_ptr<const void> owner_,identity_;
    friend class NativePlaybackOwner;
};
struct NativeSoundContextConfiguration {std::size_t explicit_role_count;};
// Source selectors are compatibility data; storage and automatic allocation
// are native and grow without the original48entry hardware-era capacity.
struct NativeProgramOutputRequest {std::int32_t selector;std::int16_t priority;};
struct SoundContextObservation {
    std::size_t pending_preparations=0,programs=0,active_voices=0;
    std::size_t retired_groups=0,retired_voices=0,active_retired_voices=0;
};
class NativePlayHandle {
public:
    NativePlayHandle()=default;
    // Identity presence only; the actual owner still determines liveness.
    bool empty() const noexcept {return !owner_||!identity_;}
    bool same_identity(const NativePlayHandle& other) const noexcept {
        return owner_&&identity_&&owner_==other.owner_&&identity_==other.identity_;
    }
private:
    std::shared_ptr<const void> owner_, identity_;
    friend class NativePlaybackOwner;
};
// All values are required from the actual caller. No factory profile, neutral
// per-play gain, guessed deadline or implicit output-device rate is selected.
struct NativePlayParameters {
    AudioControlFactors context, program;
    NativeEnvelopeProfile envelope;
    ProgramClockWord initial_deadline;
    std::uint32_t source_output_mode;
    std::uint16_t caller_play_flags;
    NativeProgramOutputRequest output;
    NativePlayParameters(AudioControlFactors c,AudioControlFactors p,NativeEnvelopeProfile e,
        ProgramClockWord already_activated_deadline,std::uint32_t mode,std::uint16_t flags,NativeProgramOutputRequest role)
        :context(c),program(p),envelope(e),initial_deadline(already_activated_deadline),
         source_output_mode(mode),caller_play_flags(flags),output(role){}
};
struct NativeActivationParameters {
    AudioControlFactors context,program;
    NativeEnvelopeProfile envelope;
    std::uint32_t source_output_mode;
    std::uint16_t caller_play_flags;
    NativeProgramOutputRequest output;
};
enum class PlaybackError { none, moved_owner, closed, invalid_output_rate,
    unknown_handle, foreign_owner, retired_handle, active_playback,
    service_failed, program_failed, voice_failed, failed_playback_requires_stop,
    allocation_failed, host_capacity, native_exception, operation_active, closing,
    bank_unloading, bank_state_unknown, close_state_unknown, unknown_context, foreign_context, retired_context,
    unsupported_play_flags, copy_failed, invalid_output_selector, unsupported_output_selector,
    output_priority_refused, clock_failed, invalid_clock, unknown_retired_output, retired_output,
    wrong_bank_interface, native_bank_failed, invalid_control_curve, wrong_thread, invalid_work_state, cancelled };
struct PlaybackFailure {
    PlaybackError kind=PlaybackError::none;
    BankServiceFailure service;
    PreparationFailure preparation;
    ProgramFailure program;
    HostVoiceFailure voice;
    ProgramCopyFailure copy;
    ProgramHostClockFailure clock;
    AudioBanksFailure native_bank;
};
struct PlaybackObservation {
    ProgramClockWord deadline;
    std::size_t command_index=0, voices=0, active_voices=0;
    bool executor_finished=false, explicitly_stopped=false;
    bool private_copy_bound=false,resources_retired=false;
    std::uint16_t effective_play_flags=0;
    std::optional<std::uint64_t> first_voice_source_index;
    std::optional<PlaybackFailure> terminal_failure;
    std::optional<std::size_t> output_role;
    bool output_owned=false,dispatch_live=false,replaced=false;
    std::int16_t priority=0;
};
class NativeRetiredOutputHandle {
public:
    NativeRetiredOutputHandle()=default;
private:
    std::shared_ptr<const void> owner_,identity_;
    friend class NativePlaybackOwner;
};
struct ForcedPlaybackRetirement {
    NativeRetiredOutputHandle outputs;
    std::size_t voices=0;
    bool native_resources_released=false;
};
struct RetiredOutputObservation {
    std::size_t voices=0,active_voices=0;
    std::optional<std::uint64_t> first_voice_source_index;
    BankWireToken original_bank;
    bool native_resources_released=false;
};
struct NativeRetiredOutputObservation {
    std::size_t voices=0,active_voices=0;
    std::optional<std::uint64_t> first_voice_source_index;
    AudioBankHandle original_bank;
    bool native_resources_released=false;
};
struct SoundContextStep {
    NativePlayHandle handle;
    std::optional<ProgramStep> event;
    std::optional<PlaybackFailure> failure;
};
struct SoundContextUpdate {ProgramClockWord observed;std::vector<SoundContextStep> steps;};
// Exclusive bank-service owner and native program/voice registry. Calls are
// serialized only for admission, commit and executor/voice mutation. Native
// resource work never holds the registry mutex. Source clock stepping and audio frames are explicit, separate
// inputs: this does not invent an output-device scheduler or timing equivalence.
class NativePlaybackOwner {
public:
    NativePlaybackOwner(const NativePlaybackOwner&)=delete;
    NativePlaybackOwner& operator=(const NativePlaybackOwner&)=delete;
    NativePlaybackOwner(NativePlaybackOwner&&) noexcept=default;
    NativePlaybackOwner& operator=(NativePlaybackOwner&&) noexcept=default;
    static std::optional<NativePlaybackOwner> create(AudioBankService&&,
        std::uint32_t explicit_output_rate,PlaybackFailure&);
    static std::optional<NativePlaybackOwner> create_with_clock(AudioBankService&&,
        std::uint32_t,std::shared_ptr<NativeProgramHostClock>,PlaybackFailure&);
    // Accepted native ingress: retains the actual catalog Session/handles.
    // No packet, wire token, compatibility service or second archive is made.
    static std::optional<NativePlaybackOwner> create(AudioBankSession&&,
        std::uint32_t explicit_output_rate,AudioControlCurve,PlaybackFailure&);
    static std::optional<NativePlaybackOwner> create_with_clock(AudioBankSession&&,
        std::uint32_t,AudioControlCurve,std::shared_ptr<NativeProgramHostClock>,PlaybackFailure&);
    std::optional<BankLoadOutcome> load_sync(const AudioBankCommand&,BankWireToken,PlaybackFailure&);
    std::optional<AsyncBankSubmission> submit_async(const AudioBankCommand&,BankWireToken,PlaybackFailure&);
    bool run_one(PlaybackFailure&);
    std::optional<BankLoadCompletion> poll_completion(PlaybackFailure&);
    std::optional<NativeSoundContext> create_sound_context(NativeSoundContextConfiguration,PlaybackFailure&);
    std::optional<SoundContextObservation> observe_sound_context(const NativeSoundContext&,PlaybackFailure&) const;
    // Aggregate actual ownership for native output teardown, including plays
    // and retained tails in every context of this same owner.
    std::optional<SoundContextObservation> observe_all_outputs(PlaybackFailure&) const;
    // Command99's active association observation, using actual native handles.
    // The source registry is owner-wide; context validates the request target.
    // No preparation, stopped entry, voice tail or fixed transport capacity.
    std::optional<std::vector<NativePlayHandle>> snapshot_active_plays(const NativeSoundContext&,PlaybackFailure&) const;
    // Idle native teardown only: this is NOT retail command12/forced cleanup1.
    bool release_sound_context(const NativeSoundContext&,PlaybackFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
    std::optional<NativePlayHandle> play(const NativeSoundContext&,const AudioBankCommand&,
        const NativePlayParameters&,PlaybackFailure&);
    // Actual native activation samples the retained shared clock after real
    // preparation/ordinary incumbent cleanup; initial deadline is not supplied.
    std::optional<NativePlayHandle> activate(const NativeSoundContext&,const AudioBankCommand&,
        const NativeActivationParameters&,PlaybackFailure&);
    std::optional<NativePlayHandle> play(const NativeSoundContext&,const AudioBankHandle&,
        std::int32_t selector,const NativePlayParameters&,PlaybackFailure&);
    std::optional<NativePlayHandle> activate(const NativeSoundContext&,const AudioBankHandle&,
        std::int32_t selector,const NativeActivationParameters&,PlaybackFailure&);
    // Explicit queued primitive; source direct-versus-worker routing is separate.
    // Admission exposes one owned pending identity without starting resources.
    std::shared_ptr<NativePlayWork> admit_queued_play(const NativeSoundContext&,const AudioBankHandle&,
        std::int32_t selector,const NativeActivationParameters&,PlaybackFailure&);
    // One clock sample; dispatch every due command per ordinary live entry. Other original
    // update passes/callbacks are distinct, still unbound work. A failed step
    // retains its exact prefix/results; no fabricated all-or-nothing update.
    bool update_ordinary_context(const NativeSoundContext&,SoundContextUpdate&,PlaybackFailure&);
    // At most one source operation per call. Start acknowledgement occurs only
    // after an owned HostVoice was created and inserted successfully.
    std::optional<ProgramStep> tick(const NativePlayHandle&,ProgramClockWord,PlaybackFailure&);
    bool stop(const NativePlayHandle&,PlaybackFailure&);
    std::optional<std::vector<float>> mix(std::size_t host_frames,PlaybackFailure&);
    std::optional<PlaybackObservation> observe(const NativePlayHandle&,PlaybackFailure&) const;
    bool release(const NativePlayHandle&,PlaybackFailure&); // Only inactive entries.
    // Typed normal resource retirement, distinct from script mode0 stop. Real
    // voices keep releasing in the native mixer after clone/semantic disposal.
    // These receipts do NOT complete source jobs/notifications/command12.
    std::optional<ForcedPlaybackRetirement> force_retire(const NativePlayHandle&,PlaybackFailure&);
    std::optional<std::vector<ForcedPlaybackRetirement>> force_retire_context_outputs(const NativeSoundContext&,PlaybackFailure&);
    std::optional<RetiredOutputObservation> observe_retired(const NativeRetiredOutputHandle&,PlaybackFailure&) const;
    std::optional<NativeRetiredOutputObservation> observe_native_retired(const NativeRetiredOutputHandle&,PlaybackFailure&) const;
    // No automatic pending-play cancellation/unload rule is inferred. A source
    // program awaiting its deadline or any active/releasing voice blocks unload.
    bool unload(const AudioBankCommand&,PlaybackFailure&);
    bool unload(const AudioBankHandle&,PlaybackFailure&);
    bool close_when_idle(PlaybackFailure&);
private:
    friend class NativePlayWork;
    struct State;
    static std::unique_ptr<NativePreparedPlay> prepare_play_plan(std::unique_ptr<RuntimeProgramReference>&,
        const NativePlayParameters&,PlaybackFailure&);
    static bool commit_play_plan(const std::shared_ptr<State>&,const NativeSoundContext&,
        const NativePlayHandle&,NativePreparedPlay&,bool,PlaybackFailure&);
    std::optional<NativePlayHandle> play_impl(const NativeSoundContext&,const AudioBankCommand*,
        const AudioBankHandle*,std::int32_t selector,const NativePlayParameters&,bool sample_activation_clock,PlaybackFailure&);
    bool unload_impl(const AudioBankCommand*,const AudioBankHandle*,PlaybackFailure&);
    std::optional<std::vector<ForcedPlaybackRetirement>> force_retire_impl(const NativePlayHandle*,const NativeSoundContext*,PlaybackFailure&);
    explicit NativePlaybackOwner(std::shared_ptr<State> s):state_(std::move(s)){}
    std::shared_ptr<State> state_;
};
} // namespace mka::sound
