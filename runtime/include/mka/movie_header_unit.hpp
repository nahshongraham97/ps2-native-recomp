#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>
#include <vector>
namespace mka::video {
enum class MovieHeaderUnitStatus {parsed,need_more,unsupported,malformed,error};
enum class MovieHeaderUnitError {none,incomplete_prefix,incomplete_header,unterminated_userdata,
    invalid_prefix,unsupported_start_code,unsupported_extension,invalid_marker,invalid_value,allocation,host_capacity};
struct MovieHeaderUnitFailure {MovieHeaderUnitError kind=MovieHeaderUnitError::none;std::uint64_t byte_offset=0;};
struct MovieSequenceHeader {
    std::uint16_t width=0,height=0,vbv_buffer_size=0;
    std::uint8_t aspect_ratio_information=0,frame_rate_code=0;
    std::uint32_t bit_rate_value=0;bool constrained_parameters=false;
    std::optional<std::array<std::uint8_t,64>> intra_matrix,non_intra_matrix;
};
struct MovieSequenceExtension {
    std::uint8_t profile_and_level=0,chroma_format=0,horizontal_size_extension=0,vertical_size_extension=0;
    std::uint16_t bit_rate_extension=0;std::uint8_t vbv_buffer_size_extension=0;
    bool progressive_sequence=false,low_delay=false;std::uint8_t frame_rate_extension_n=0,frame_rate_extension_d=0;
};
struct MovieQuantMatrixExtension {
    // Serialized order, deliberately not decoder zigzag/permuted storage.
    std::optional<std::array<std::uint8_t,64>> intra_matrix,non_intra_matrix,
        chroma_intra_matrix,chroma_non_intra_matrix;
};
struct MovieDisplayColor {std::uint8_t primaries=0,transfer=0,matrix=0;};
struct MovieDisplayExtension {
    std::uint8_t video_format=0;std::optional<MovieDisplayColor> color;
    std::uint16_t horizontal_size=0,vertical_size=0;
};
struct MovieCompositeDisplay {
    bool v_axis=false,sub_carrier=false;std::uint8_t field_sequence=0,burst_amplitude=0,sub_carrier_phase=0;
};
struct MoviePictureCodingExtension {
    std::array<std::uint8_t,4> f_code{};std::uint8_t intra_dc_precision=0,picture_structure=0;
    bool top_field_first=false,frame_pred_frame_dct=false,concealment_motion_vectors=false,
        q_scale_type=false,intra_vlc_format=false,alternate_scan=false,repeat_first_field=false,
        chroma_420_type=false,progressive_frame=false;
    // Five actual serialized composite fields; no host display policy inferred.
    std::optional<MovieCompositeDisplay> composite_display;
};
struct MovieGopHeader {
    std::uint32_t raw_time_code=0;bool drop_frame=false,closed_gop=false,broken_link=false;
    std::uint8_t hours=0,minutes=0,seconds=0,pictures=0;
};
struct MoviePictureDirection {bool full_pel=false;std::uint8_t f_code=0;};
struct MoviePictureHeader {
    std::uint16_t temporal_reference=0,vbv_delay=0;std::uint8_t coding_type=0;
    std::optional<MoviePictureDirection> forward,backward;
    std::vector<std::uint8_t> extra_information;
};
struct MovieUserDataHeader {std::vector<std::uint8_t> payload;};
struct MovieSequenceEndHeader {};
using MovieHeaderUnitPayload=std::variant<MovieSequenceHeader,MovieSequenceExtension,MovieDisplayExtension,
    MoviePictureCodingExtension,MovieQuantMatrixExtension,MovieGopHeader,MoviePictureHeader,MovieUserDataHeader,MovieSequenceEndHeader>;
struct MovieHeaderUnit {std::uint64_t consumed_bytes=0;std::vector<std::uint8_t> bytes;MovieHeaderUnitPayload payload;};
struct MovieHeaderUnitResult {
    MovieHeaderUnitStatus status=MovieHeaderUnitStatus::error;MovieHeaderUnitFailure failure;
    std::optional<MovieHeaderUnit> unit;
};
// One borrowed available unit at offset zero. No owner advancement, batch
// admission, counter, callback, timestamp or end-of-source inference.
MovieHeaderUnitResult parse_movie_header_unit(std::span<const std::uint8_t>) noexcept;
} // namespace mka::video
