#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace mka::video {
enum class MoviePcmMixMode { unspecified, generic_stateless };
enum class MoviePcmMixError { none, unsupported_profile, invalid_matrix, invalid_planes, invalid_gain, host_capacity, allocation };
struct MoviePcmMixFailure { MoviePcmMixError kind=MoviePcmMixError::none;const char* operation=""; };
const char* movie_pcm_mix_error_name(MoviePcmMixError) noexcept;
struct MoviePcmGain { std::int32_t effective_tenth_db;std::uint16_t table_value; };
// Both operands mandatory. Native wide addition BEFORE the source GAME clamp.
MoviePcmGain movie_pcm_gain(std::int64_t requested_tenth_db,std::int64_t metadata_tenth_db) noexcept;
// Explicit output0/output1 identities only; no speaker/plane/profile inference.
std::optional<std::array<std::uint16_t,2>> movie_pcm_pan(std::uint16_t gain_table_value,
    std::int64_t explicit_pan,MoviePcmMixFailure&) noexcept;
struct MoviePcmMixProfile {
    MoviePcmMixMode mode=MoviePcmMixMode::unspecified;
    std::size_t output_count=0;
    // [source plane][output], each explicit integer coefficient0..255.
    std::vector<std::vector<std::uint16_t>> coefficients;
};
struct MoviePcmMixResult { std::vector<std::vector<std::int16_t>> outputs;std::size_t frames=0; };
// Stateless owned result. Dynamic planes/frames/outputs; no device/block caps.
std::optional<MoviePcmMixResult> mix_movie_pcm(const MoviePcmMixProfile&,
    std::span<const std::span<const std::int16_t>> original_planes,MoviePcmMixFailure&) noexcept;
// Exact FINAL signed16 value *2^-15, interleaved explicit output order.
// Native representation only, not historical analog gain/fidelity proof.
std::optional<std::vector<float>> movie_pcm_float_interleaved(const MoviePcmMixResult&,
    MoviePcmMixFailure&) noexcept;
} // namespace mka::video
