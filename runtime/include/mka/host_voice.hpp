#pragma once
#include "mka/audio_controls.hpp"
namespace mka::sound {
// Ordinary sample-domain ADSR coefficients supplied explicitly by the caller.
// No register layout, core, device, IOP cycle counter or implicit unity profile.
struct NativeEnvelopeRate { std::uint64_t period; std::int32_t step; bool exponential; };
struct NativeEnvelopeProfile {
    NativeEnvelopeRate attack, decay, sustain, release;
    std::int32_t sustain_threshold_q15;
};
enum class HostVoiceError { none, control_failed, invalid_output_rate, unsupported_envelope,
    invalid_clip, moved_voice, wrong_stop_event, foreign_program, unsupported_sample_end_control,
    arithmetic_overflow, null_voice, duplicate_voice, output_rate_mismatch,
    allocation_failed, host_capacity };
struct HostVoiceFailure { HostVoiceError kind=HostVoiceError::none; AudioControlFailure control; std::size_t frame_index=0, voice_index=0; };
class HostVoice;
// Additive float stereo in ordinary host memory, before any host master/output
// limiter. Empty voice set is explicit silence. Failure publishes no partial
// output and advances NONE of the voices, including a valid prefix.
std::optional<std::vector<float>> mix_host_voices(HostVoice* const*,std::size_t voices,
                                                std::size_t frames,HostVoiceFailure&);
class HostVoice {
public:
    HostVoice(const HostVoice&)=delete;
    HostVoice& operator=(const HostVoice&)=delete;
    HostVoice(HostVoice&&) noexcept;
    HostVoice& operator=(HostVoice&&) noexcept;
    ~HostVoice();
    static std::optional<HostVoice> start(const ProgramStep&,const AudioControlFactors& context,
        const AudioControlFactors& program,std::uint32_t source_output_mode,
        const NativeEnvelopeProfile&,std::uint32_t explicit_host_output_rate,HostVoiceFailure&,
        AudioControlCurve=AudioControlCurve::initial_nonlinear);
    std::optional<std::vector<float>> render(std::size_t frames,HostVoiceFailure&);
    // Proven normal stop path: freeze source position and enter release envelope.
    bool stop_from_program(const ProgramStep&,HostVoiceFailure&);
    bool validate_program_owner(const std::shared_ptr<const PreparedProgram>&,HostVoiceFailure&) const noexcept;
    // Typed resource detachment uses the same real normal release, without
    // fabricating ordinary ProgramStep cleanup metadata or callback completion.
    bool stop_for_resource_retirement(const std::shared_ptr<const PreparedProgram>&,HostVoiceFailure&) noexcept;
    bool active() const noexcept;
    std::uint64_t source_index() const noexcept;
    std::uint32_t output_rate() const noexcept;
private:
    struct Data;
    explicit HostVoice(std::unique_ptr<Data>);
    std::unique_ptr<Data> data_;
    friend std::optional<std::vector<float>> mix_host_voices(HostVoice* const*,std::size_t,std::size_t,HostVoiceFailure&);
};
} // namespace mka::sound
