#pragma once
#include "mka/movie_container.hpp"
#include <memory>
#include <span>
namespace mka::video {
enum class MovieMetadataError { none, invalid_packet_extent, packet_order,
    missing_header_prefix, invalid_header_prefix, malformed_version,
    version_overflow, truncated_tag_header, invalid_tag_length,
    tag_body_out_of_bounds, allocation_failed, host_capacity };
struct MovieMetadataFailure {
    MovieMetadataError kind=MovieMetadataError::none;
    std::size_t packet_index=0;
    std::uint64_t stream_offset=0;
};
const char* movie_metadata_error_name(MovieMetadataError) noexcept;
enum class MovieMetadataSelection { invalid_owner, empty, no_root, end_before_root, root_found };
struct MovieMetadataOrigin {
    std::size_t packet_index;
    std::uint64_t payload_file_offset,stream_offset,bytes;
};
struct MovieSofdecHeader {
    std::size_t packet_index;
    std::uint64_t file_offset,version_major,version_minor,version_code;
    std::vector<std::uint8_t> full_bytes;
    // callback_intended means the source's CONDITIONAL callback opportunity;
    // it does not assert a callback was registered. No callback/delivery runs.
    bool callback_intended,first_header_forward_private_index0;
    std::uint64_t source_first_cache_bytes;
};
struct MovieMetadataStorage;
class NativeMovieMetadata {
public:
    NativeMovieMetadata(const NativeMovieMetadata&)=default;
    NativeMovieMetadata& operator=(const NativeMovieMetadata&)=default;
    NativeMovieMetadata(NativeMovieMetadata&&) noexcept=default;
    NativeMovieMetadata& operator=(NativeMovieMetadata&&) noexcept=default;
    // Explicit ordered native Container records and source session latch input.
    // No first-packet assumption, player callback execution, track inference or
    // parse-ready signal. All ordinary bytes and classified headers are owned.
    static std::optional<NativeMovieMetadata> assemble(
        std::vector<MoviePrivate2Payload> owned_packets,
        bool initial_first_header_latched,MovieMetadataFailure&);
    bool valid() const noexcept { return bool(storage_); }
    bool next_first_header_latched() const noexcept;
    MovieMetadataSelection selection() const noexcept;
    std::span<const std::uint8_t> ordinary_stream() const noexcept;
    std::span<const MovieMetadataOrigin> origins() const noexcept;
    std::span<const MovieSofdecHeader> sofdec_headers() const noexcept;
    // Complete selected TLV header+body; backing remains owned by this object
    // or its copies. Other stream bytes are retained, never silently dropped.
    std::span<const std::uint8_t> selected_root_bytes() const noexcept;
private:
    explicit NativeMovieMetadata(std::shared_ptr<const MovieMetadataStorage> p):storage_(std::move(p)){}
    std::shared_ptr<const MovieMetadataStorage> storage_;
};
} // namespace mka::video
