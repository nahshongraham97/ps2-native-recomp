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

// Exact source-game UI category construction. Percent clamping is game policy;
// arbitrary finite profile bases are looked up separately without quantization.
float movie_gain_ui_base(std::int64_t percent) noexcept;
float movie_gain_default_base() noexcept;

// Immutable compiled integer interval lookup for every finite binary32 base,
// using magnitude bits (including signed zeros/subnormals). Inf/NaN fail named.
// No runtime file/environment/math helper, nearest-value matching, profile
// normalization or unity substitution. Fade4 always mutes: a reviewed native
// capacity extension eliminates legacy intermediate-overflow noncancellation.
// The integer result is signed tenths of a dB, not a linear PCM coefficient.
std::optional<std::int32_t> movie_gain_initial(float exact_base,
                                             MovieGainFailure&) noexcept;
std::optional<std::int32_t> movie_gain_fade(const MovieFadeStep&,
                                          MovieGainFailure&) noexcept;
const char* movie_gain_error_name(MovieGainError) noexcept;

// Player/backend binding, initial-volume chronology, profile storage/invalid
// legacy types and host gain coefficients remain separate contracts. Complete
// finite lookup does not claim legacy large-base/hardware/binary parity.
} // namespace mka::video
