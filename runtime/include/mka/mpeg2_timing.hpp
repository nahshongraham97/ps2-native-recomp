#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace mka::video {
enum class Mpeg2TimingError {
    none, invalid_start_code, truncated_header, invalid_marker, invalid_sequence,
    unsupported_rate, unsupported_extension, unsupported_start_code,
    missing_sequence, missing_sequence_extension, duplicate_extension,
    missing_picture, multiple_pictures, unsupported_field_picture,
    invalid_picture, missing_picture_extension, missing_slices,
    unsupported_composite_display, picture_in_extradata
};
struct Mpeg2TimingFailure { Mpeg2TimingError kind=Mpeg2TimingError::none; std::size_t offset=0; };
const char* mpeg2_timing_error_name(Mpeg2TimingError) noexcept;
struct Mpeg2SequenceTiming {
    std::uint8_t frame_rate_code=0, extension_n=0, extension_d=0;
    bool progressive_sequence=false;
    // extension_n/d are serialized fields; actual multipliers are n+1,d+1.
    std::uint16_t width=0,height=0;
    std::uint8_t chroma_format=0;
    bool operator==(const Mpeg2SequenceTiming&) const = default;
};
struct Mpeg2GopTiming {
    std::uint32_t raw_time_code=0; // exact25 serialized bits, including marker
    std::uint8_t hours=0,minutes=0,seconds=0,pictures=0;
    bool drop_frame=false,closed_gop=false,broken_link=false;
    bool operator==(const Mpeg2GopTiming&) const = default;
};
struct Mpeg2PictureTiming {
    Mpeg2SequenceTiming sequence;
    std::optional<Mpeg2GopTiming> gop; // absence is explicit, never zero timecode
    std::uint16_t temporal_reference=0;
    std::uint8_t coding_type=0,picture_structure=0;
    bool top_field_first=false,repeat_first_field=false,progressive_frame=false;
    bool operator==(const Mpeg2PictureTiming&) const = default;
};
// Elementary codec packet headers only. Not PES, an entropy decoder or timeline
// conversion. Each call requires complete header units (no fragment buffering).
// State and output commit together only on success. Owned scalar records survive
// input destruction. Sequence/GOP inheritance follows explicit serialized data.
class NativeMpeg2Timing {
public:
    // Prime with actual codec extradata, never guessed sequence defaults. A
    // complete sequence+sequence-extension may instead arrive in a packet.
    bool prime(std::span<const std::uint8_t>,Mpeg2TimingFailure&) noexcept;
    bool parse(std::span<const std::uint8_t>,std::optional<Mpeg2PictureTiming>&,
               Mpeg2TimingFailure&) noexcept;
    void reset() noexcept { *this=NativeMpeg2Timing{}; }
private:
    bool sequence_header_=false,sequence_extension_=false;
    Mpeg2SequenceTiming sequence_;
    std::optional<Mpeg2GopTiming> gop_;
};
}
