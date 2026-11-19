#pragma once
#include "mka/movie_decode.hpp"
#include <optional>

namespace mka::video {
enum class MovieColorRole {unspecified,helper_default,fresh_preset21};
enum class MovieColorError {none,missing_table_role,invalid_table_bytes,table_identity_mismatch,verification_unavailable,invalid_frame,host_capacity};
struct MovieRgbaFrame {
    std::uint32_t width=0,height=0;
    std::vector<std::uint8_t> rgba;
    MovieTimestamp source_pts;
    MovieTimestamp timestamp;
    std::int32_t source_sample_aspect_numerator=0,source_sample_aspect_denominator=0;
    std::int32_t source_color_range=0,source_color_space=0,source_color_primaries=0,source_color_transfer=0;
};
// Portable original retail lookup arithmetic, not a GS/IPU/chip model.
// Selection is mandatory: the two original tables are materially different.
class NativeMovieColorTable {
public:
    static std::optional<NativeMovieColorTable> decode(MovieColorRole,const std::uint8_t*,std::size_t,MovieColorError&);
    std::array<std::uint8_t,4> pixel(std::uint8_t y,std::uint8_t u,std::uint8_t v)const noexcept;
    MovieColorRole role()const noexcept{return role_;}
private:
    NativeMovieColorTable()=default;
    MovieColorRole role_=MovieColorRole::unspecified;
    std::array<std::int16_t,3072> values_{};
};
std::optional<MovieRgbaFrame> convert_movie_yuv420(const MovieVideoFrame&,const NativeMovieColorTable&,MovieColorError&);
} // namespace mka::video
