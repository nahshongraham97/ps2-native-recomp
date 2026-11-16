#include "media/mpeg2.hpp"

#include <algorithm>
#include <cstring>

namespace ps2::media::mpeg2 {

namespace {

// The start codes the program stream uses.
constexpr std::uint8_t kPackHeader = 0xBA;
constexpr std::uint8_t kSystemHeader = 0xBB;
constexpr std::uint8_t kProgramStreamMap = 0xBC;
constexpr std::uint8_t kPrivateStream1 = 0xBD;
constexpr std::uint8_t kPrivateStream2 = 0xBF;   // CRI keeps its metadata and its audio here
constexpr std::uint8_t kPaddingStream = 0xBE;
constexpr std::uint8_t kEndCode = 0xB9;

// The video elementary stream's start codes.
constexpr std::uint8_t kSequenceHeader = 0xB3;
constexpr std::uint8_t kExtensionStart = 0xB5;
constexpr std::uint8_t kSequenceEnd = 0xB7;
constexpr std::uint8_t kGopHeader = 0xB8;  // the group of pictures header
constexpr std::uint8_t kPictureStart = 0x00;   // 0x00-0x1F
constexpr std::uint8_t kPictureEnd = 0x1F;

std::uint32_t read_be32(const std::uint8_t *p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

std::uint64_t read_be64(const std::uint8_t *p) {
    return (static_cast<std::uint64_t>(read_be32(p)) << 32) | read_be32(p + 4);
}

// The next start code in the buffer, or the size when there is none.
std::size_t find_start_code(const std::uint8_t *data, std::size_t size, std::size_t from) {
    for (std::size_t at = from; at + 4 <= size; ++at) {
        if (data[at] == 0x00 && data[at + 1] == 0x00 && data[at + 2] == 0x01) return at;
    }
    return size;
}

// Whether a start code belongs to the program stream's own layer (a pack, a
// system header or a PES packet) rather than to an elementary stream.
bool is_program_stream_code(std::uint8_t code) {
    if (code >= 0xE0 && code <= 0xEF) return true;   // a video PES packet
    if (code >= 0xC0 && code <= 0xDF) return true;   // an audio PES packet
    if (code >= 0xF0 && code <= 0xFF) return true;   // a private PES packet
    return code == kPrivateStream1 || code == kPrivateStream2 || code == kPaddingStream ||
           code == kEndCode || code == kSystemHeader || code == kProgramStreamMap || code == kPackHeader;
}

// The presentation time stamp of an MPEG-2 PES header.
bool read_pts(const std::uint8_t *p, std::uint64_t &pts) {
    if ((p[0] & 0x20) == 0) return false;             // the 'PTS' flag
    pts = (static_cast<std::uint64_t>((p[0] >> 1) & 0x07) << 30) |
          (static_cast<std::uint64_t>(p[1]) << 22) |
          (static_cast<std::uint64_t>((p[1] >> 1) & 0x7F) << 15) |
          (static_cast<std::uint64_t>(p[2]) << 7) |
          (static_cast<std::uint64_t>((p[3] >> 1) & 0x7F));
    return true;
}

// Reads the sequence header of a video elementary stream.
bool read_sequence_header(const std::uint8_t *p, std::size_t size, ProgramStream &out) {
    if (size < 12) return false;
    out.width = (static_cast<std::uint32_t>(p[0]) << 4) | (p[1] >> 4);
    out.height = (static_cast<std::uint32_t>(p[1] & 0x0F) << 8) | p[2];
    out.frame_rate_code = p[3] & 0x0F;
    return out.width > 0 && out.height > 0;
}

// The sequence extension carries the profile and the progressive flag.
bool read_sequence_extension(const std::uint8_t *p, std::size_t size, ProgramStream &out) {
    if (size < 4) return false;
    out.profile = p[0] >> 4;
    out.progressive = (p[1] & 0x80) != 0;
    return true;
}

// Walks a video elementary stream and records its pictures. The pictures are
// the picture headers, each followed by its slices up to the next picture.
void read_pictures(const std::uint8_t *data, std::size_t size, std::size_t from, ProgramStream &out) {
    Stream *video = nullptr;
    for (const Stream &stream : out.streams) {
        if (stream.stream_id >= 0xE0 && stream.stream_id <= 0xEF) {
            video = const_cast<Stream *>(&stream);
            break;
        }
    }
    if (!video) return;
    std::size_t at = from;
    while (at + 4 <= size) {
        const std::size_t code_at = find_start_code(data, size, at);
        if (code_at + 4 > size) break;
        const std::uint8_t code = data[code_at + 3];
        const std::uint8_t *body = data + code_at + 4;
        const std::size_t body_size = size - (code_at + 4);
        if (code == kSequenceHeader) {
            read_sequence_header(body, body_size, out);
        } else if (code == kExtensionStart) {
            read_sequence_extension(body, body_size, out);
        } else if (code >= kPictureStart && code <= kPictureEnd) {
            if (body_size < 6) break;
            Stream::Picture picture;
            picture.coding_type = (body[1] >> 3) & 0x07;
            picture.payload_at = static_cast<std::uint32_t>(code_at + 4);
            const std::size_t next = find_start_code(data, size, code_at + 4);
            picture.payload_size = static_cast<std::uint32_t>(next - (code_at + 4));
            video->pictures.push_back(picture);
        } else if (code == kSequenceEnd || is_program_stream_code(code)) {
            break;
        }
        at = code_at + 4;
    }
}

}  // namespace

double frame_rate(std::uint32_t code) {
    switch (code) {
    case 1: return 24000.0 / 1001.0;
    case 2: return 24.0;
    case 3: return 25.0;
    case 4: return 30000.0 / 1001.0;
    case 5: return 30.0;
    case 6: return 50.0;
    case 7: return 60000.0 / 1001.0;
    case 8: return 60.0;
    default: return 0.0;
    }
}

bool parse_program_stream(const std::uint8_t *data, std::size_t size, ProgramStream &out) {
    if (!data || size < 4) return false;
    if (data[0] != 0x00 || data[1] != 0x00 || data[2] != 0x01 || data[3] != kPackHeader) return false;

    std::size_t at = 0;
    while (at + 4 <= size) {
        const std::size_t code_at = find_start_code(data, size, at);
        if (code_at + 4 > size) break;
        const std::uint8_t code = data[code_at + 3];
        const std::uint8_t *body = data + code_at + 4;
        const std::size_t body_size = size - (code_at + 4);

        if (code == kPackHeader) {
            // The pack header: the system clock reference and the multiplex
            // rate, then the stuffing and the optional extension. The MPEG-2
            // form carries ten bytes after the start code and the MPEG-1 form
            // eight; the marker bits in the first byte tell them apart.
            const bool mpeg2_pack = body_size > 0 && (body[0] & 0xC0) == 0x40;
            std::size_t next = code_at + 4 + (mpeg2_pack ? 10u : 8u);
            while (next < size && data[next] == 0xFF) ++next;
            if (next < size && (data[next] & 0xC0) == 0x40) next += 1 + data[next];
            at = next;
            continue;
        }
        if (code == kSystemHeader || code == kProgramStreamMap) {
            const std::size_t header = (static_cast<std::size_t>(body[0]) << 8) | body[1];
            at = code_at + 4 + 2 + header;
            continue;
        }
        if (code == kPaddingStream || code == kEndCode) {
            at = code_at + 4;
            continue;
        }
        if (is_program_stream_code(code)) {
            // A PES packet: the packet length, the flags, the optional
            // headers, then the payload.
            if (body_size < 4) break;
            // The packet length is a hint only: this muxer's lengths run past
            // the pack they sit in, so the payload really ends at the next
            // program stream start code.
            const std::size_t length = (static_cast<std::size_t>(body[0]) << 8) | body[1];
            (void)length;
            std::size_t payload_at = 0;
            std::size_t payload_end = 0;
            std::uint64_t pts = 0;
            bool has_pts = false;
            if (body[1] & 0x80) {
                // An MPEG-2 PES header: the flags byte says what the optional
                // header holds, and the header length says how long it is.
                const std::size_t header = body[2];
                const std::size_t optional = header > 3 ? (body[3] >> 4) & 0x03 : 0;
                if (optional == 2 && header >= 5) has_pts = read_pts(body + 4, pts);
                if (optional == 3 && header >= 10) {
                    read_pts(body + 4, pts);
                    has_pts = true;
                }
                payload_at = code_at + 4 + 3 + header;
                payload_end = size;
            } else {
                // An MPEG-1 PES header: the flags byte and the PTS flags byte
                // say whether a time stamp follows.
                const std::uint8_t pts_flags = body[3];
                const std::size_t kind = (pts_flags >> 6) & 0x03;
                const std::size_t optional = kind == 2 ? 5 : kind == 3 ? 10 : 0;
                if (optional >= 5) has_pts = read_pts(body + 4, pts);
                payload_at = code_at + 4 + 4 + optional;
                // The packet length is a hint: this muxer's lengths run past
                // the pack they sit in, so the payload really ends at the next
                // program stream start code.
                payload_end = size;
            }
            // The payload runs to the next PES packet.
            std::size_t next = payload_at;
            while (next + 4 <= size && next < payload_end) {
                const std::size_t candidate = find_start_code(data, size, next);
                if (candidate + 4 > size || candidate >= payload_end) {
                    next = payload_end;
                    break;
                }
                if (is_program_stream_code(data[candidate + 3])) {
                    next = candidate;
                    break;
                }
                next = candidate + 4;
            }
            PesPacket packet;
            packet.stream_id = code;
            packet.pts = pts;
            packet.has_pts = has_pts;
            packet.payload_at = static_cast<std::uint32_t>(payload_at);
            packet.payload_size = static_cast<std::uint32_t>(next - payload_at);
            Stream *stream = nullptr;
            for (Stream &candidate : out.streams) {
                if (candidate.stream_id == code) {
                    stream = &candidate;
                    break;
                }
            }
            if (!stream) {
                out.streams.push_back(Stream{});
                stream = &out.streams.back();
                stream->stream_id = code;
            }
            stream->packets.push_back(packet);
            // The video elementary stream inside the packet carries the
            // sequence header and the pictures.
            if (code >= 0xE0 && code <= 0xEF) read_pictures(data, next, payload_at, out);
            at = next;
            continue;
        }
        at = code_at + 4;
    }

    read_pictures(data, size, 0, out);
    // The sequence header is the first thing in the video elementary stream;
    // find it on its own so the picture size does not depend on the walk
    // above reaching the packet that carries it.
    if (out.width == 0 || out.height == 0) {
        for (std::size_t at = 0; at + 4 <= size; ++at) {
            if (data[at] != 0x00 || data[at + 1] != 0x00 || data[at + 2] != 0x01) continue;
            const std::uint8_t code = data[at + 3];
            if (code == kSequenceHeader) {
                read_sequence_header(data + at + 4, size - (at + 4), out);
                break;
            }
            if (code == kExtensionStart) {
                read_sequence_extension(data + at + 4, size - (at + 4), out);
            }
        }
    }
    for (const Stream &stream : out.streams) {
        if (stream.stream_id >= 0xE0 && stream.stream_id <= 0xEF) return out.width > 0 && out.height > 0;
    }
    return false;
}

}  // namespace ps2::media::mpeg2
