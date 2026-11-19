#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>
namespace mka::video {
enum class MovieAdxHeaderError {
    none,incomplete,signature,extent,unsupported_profile,invalid_field,host_capacity,allocation
};
struct MovieAdxHeaderFailure {
    MovieAdxHeaderError kind=MovieAdxHeaderError::none;
    const char* operation="";
};
struct MovieAdxHeader {
    // Exactly the declared elementary-stream header; trailing packet bytes are
    // neither copied nor consumed. Unknown/opaque header bytes remain intact.
    std::vector<std::uint8_t> raw_header;
    std::uint64_t data_start=0;
    std::uint8_t encoding=0,block_bytes=0,sample_bits=0,source_planes=0;
    std::uint64_t sample_rate=0,declared_samples=0;
    std::uint16_t cutoff=0;
    std::uint8_t version=0,flags=0;
    // Correct final backend mapping, not the parser temporary-array grouping.
    // Each pair is latest,older: plane0 header18/1A; plane1 header1C/1E.
    std::array<std::pair<std::int16_t,std::int16_t>,2> histories{};
};
// Header bytes begin at ADX ES byte0, not a container/file offset. Admit only
// encoding3/block18/bits4/planes2/version4/flags0. Positive other rates/cutoffs
// are retained data, NOT coefficient/profile admission. No header search,
// zero history, coefficient selection, speaker mapping or trim/end policy.
std::optional<MovieAdxHeader> parse_movie_adx_header(std::span<const std::uint8_t>,
    MovieAdxHeaderFailure&) noexcept;
} // namespace mka::video
