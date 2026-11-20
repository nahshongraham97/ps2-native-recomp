#pragma once
#include "mka/movie_header_unit.hpp"
#include "mka/movie_source_observations.hpp"
#include <string_view>
#include <utility>
namespace mka::video {
enum class MovieSourceSyntaxError {none,invalid_source,invalid_stream,invalid_owner,foreign_source,range,
    source_io,host_capacity,allocation,need_more,unsupported,malformed,invalid_order,
    missing_sequence,missing_sequence_extension,missing_coding_extension,missing_slice,missing_picture};
struct MovieSourceSyntaxFailure {
    MovieSourceSyntaxError kind=MovieSourceSyntaxError::none;
    std::uint64_t es_byte=0;
    MoviePesFailure pes;
    MovieHeaderUnitFailure unit;
};
struct MovieSourceSyntaxUnit {std::uint64_t es_byte=0;MovieHeaderUnit unit;};
struct MovieSourceSyntaxPicture {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    std::uint64_t picture_es_byte=0,sequence_es_byte=0,sequence_extension_es_byte=0,coding_extension_es_byte=0;
    MovieVideoPesSpan pes;
    std::optional<std::uint64_t> quarter_pts;
    MovieSequenceHeader sequence;
    MovieSequenceExtension sequence_extension;
    std::optional<MovieDisplayExtension> display_extension;
    std::optional<MovieGopHeader> gop;
    std::optional<std::uint64_t> gop_es_byte;
    MoviePictureHeader picture;
    MoviePictureCodingExtension coding;
    // Shared flat arena, half-open range since the current sequence. No cycle.
    std::shared_ptr<const std::vector<MovieSourceSyntaxUnit>> quant_matrix_units;
    std::size_t quant_history_begin=0,quant_history_end=0;
    std::vector<MovieSourceSyntaxUnit> userdata;
};
struct MovieSourceSyntaxState;
class NativeMovieSourceSyntaxIndex {
public:
    static std::optional<NativeMovieSourceSyntaxIndex> build(const NativeMoviePesIndex&,
        std::uint8_t video_stream,std::string source_key,
        std::shared_ptr<const MovieSourceObservationIdentity>,MovieSourceSyntaxFailure&) noexcept;
    NativeMovieSourceSyntaxIndex(const NativeMovieSourceSyntaxIndex&)=delete;
    NativeMovieSourceSyntaxIndex& operator=(const NativeMovieSourceSyntaxIndex&)=delete;
    NativeMovieSourceSyntaxIndex(NativeMovieSourceSyntaxIndex&&) noexcept=default;
    NativeMovieSourceSyntaxIndex& operator=(NativeMovieSourceSyntaxIndex&& other) noexcept {
        if(this!=&other)state_=std::move(other.state_);return *this;
    }
    std::shared_ptr<const MovieSourceSyntaxPicture> lookup(std::string_view,
        const std::shared_ptr<const MovieSourceObservationIdentity>&,std::uint64_t picture_es_byte,
        MovieSourceSyntaxFailure&) const noexcept;
    // Borrowed view; copy an individual shared_ptr for independent lifetime.
    std::span<const std::shared_ptr<const MovieSourceSyntaxPicture>> pictures() const noexcept;
private:
    explicit NativeMovieSourceSyntaxIndex(std::shared_ptr<const MovieSourceSyntaxState> p):state_(std::move(p)){}
    std::shared_ptr<const MovieSourceSyntaxState> state_;
};
} // namespace mka::video
