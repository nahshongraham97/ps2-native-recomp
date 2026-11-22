#include "mka/retail_normal_envelope_intent.hpp"
namespace mka::sound {
std::optional<RetailNormalEnvelopeIntent> decode_retail_normal_envelope_setup(
    std::uint16_t attack_decay_level,std::uint16_t sustain_release,
    RetailNormalEnvelopeFailure& failure) noexcept {
    failure={};
    if(attack_decay_level!=0x000f || sustain_release!=0x1fc6) {
        failure.kind=RetailNormalEnvelopeError::unsupported_setup;
        failure.operation="retail normal envelope setup outside verified000F/1FC6";
        return {};
    }
    RetailNormalEnvelopeIntent result;
    result.attack_setting=static_cast<std::uint8_t>((attack_decay_level>>8)&127);
    result.decay_setting=static_cast<std::uint8_t>((attack_decay_level>>4)&15);
    result.sustain_setting=static_cast<std::uint8_t>((sustain_release>>6)&127);
    result.release_setting=static_cast<std::uint8_t>(sustain_release&31);
    result.sustain_level_sixteenths=static_cast<std::uint8_t>((attack_decay_level&15)+1);
    result.decay_has_one_sample_at_full_sustain=true;
    return result;
}
RetailNormalStartIntent retail_normal_start_intent(std::uint16_t flags) noexcept {
    return {(flags&8u)==0,true,true};
}
}
