#pragma once
#include "mka/movie_pcm_mix.hpp"
namespace mka::video {
// Conditional plain decoded-data transform, not decoder-mode admission.
// Independent predictor histories are upstream; outputs are identical queues.
std::optional<MoviePcmMixResult> movie_pcm_downmix_duplicate(
    std::span<const std::int16_t> first,std::span<const std::int16_t> second,
    MoviePcmMixFailure&) noexcept;
}
