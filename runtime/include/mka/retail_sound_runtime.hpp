#pragma once
#include "mka/playback_owner.hpp"
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
// Native Windows output/thread ownership for the actual current context.
// It advances the admitted ordinary normal route; source control/queued-role
// producers, alternate flags9 and full worker/DSP equivalence remain required.
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
    bool shutdown(RetailSoundRuntimeFailure&);
private:
    struct State;
    explicit NativeRetailSoundRuntime(std::shared_ptr<State>);
    std::shared_ptr<State> state_;
};
}
