// The native MPEG-2 decoder against the game's own movie: the first intra
// picture has to decode, and the picture it produces has to look like a
// picture (the movie's title frame), not like noise.

#include "media/mpeg2.hpp"
#include "media/mpeg2_decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

std::filesystem::path find_movie() {
    const char *const relative = "games/mkarmageddon/disc-files/MOVIEPS2/LOGON.SFD";
    std::filesystem::path here = std::filesystem::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        const std::filesystem::path candidate = here / relative;
        if (std::filesystem::exists(candidate)) return candidate;
        if (!here.has_parent_path()) break;
        here = here.parent_path();
    }
    return {};
}

void write_ppm(const std::string &path, const ps2::media::mpeg2::Picture &picture) {
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << picture.width << " " << picture.height << "\n255\n";
    for (std::uint32_t y = 0; y < picture.height; ++y) {
        for (std::uint32_t x = 0; x < picture.width; ++x) {
            const std::size_t at = static_cast<std::size_t>(y) * picture.width + x;
            const int lum = picture.luma[at];
            const std::size_t at_chroma = static_cast<std::size_t>(y / 2) * (picture.width / 2) + x / 2;
            const int cb = picture.chroma_b[at_chroma] - 128;
            const int cr = picture.chroma_r[at_chroma] - 128;
            const int r = std::clamp((lum * 256 + 204 * cr + 128) >> 8, 0, 255);
            const int g = std::clamp((lum * 256 - 50 * cb - 104 * cr + 128) >> 8, 0, 255);
            const int b = std::clamp((lum * 256 + 258 * cb + 128) >> 8, 0, 255);
            const char rgb[3] = {static_cast<char>(r), static_cast<char>(g), static_cast<char>(b)};
            file.write(rgb, 3);
        }
    }
}

}  // namespace

int main() {
    const std::filesystem::path movie = find_movie();
    if (movie.empty()) {
        std::cerr << "the movie file is not here\n";
        return 1;
    }
    std::ifstream file(movie, std::ios::binary);
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    ps2::media::mpeg2::ProgramStream stream;
    if (!ps2::media::mpeg2::parse_program_stream(data.data(), data.size(), stream)) {
        std::cerr << "the movie is not a program stream\n";
        return 1;
    }

    // The video elementary stream: the payloads of the video packets in order.
    std::vector<std::uint8_t> elementary;
    for (const ps2::media::mpeg2::Stream &s : stream.streams) {
        if (s.stream_id < 0xE0 || s.stream_id > 0xEF) continue;
        for (const ps2::media::mpeg2::PesPacket &packet : s.packets)
            elementary.insert(elementary.end(), data.begin() + packet.payload_at,
                               data.begin() + packet.payload_at + packet.payload_size);
    }
    std::cout << "the video elementary stream is " << elementary.size() << " bytes\n";

    ps2::media::mpeg2::VideoDecoder decoder;
    decoder.configure(stream.width, stream.height);
    std::size_t at = 0;
    ps2::media::mpeg2::Picture picture;
    if (!decoder.decode(elementary.data(), elementary.size(), at, picture)) {
        std::cerr << "the first picture did not decode: " << decoder.error() << " (macroblocks " << decoder.macroblocks() << ")\n";
        return 1;
    }

    // The picture has to hold a real image: the luma plane must take many
    // values, not one.
    bool seen[256] = {};
    std::size_t distinct = 0;
    for (std::uint8_t sample : picture.luma) {
        if (!seen[sample]) {
            seen[sample] = true;
            ++distinct;
        }
    }
    std::cout << "decoded " << picture.width << "x" << picture.height
              << ", " << distinct << " distinct luma values\n";

    const std::string dump = "mpeg2_first_picture.ppm";
    write_ppm(dump, picture);
    std::cout << "wrote " << dump << "\n";

    bool ok = picture.width == 640 && picture.height == 448;
    ok = ok && distinct > 32;   // a real picture, not a flat block
    if (!ok) std::cerr << "the decoded picture does not look like the movie's first frame\n";
    return ok ? 0 : 1;
}
