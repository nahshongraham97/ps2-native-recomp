#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>
namespace mka::video {
enum class AdxBlockMode { unspecified,version4_flags0 };
struct AdxBlockProfile {
    AdxBlockMode mode;
    std::vector<std::pair<std::int16_t,std::int16_t>> coefficients;
    std::vector<std::pair<std::int16_t,std::int16_t>> histories; // latest, older
    AdxBlockProfile(AdxBlockMode m,std::vector<std::pair<std::int16_t,std::int16_t>> c,
        std::vector<std::pair<std::int16_t,std::int16_t>> h):mode(m),coefficients(std::move(c)),histories(std::move(h)){}
};
enum class AdxBlockError { none,unsupported_profile,invalid_profile,invalid_extent,marker,host_capacity,allocation };
struct AdxBlockFailure { AdxBlockError kind=AdxBlockError::none;const char* operation=""; };
struct AdxBlockResult {
    std::vector<std::vector<std::int16_t>> planes;
    std::vector<std::pair<std::int16_t,std::int16_t>> final_histories;
    std::size_t frames=0;
};
// Original encoded block data only, with explicitly supplied coefficients and
// seeds. No header/defaults/trim/end policy or initial decoder-mode selection.
std::optional<AdxBlockResult> decode_movie_adx_blocks(std::span<const std::uint8_t>,
    const AdxBlockProfile&,AdxBlockFailure&) noexcept;
}
