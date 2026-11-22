#pragma once
#include <cstdint>
#include <optional>
namespace mka::sound {
// Compatibility metadata decoded into named game intent,not an envelope engine.
// Rate settings require verified native timing realization before playback.
enum class RetailEnvelopeCurve { linear_increase, exponential_decrease, linear_decrease };
struct RetailNormalEnvelopeIntent {
    RetailEnvelopeCurve attack{RetailEnvelopeCurve::linear_increase};
    RetailEnvelopeCurve decay{RetailEnvelopeCurve::exponential_decrease};
    RetailEnvelopeCurve sustain{RetailEnvelopeCurve::linear_increase};
    RetailEnvelopeCurve release{RetailEnvelopeCurve::linear_decrease};
    std::uint8_t attack_setting{},decay_setting{},sustain_setting{},release_setting{};
    std::uint8_t sustain_level_sixteenths{};
    bool decay_has_one_sample_at_full_sustain{};
};
enum class RetailNormalEnvelopeError { none, unsupported_setup, timing_realization_unbound };
struct RetailNormalEnvelopeFailure {
    RetailNormalEnvelopeError kind{RetailNormalEnvelopeError::none};
    const char* operation{""};
};
// Only the source-proved000F/1FC6 pair is admitted. Other compatibility data
// remains unsupported rather than being treated as the same default.
std::optional<RetailNormalEnvelopeIntent> decode_retail_normal_envelope_setup(
    std::uint16_t attack_decay_level,std::uint16_t sustain_release,
    RetailNormalEnvelopeFailure&) noexcept;
struct RetailNormalStartIntent {
    bool key_on_after_configuration{};
    bool freeze_source_before_release{};
    bool retain_release_tail{};
};
// Normal A49C/6330: flags mask8 means held configuration without key-on.
// Source BF3C/C1E8 writes pitch0 before key-off;existing native owners retain tail.
RetailNormalStartIntent retail_normal_start_intent(std::uint16_t program_flags) noexcept;
}
