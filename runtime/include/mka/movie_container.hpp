#pragma once
#include "mka/movie_assets.hpp"
#include <array>
#include <optional>
#include <vector>
namespace mka::video {
enum class MovieContainerError {none,asset_io,cursor_restore,truncated,bad_prefix,
    bad_pack_marker,missing_pack_header,bad_system_header,unsupported_pack,unsupported_packet,unsupported_zero_length,
    extent_overflow,extent_out_of_bounds,missing_program_end,non_ff_tail,allocation,host_capacity};
struct MovieContainerFailure {
    MovieContainerError kind=MovieContainerError::none;
    std::uint64_t offset=0;
    MovieAssetFailure asset;
    MovieContainerError before_restore=MovieContainerError::none;
};
struct MoviePrivate2Prefix {
    std::uint64_t file_offset=0;
    // Actual validated adjacent MPEG1 pack[12] and BF packet prefix[6].
    // Retained serialized data for header classification/forwarding, no device.
    std::array<std::uint8_t,18> bytes{};
};
struct MoviePrivate2Payload {
    std::uint64_t packet_offset=0,payload_offset=0;
    std::vector<std::uint8_t> bytes;
    // Present only when this BF immediately followed a validated MPEG1 pack.
    // Never synthesized for a packet following other PES/system/padding data.
    std::optional<MoviePrivate2Prefix> adjacent_mpeg1_prefix;
};
struct NativeMovieContainer {
    std::uint64_t file_bytes=0,packet_count=0,pack_count=0;
    std::uint64_t program_end_offset=0,trailing_ff_bytes=0;
    std::vector<MoviePrivate2Payload> private2; // all BF packets, original order
};
// Pure checked serialized packet extent (4-byte code + BE16 length + body).
// The uint16 field is format syntax, not a total native resource limit.
std::optional<std::uint64_t> movie_container_packet_extent(std::uint64_t packet_offset,
    std::uint16_t payload_bytes,std::uint64_t file_bytes,MovieContainerFailure&)noexcept;
// Borrows exclusive cursor access, scans bytezero, restores original cursor on
// both success/failure. Returns owning byte results, no tag interpretation.
// MPEG1 bounded PS subset; MPEG2/unknown/unbounded PES reject by name.
std::optional<NativeMovieContainer> extract_movie_private2(NativeMovieAsset&,MovieContainerFailure&);
} // namespace mka::video
