#pragma once

// The CRI ADX movie is an MPEG-2 program stream (ISO/IEC 13818-1): pack
// headers, a program stream map, and PES packets that carry the video
// elementary stream and CRI's private audio stream.
//
// The parser is the front half of the native ADX video player: it splits the
// file into the elementary streams and reads the sequence header, so the
// decoder gets the picture size, the frame rate and the profile it needs
// without walking the file again.

#include <cstdint>
#include <string>
#include <vector>

namespace ps2::media::mpeg2 {

// One PES packet's payload inside the program stream.
struct PesPacket {
    std::uint8_t stream_id = 0;      // 0xE0-0xEF: the video stream, 0xBD: CRI's private stream 1
    std::uint64_t pts = 0;           // the presentation time stamp, when the packet carries one
    bool has_pts = false;
    std::uint32_t payload_at = 0;    // the offset of the payload in the file
    std::uint32_t payload_size = 0;
};

// One elementary stream of the program stream.
struct Stream {
    std::uint8_t stream_id = 0;
    std::vector<PesPacket> packets;
    // The video elementary stream's picture headers, in file order: the
    // temporal reference, the coding type (1 = I, 2 = P, 3 = B) and the
    // picture's payload.
    struct Picture {
        std::uint64_t pts = 0;
        std::uint8_t coding_type = 0;
        std::uint32_t payload_at = 0;
        std::uint32_t payload_size = 0;
    };
    std::vector<Picture> pictures;
};

struct ProgramStream {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frame_rate_code = 0;   // the sequence header's frame rate code
    std::uint8_t profile = 0;            // the sequence header's profile (0 = simple)
    bool progressive = false;            // the sequence extension's progressive flag
    std::vector<Stream> streams;
};

// Splits a program stream into its elementary streams and reads the video
// sequence header. Returns false when the file is not a program stream or
// when it carries no video.
bool parse_program_stream(const std::uint8_t *data, std::size_t size, ProgramStream &out);

// The frame rate a sequence header's frame rate code stands for.
double frame_rate(std::uint32_t code);

}  // namespace ps2::media::mpeg2
