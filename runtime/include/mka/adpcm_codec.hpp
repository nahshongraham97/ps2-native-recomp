#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
namespace mka::sound {
struct AdpcmHistory { std::int16_t s1 = 0, s2 = 0; };
enum class AdpcmError { none, truncated_block, null_input, unsupported_filter, unsupported_shift, host_capacity, allocation_failed };
struct AdpcmBlock { std::array<std::int16_t,28> samples{}; std::uint8_t flags = 0; };
struct AdpcmMono { std::vector<std::int16_t> samples; std::vector<std::uint8_t> block_flags; };
// Explicit 16-byte PS-style ADPCM codec subset, not container autodetection.
// Flags remain opaque; no loop, envelope, device, resampling or stereo behavior.
// On any failure history/output are unchanged, including after a valid prefix.
AdpcmError decode_adpcm_block(const std::uint8_t*, std::size_t, AdpcmHistory&, AdpcmBlock&) noexcept;
std::optional<AdpcmMono> decode_adpcm_mono(const std::uint8_t*, std::size_t, AdpcmHistory&, AdpcmError&);
}
