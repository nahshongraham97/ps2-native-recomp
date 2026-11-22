#pragma once
#include "mka/retail_sound_runtime.hpp"
#include <array>
namespace mka::retail {
// Typed game publication of314F30. Native diagnostics replace the removed IOP
// heap/arena fields; no hardware pool, wire reply or packed play ID is retained.
struct NativeRetailSoundStatus {
    std::array<float,6> controls;
    std::vector<sound::NativePlayHandle> active_plays;
    RetailSoundRuntimeObservation native;
};
std::array<float,6> retail_sound_status_controls(const sound::AudioControlFactors&) noexcept;
}
