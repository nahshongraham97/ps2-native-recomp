#pragma once
#include "mka/movie_assets.hpp"
#include <mutex>
#include <span>
namespace mka::video {
enum class MoviePesError {none,io,cursor_restore,truncated,bad_prefix,missing_pack,bad_pack,bad_system_header,unsupported_packet,unsupported_header,unsupported_zero_length,bad_timestamp,missing_end,bad_tail,overflow,allocation,invalid_stream,range,byte_mismatch,bad_picture_prefix,moved};
struct MoviePesFailure {MoviePesError kind=MoviePesError::none; std::uint64_t offset=0; MovieAssetFailure asset; MoviePesError before_restore=MoviePesError::none;};
// Legacy type name; identical checked PES span fields also describe audio.
struct MovieVideoPesSpan {
    std::uint8_t stream_id=0;
    std::uint64_t packet_offset=0,payload_offset=0,packet_end=0,es_begin=0,es_end=0;
    std::optional<std::uint64_t> pts33,dts33;
    bool operator==(const MovieVideoPesSpan&) const = default;
    std::optional<std::uint64_t> quarter_pts()const noexcept {return pts33 ? std::optional<std::uint64_t>(*pts33>>2):std::nullopt;}
};
struct MoviePesStream {std::uint8_t stream_id=0;std::uint64_t bytes=0;std::vector<MovieVideoPesSpan> spans;};
// A grammar-validated top-level program-end code, not decoder/client completion.
// The observed four bytes and verified trailing FF padding belong to this file.
struct MovieProgramEndObservation {
    std::uint64_t file_offset=0,byte_extent=4,trailing_padding_bytes=0;
    bool operator==(const MovieProgramEndObservation&) const = default;
};
struct MoviePesBacking;
class MovieEsReader;
class MovieSourceCursor;
class NativeMoviePesIndex {
public:
    // Takes the actual opened asset. No reopen/path matching or decoder policy.
    static std::optional<NativeMoviePesIndex> build(NativeMovieAsset&&,MoviePesFailure&);
    // Existing enumeration remains VIDEO-only (E0..EF).
    std::vector<MoviePesStream> streams()const;
    // Audio C0..DF payloads share the span layout/reader ownership semantics.
    std::vector<MoviePesStream> audio_streams()const;
    // Owned optional value; absent means no admitted B9, not an invented EOF event.
    // Moved-from index returns no observation. Copies survive backing teardown.
    std::optional<MovieProgramEndObservation> program_end()const noexcept;
    std::optional<MovieVideoPesSpan> lookup(std::uint8_t stream,std::uint64_t first_byte,MoviePesFailure&)const;
    // Verifies all four 00 00 01 00 bytes, including a cross-PES prefix;
    // selects only the FIRST byte's span, preserving absent timestamps.
    std::optional<MovieVideoPesSpan> lookup_picture_prefix(std::uint8_t,std::uint64_t,MoviePesFailure&)const;
    // Output is published only on success; backing cursor is restored.
    bool read_range(std::uint8_t stream,std::uint64_t begin,std::span<std::uint8_t> out,MoviePesFailure&)const;
    // Independent file cursor on the SAME retained opened asset; no reopen.
    std::optional<MovieSourceCursor> file_reader(MoviePesFailure&)const;
    std::optional<MovieEsReader> reader(std::uint8_t stream,MoviePesFailure&)const;
private:
    explicit NativeMoviePesIndex(std::shared_ptr<MoviePesBacking> p):state_(std::move(p)){}
    std::shared_ptr<MoviePesBacking> state_;
};
class MovieSourceCursor {
public:
    content::FileRead read(void*,std::size_t,MoviePesFailure&);
    bool seek(std::int64_t,content::SeekOrigin,MoviePesFailure&);
    std::optional<std::uint64_t> length(MoviePesFailure&)const;
    std::uint64_t position()const noexcept{return position_;}
private:
    explicit MovieSourceCursor(std::shared_ptr<MoviePesBacking> p):state_(std::move(p)){}
    std::shared_ptr<MoviePesBacking> state_;std::uint64_t position_=0;
    friend class NativeMoviePesIndex;
};
class MovieEsReader {
public:
    // Exclusive access to this reader is required. Shared backing IO is serialized.
    // Exact sequential byte validation only. No content search or silent skips.
    bool validate_next(std::span<const std::uint8_t>,MoviePesFailure&);
    bool finished()const noexcept;
    std::uint64_t position()const noexcept{return position_;}
private:
    MovieEsReader(std::shared_ptr<MoviePesBacking> p,std::uint8_t s):state_(std::move(p)),stream_(s){}
    std::shared_ptr<MoviePesBacking> state_;std::uint8_t stream_;std::uint64_t position_=0;
    friend class NativeMoviePesIndex;
};
}
