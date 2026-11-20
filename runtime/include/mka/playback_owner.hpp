#pragma once
#include "mka/audio_bank_service.hpp"
#include "mka/host_voice.hpp"
namespace mka::sound {
class NativePlaybackOwner;
// Explicit native ownership, unrelated to serialized context words or gain data.
class NativeSoundContext {
public:
    NativeSoundContext()=default;
private:
    std::shared_ptr<const void> owner_,identity_;
    friend class NativePlaybackOwner;
};
struct SoundContextObservation {
    std::size_t pending_preparations=0,programs=0,active_voices=0;
};
class NativePlayHandle {
public:
    NativePlayHandle()=default;
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
};
enum class PlaybackError { none, moved_owner, closed, invalid_output_rate,
    unknown_handle, foreign_owner, retired_handle, active_playback,
    service_failed, program_failed, voice_failed, failed_playback_requires_stop,
    allocation_failed, host_capacity, native_exception, operation_active, closing,
    bank_unloading, bank_state_unknown, close_state_unknown, unknown_context, foreign_context, retired_context };
struct PlaybackFailure {
    PlaybackError kind=PlaybackError::none;
    BankServiceFailure service;
    PreparationFailure preparation;
    ProgramFailure program;
    HostVoiceFailure voice;
};
struct PlaybackObservation {
    ProgramClockWord deadline;
    std::size_t command_index=0, voices=0, active_voices=0;
    bool executor_finished=false, explicitly_stopped=false;
    std::optional<std::uint64_t> first_voice_source_index;
    std::optional<PlaybackFailure> terminal_failure;
};
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
    std::optional<BankLoadOutcome> load_sync(const AudioBankCommand&,BankWireToken,PlaybackFailure&);
    std::optional<AsyncBankSubmission> submit_async(const AudioBankCommand&,BankWireToken,PlaybackFailure&);
    bool run_one(PlaybackFailure&);
    std::optional<BankLoadCompletion> poll_completion(PlaybackFailure&);
    std::optional<NativeSoundContext> create_sound_context(PlaybackFailure&);
    std::optional<SoundContextObservation> observe_sound_context(const NativeSoundContext&,PlaybackFailure&) const;
    // Idle native teardown only: this is NOT retail command12/forced cleanup1.
    bool release_sound_context(const NativeSoundContext&,PlaybackFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
    std::optional<NativePlayHandle> play(const NativeSoundContext&,const AudioBankCommand&,
        const NativePlayParameters&,PlaybackFailure&);
    // At most one source operation per call. Start acknowledgement occurs only
    // after an owned HostVoice was created and inserted successfully.
    std::optional<ProgramStep> tick(const NativePlayHandle&,ProgramClockWord,PlaybackFailure&);
    bool stop(const NativePlayHandle&,PlaybackFailure&);
    std::optional<std::vector<float>> mix(std::size_t host_frames,PlaybackFailure&);
    std::optional<PlaybackObservation> observe(const NativePlayHandle&,PlaybackFailure&) const;
    bool release(const NativePlayHandle&,PlaybackFailure&); // Only inactive entries.
    // No automatic pending-play cancellation/unload rule is inferred. A source
    // program awaiting its deadline or any active/releasing voice blocks unload.
    bool unload(const AudioBankCommand&,PlaybackFailure&);
    bool close_when_idle(PlaybackFailure&);
private:
    struct State;
    explicit NativePlaybackOwner(std::shared_ptr<State> s):state_(std::move(s)){}
    std::shared_ptr<State> state_;
};
} // namespace mka::sound
