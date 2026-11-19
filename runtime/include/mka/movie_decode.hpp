#pragma once
#include "mka/movie_assets.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace mka::video {
struct MovieTimestamp {
    std::int64_t value=0;
    std::int32_t numerator=0,denominator=0;
    bool present=false;
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
};
struct MovieAudioFrame {
    std::uint32_t sample_rate=0;
    // Channel count is the vector size, with no console voice/core limit.
    std::vector<std::vector<std::int16_t>> channels;
    MovieTimestamp source_pts;
    MovieTimestamp timestamp;
};
using MovieDecodedFrame=std::variant<MovieVideoFrame,MovieAudioFrame>;
enum class MovieDecodeError {none,unsupported_host,library_load,missing_symbol,
    library_version,file_io,allocation,invalid_stream,unsupported_stream,
    unsupported_frame,demux,decode,invalid_owner,failed_owner};
struct MovieDecodeFailure {
    MovieDecodeError kind=MovieDecodeError::none;
    std::int32_t native_code=0;
    std::string operation;
    MovieAssetFailure asset_failure;
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
