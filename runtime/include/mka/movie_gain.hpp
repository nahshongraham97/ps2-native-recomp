#pragma once

#include "mka/movie_control.hpp"
#include <cstdint>
#include <optional>

namespace mka::video {

enum class MovieGainError {
    none, unsupported_base, unsupported_fade_total, unsupported_fade_step
};
struct MovieGainFailure {
    MovieGainError kind = MovieGainError::none;
    const char* operation = ""; // static storage
};

// Exact source-game UI categories, not an approximation of arbitrary profile
// floats. Percent clamping is game policy; no backend/resource limit involved.
float movie_gain_ui_base(std::int64_t percent) noexcept;
float movie_gain_default_base() noexcept;

// Immutable compiled source-derived lookup. No runtime file/environment/math
// helper, nearest-value matching, profile normalization or unity substitution.
// The integer result is signed tenths of a dB, not a linear PCM coefficient.
std::optional<std::int32_t> movie_gain_initial(float exact_base,
                                             MovieGainFailure&) noexcept;
std::optional<std::int32_t> movie_gain_fade(const MovieFadeStep&,
                                          MovieGainFailure&) noexcept;
const char* movie_gain_error_name(MovieGainError) noexcept;

// Player/backend binding, initial-volume chronology, arbitrary profile domain
// and conversion of tenths-dB into actual host gain remain separate contracts.
} // namespace mka::video
