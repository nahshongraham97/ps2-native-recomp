// The CRI ADX movie is an MPEG-2 program stream. This test parses the real
// movie the game plays and states what the native decoder has to handle:
// the picture size, the frame rate, the profile, and the streams the file
// carries (the video and CRI's private audio stream).

#include "media/mpeg2.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

// The movie the game plays, found by walking up from the working directory.
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

}  // namespace

int main() {
    const std::filesystem::path movie = find_movie();
    if (movie.empty()) {
        std::cerr << "the movie file is not here\n";
        return 1;
    }
    std::ifstream file(movie, std::ios::binary);
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        std::cerr << "the movie file is empty\n";
        return 1;
    }

    ps2::media::mpeg2::ProgramStream stream;
    const bool parsed = ps2::media::mpeg2::parse_program_stream(data.data(), data.size(), stream);

    std::size_t video_packets = 0, private_packets = 0, pictures = 0, intra = 0, predicted = 0;
    for (const ps2::media::mpeg2::Stream &elementary : stream.streams) {
        if (elementary.stream_id == 0xC0) private_packets += elementary.packets.size();  // the audio stream
        else video_packets += elementary.packets.size();
        for (const ps2::media::mpeg2::Stream::Picture &picture : elementary.pictures) {
            ++pictures;
            if (picture.coding_type == 1) ++intra;
            if (picture.coding_type == 2) ++predicted;
        }
    }

    std::cout << "parsed: " << (parsed ? "yes" : "no") << "\n";
    std::cout << "picture " << stream.width << "x" << stream.height
              << "  frame rate " << ps2::media::mpeg2::frame_rate(stream.frame_rate_code)
              << " fps  profile " << static_cast<unsigned>(stream.profile)
              << "  progressive " << (stream.progressive ? "yes" : "no") << "\n";
    std::cout << "video packets " << video_packets << "  private-stream-1 packets " << private_packets
              << "  pictures " << pictures << " (I " << intra << ", P " << predicted << ")\n";
    for (const ps2::media::mpeg2::Stream &elementary : stream.streams)
        std::cout << "  stream 0x" << std::hex << static_cast<unsigned>(elementary.stream_id) << std::dec
                  << ": " << elementary.packets.size() << " packets, " << elementary.pictures.size() << " pictures\n";
    if (!parsed) {
        std::cerr << "the movie is not a program stream with a video sequence\n";
        return 1;
    }

    bool ok = stream.width == 640 && stream.height == 448;
    ok = ok && stream.frame_rate_code == 5;          // 30 fps
    ok = ok && video_packets > 100 && private_packets > 100;   // the video and the audio
    ok = ok && pictures > 100 && intra > 0 && predicted > 0;
    if (!ok) std::cerr << "the movie's parameters are not the ones the decoder is written for\n";
    return ok ? 0 : 1;
}
