#pragma once
#include "mka/movie_assets.hpp"
#include "mka/movie_pes_index.hpp"
#include "mka/mpeg2_timing.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mka::video {
struct MovieTimestamp {
    std::int64_t value=0;
    std::int32_t numerator=0,denominator=0;
    bool present=false;
};
struct MovieRateRatio {
    std::int32_t numerator=0,denominator=0;
    bool operator==(const MovieRateRatio&) const = default;
};
struct MoviePictureSource {
    std::uint64_t packet_es_begin=0,picture_es_byte=0;
    MovieVideoPesSpan pes;
    std::optional<std::uint64_t> quarter_pts;
    bool operator==(const MoviePictureSource&) const = default;
};
struct MovieCodecTiming {
    // Exact library values, including 0/0 when unavailable. The demuxer's
    // r_frame_rate is an estimate; none of these selects a playback clock.
    MovieRateRatio codec_framerate,demux_avg_frame_rate,demux_r_frame_rate;
    // Owned raw MPEG 25-bit GOP timecode, emitted by the codec only on its
    // associated first output. Not a PTS, per-picture reference or origin.
    std::optional<std::uint64_t> gop_timecode;
    // Parsed elementary-picture metadata, carried through codec reorder by an
    // owned opaque tag. Distinct from GOP side-data, PTS and library rate hints.
    std::optional<Mpeg2PictureTiming> picture;
    // Exact original PES provenance; independent of library PTS/position hints.
    std::optional<MoviePictureSource> picture_source;
    bool operator==(const MovieCodecTiming&) const = default;
};
struct MovieVideoFrame {
    std::uint32_t width=0,height=0;
    // Canonical owned, tightly packed Y/U/V planes. Chroma dimensions are
    // ceil(width/2),ceil(height/2). No guest memory or macroblock transport.
    std::array<std::vector<std::uint8_t>,3> planes;
    MovieTimestamp source_pts;
    // Codec best-effort timestamp, retained separately from optional raw PTS.
    MovieTimestamp timestamp;
    std::int32_t sample_aspect_numerator=0,sample_aspect_denominator=0;
    std::int32_t color_range=0,color_space=0,color_primaries=0,color_transfer=0;
    MovieCodecTiming codec_timing;
};
enum class MovieAudioChannelOrder {unspecified,native,custom,ambisonic};
struct MovieAudioChannel {
    // Exact public AVChannel numeric ID; no decoder/private pointer survives.
    std::int32_t id=0;
    std::array<char,16> custom_name{};
    bool operator==(const MovieAudioChannel&) const = default;
};
struct MovieAudioChannelLayout {
    MovieAudioChannelOrder order=MovieAudioChannelOrder::unspecified;
    std::uint64_t channel_count=0;
    // NATIVE mask or AMBISONIC non-diegetic mask only; undefined for UNSPEC
    // and CUSTOM, so those orders retain explicit absence.
    std::optional<std::uint64_t> mask;
    // Exact plane-order IDs for known orders; empty for UNSPEC (count only).
    // AMBISONIC IDs retain ACN order and non-diegetic suffix. CUSTOM names keep
    // all16 public bytes. FFmpeg application-private opaque pointers omitted.
    std::vector<MovieAudioChannel> channels;
    bool operator==(const MovieAudioChannelLayout&) const = default;
};
struct MovieAudioFrame {
    MovieAudioChannelLayout channel_layout;
    std::uint32_t sample_rate=0;
    // Channel count is the vector size, with no console voice/core limit.
    std::vector<std::vector<std::int16_t>> channels;
    MovieTimestamp source_pts;
    MovieTimestamp timestamp;
};
using MovieDecodedFrame=std::variant<MovieVideoFrame,MovieAudioFrame>;
enum class MovieDecodeError {none,unsupported_host,library_load,missing_symbol,
    library_version,file_io,allocation,invalid_stream,unsupported_stream,
    unsupported_frame,unsupported_audio_layout,demux,decode,invalid_owner,failed_owner,timing_parse,timing_association,source_index,source_validation,source_association};
struct MovieDecodeFailure {
    MovieDecodeError kind=MovieDecodeError::none;
    std::int32_t native_code=0;
    std::string operation;
    MovieAssetFailure asset_failure;
    MoviePesFailure source_failure;
};
enum class MovieReadStatus {frame,end,error};
struct MovieReadResult {
    MovieReadStatus status=MovieReadStatus::error;
    MovieDecodedFrame frame;
};
// Native filesystem -> MPEG-PS demux -> ordinary MPEG2/ADX codec library.
// No CDVD, IPU, GS, IOP, SPU2 or device clock. This decoder does not invent
// an A/V clock, skip/fade lifecycle, pixel conversion, presentation or audio
// output. Those have separate source-proven contracts.
// Frames own their raw codec/header timing metadata as well as YUV/PCM. CRI
// private-stream subtitle tags use the separate native container/metadata
// parser; they are not emitted by this codec API.
class NativeMovieDecoder {
public:
    ~NativeMovieDecoder();
    NativeMovieDecoder(NativeMovieDecoder&&) noexcept;
    NativeMovieDecoder& operator=(NativeMovieDecoder&&) noexcept;
    NativeMovieDecoder(const NativeMovieDecoder&)=delete;
    NativeMovieDecoder& operator=(const NativeMovieDecoder&)=delete;
    static std::unique_ptr<NativeMovieDecoder> open(const std::filesystem::path& library_directory,
        const std::filesystem::path& asset,MovieDecodeFailure&);
    // Consumes the already-open catalog asset and decodes the complete movie
    // from byte zero, regardless of prior reads/seeks/EOF. Never reopens its path.
    static std::unique_ptr<NativeMovieDecoder> open(const std::filesystem::path& library_directory,
        NativeMovieAsset&& asset,MovieDecodeFailure&);
    MovieReadResult next(MovieDecodeFailure&);
private:
    struct Impl;
    explicit NativeMovieDecoder(std::unique_ptr<Impl>);
    static std::unique_ptr<NativeMovieDecoder> finish_open(std::unique_ptr<Impl>,MovieDecodeFailure&);
    std::unique_ptr<Impl> impl_;
};
} // namespace mka::video
