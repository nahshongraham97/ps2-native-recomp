#pragma once
#include "mka/mpeg2_timing.hpp"
#include <cstdint>
#include <optional>
namespace mka::video {
enum class MovieTimecodeError {none,missing_gop,missing_timestamp,unsupported_rate,invalid_temporal_reference,unsupported_fp_environment,overflow};
struct MovieTimecodeFailure {MovieTimecodeError kind=MovieTimecodeError::none;};
const char* movie_timecode_error_name(MovieTimecodeError) noexcept;
// Native arithmetic descriptor, not a serialized SDK struct. All timeline
// scalars are64bit; original normal packet fields map without narrowing. The
// original conversion does not read the other descriptor flag at+1C.
struct MovieTimecodeDescriptor {
    std::uint8_t frame_rate_code=0;
    bool drop_frame=false;
    std::int64_t hours=0,minutes=0,seconds=0,pictures=0;
    std::int64_t temporal_reference=0,half_frame_adjustment=0;
};
struct MovieTimecodeRational {
    std::int64_t numerator=0,denominator=0;
    bool operator==(const MovieTimecodeRational&) const = default;
};
// Exact original813770 retail rate-code conversion, extended to checked native
// signed64 output. No reduction, PTS/fps substitution, playback clock or origin
// selection. Sequence extension N/D remain distinct metadata, not this table.
std::optional<MovieTimecodeRational> convert_movie_timecode(
    const MovieTimecodeDescriptor&,MovieTimecodeFailure&) noexcept;
// Source80AB38 maps GOP H/M/S/pictures +picture temporal_ref; it explicitly sets
// descriptor+1E half-frame adjustment0. A missing GOP is not a zero timecode.
std::optional<MovieTimecodeRational> convert_movie_picture_timecode(
    const Mpeg2PictureTiming&,MovieTimecodeFailure&) noexcept;
// Original80A608 position route. Caller supplies explicit quantized PES timestamp
// (raw33-bit PTS >>2 in the serialized domain); wide native values are allowed.
// No timestamp inheritance, playback clock, source origin or GOP substitution.
std::optional<MovieTimecodeDescriptor> convert_movie_position_timecode(
    std::optional<std::uint64_t> timestamp22500,std::uint8_t frame_rate_code,
    bool drop_frame,std::uint16_t temporal_reference,MovieTimecodeFailure&) noexcept;
}
