#pragma once
#include "mka/retail_normal_envelope_intent.hpp"
#include "mka/host_voice.hpp"
namespace mka::sound::normal_envelope_required {
// REQUIRED,UNIMPLEMENTED: exact native coefficients,phase order and resampling
// from source-proved timing. Caller supplies actual queried host output rate.
// Intent decode alone does not create an accepted playback profile.
std::optional<NativeEnvelopeProfile> realize_retail_normal_envelope_profile(
    const RetailNormalEnvelopeIntent&,std::uint32_t queried_host_output_rate,
    RetailNormalEnvelopeFailure&);
}
