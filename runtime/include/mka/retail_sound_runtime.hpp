#pragma once
#include "mka/playback_owner.hpp"
#include "mka/pending_play.hpp"
#include "mka/sound_output.hpp"
#include <memory>
namespace mka::retail {
enum class RetailSoundRuntimeError {none,allocation,host_capacity,native_exception,wrong_thread,
    output,invalid_output_format,playback,inconsistent_output,active_playback,closed};
struct RetailSoundRuntimeFailure {
    RetailSoundRuntimeError kind{RetailSoundRuntimeError::none};
    const char* operation{""};
    sound::AudioDeviceFailure output,cleanup;
    sound::PlaybackFailure playback;
    std::optional<sound::AudioSubmission> submission;
    std::uint64_t mixed_unsubmitted_frames{};
    std::int64_t native_code{};
};
struct RetailSoundRuntimeObservation {
    sound::AudioStreamFormat format;
    std::uint64_t worker_passes{},submitted_frames{};
    std::uint64_t pending_pcm_frames{};
    std::uint32_t observed_padding_frames{};
    bool output_started{};
};
struct RetailSoundRuntimeStatus {
    sound::AudioControlFactors controls;
    std::vector<sound::NativePlayHandle> active_plays;
    RetailSoundRuntimeObservation native;
};
struct RetailSoundPlayCompletion {
    sound::NativePlayHandle handle;
    sound::NativePlayWorkObservation result;
    bool resume_delivered=false;
    std::optional<sound::PlaybackFailure> resume_failure;
};
// Native Windows output/thread ownership for the actual current context.
// It owns admitted ordinary and held normal work, including actual resume.
// Source parameter/control producers, alternate sample classes and full
// worker/DSP equivalence remain required.
// Lifecycle belongs to Main's thread. Output/COM belongs to the worker thread.
// Shutdown requires idle playback across the owner and drains pending/queued
// client PCM on the worker. This is not a claim that physical sound was heard.
// It never disposes live programs or banks.
class NativeRetailSoundRuntime final {
public:
    static std::unique_ptr<NativeRetailSoundRuntime> create(sound::AudioBankSession&&,RetailSoundRuntimeFailure&);
    ~NativeRetailSoundRuntime();
    NativeRetailSoundRuntime(const NativeRetailSoundRuntime&)=delete;
    NativeRetailSoundRuntime& operator=(const NativeRetailSoundRuntime&)=delete;
    std::shared_ptr<sound::NativePlaybackOwner> playback_owner() const;
    sound::NativeSoundContext sound_context() const;
    // Original context290..2A4/270 defaults, not an envelope/output-rate profile.
    static sound::AudioControlFactors initial_context_controls() noexcept;
    static constexpr std::uint32_t initial_output_mode() noexcept{return 3;}
    std::optional<RetailSoundRuntimeObservation> observe(RetailSoundRuntimeFailure&) const;
    std::optional<RetailSoundRuntimeStatus> snapshot_status(RetailSoundRuntimeFailure&) const;
    // Production EE8/E28 admission: real cached/sample-ready construction may
    // complete before return; unavailable samples use the resource worker.
    std::optional<sound::NativePlayHandle> submit_play(const sound::AudioBankHandle&,std::int32_t,
        const sound::NativeActivationParameters&,RetailSoundRuntimeFailure&);
    std::optional<sound::NativePlayHandle> submit_queued_play(const sound::AudioBankHandle&,std::int32_t,
        const sound::NativeActivationParameters&,RetailSoundRuntimeFailure&);
    std::optional<sound::ProgramWorkCancellation> cancel_pending_play(const sound::NativePlayHandle&,RetailSoundRuntimeFailure&);
    // Pending work is promoted, then resumed only after its real consumer.
    // Ready native plays resume directly; a genuinely retired identity is absent.
    bool request_resume(const sound::NativePlayHandle&,RetailSoundRuntimeFailure&);
    std::optional<RetailSoundPlayCompletion> poll_play_completion(RetailSoundRuntimeFailure&);
    bool shutdown(RetailSoundRuntimeFailure&);
private:
    struct State;
    explicit NativeRetailSoundRuntime(std::shared_ptr<State>);
    std::optional<sound::NativePlayHandle> submit_play_impl(const sound::AudioBankHandle&,std::int32_t,
        const sound::NativeActivationParameters&,bool explicitly_queued,RetailSoundRuntimeFailure&);
    std::shared_ptr<State> state_;
};
}
