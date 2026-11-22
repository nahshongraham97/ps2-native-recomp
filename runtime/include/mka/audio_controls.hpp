#pragma once
#include "mka/program_executor.hpp"
namespace mka::sound {
enum class AudioControlCurve { initial_nonlinear, startup_linear };
constexpr bool valid_audio_control_curve(AudioControlCurve value) noexcept {
    return value==AudioControlCurve::initial_nonlinear || value==AudioControlCurve::startup_linear;
}
// Explicit game/context factors. No implicit neutral/default settings.
struct AudioControlFactors {
    std::int32_t gain_q14, pan_q14, pitch_q12;
    std::int32_t secondary_gain_q14, secondary_pan_q14, secondary_pitch_q12;
};
AudioControlFactors initial_program_control_factors() noexcept;
enum class AudioControlError { none, not_sample_start, missing_clip, unsupported_spatial_mode,
    unsupported_deferred_key_on, unsupported_noncenter_pan, unsupported_repeat,
    unsupported_start_argument, unsupported_descriptor_word, unsupported_arithmetic_overflow,
    unsupported_zero_pitch, invalid_plane_count, invalid_program_factor_encoding, unsupported_dynamic_pan_update,
    unsupported_curve };
struct AudioControlFailure {
    AudioControlError kind=AudioControlError::none;
    std::uint64_t desired_frequency_numerator=0; // Kept even on unsupported source-overflow.
};
enum class AudioControlUse { fresh_start, dynamic_update };
struct NativeAudioControls {
    std::int32_t effective_gain_q14=0, effective_pan_q14=0;
    std::int32_t routed_pan_q14=0; // After source mode1 fold; effectivepan retains original clamped sum.
    std::int64_t effective_pitch_q12=0;
    std::uint64_t desired_frequency_numerator=0; // /4096, before reference quantization.
    std::uint64_t sample_step_q12=0; // Wide floor(rate*pitch/48000); never masked to16 bits.
    std::uint16_t reference_packed_pitch_word=0; // Evidence only; NOT a native sampler limit.
    bool reference_word_truncated=false;
    std::uint32_t source_output_mode=0;
    std::size_t planes=0;
    std::array<std::array<std::int32_t,2>,2> gains_q15{}; // [ordered plane][host L/R].
    AudioControlCurve curve=AudioControlCurve::initial_nonlinear;
};
// Proved ordinary fresh-start subset, including exact nonlinear/phase pan.
// Freshness is source3259C descriptor50/52=F000,56=0, not neutral gain/pitch.
// All factors/outputmode are explicit; dynamic history/smoothing is unsupported.
// Unknown spatial/loop/start-offset behavior fails rather than inventing it.
std::optional<NativeAudioControls> decode_audio_controls(const ProgramStep&,
    const AudioControlFactors& context, const AudioControlFactors& program,
    std::uint32_t source_output_mode, AudioControlFailure&,
    AudioControlUse=AudioControlUse::fresh_start,
    AudioControlCurve=AudioControlCurve::initial_nonlinear);
} // namespace mka::sound
