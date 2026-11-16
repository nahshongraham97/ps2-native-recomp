#pragma once

// The native MPEG-1/2 video decoder.
//
// The CRI ADX movie the game plays is an MPEG-2 program stream, and the
// guest ADX module decoded it with the PS2's IPU (the coefficients and the
// inverse DCT) and its RSP (the slice layer, the motion compensation and the
// deblocking). Both are emulated PS2 devices, so the decode is done here
// instead: this file reads an elementary stream and produces the pictures.
//
// The decoder follows ISO/IEC 13818-2. The intra pictures are decoded from
// the coefficients; the predicted pictures need the motion compensation.

#include <cstdint>
#include <vector>

namespace ps2::media::mpeg2 {

// A decoded picture: the three planes as 8-bit samples.
struct Picture {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> luma;      // width * height
    std::vector<std::uint8_t> chroma_b;   // (width/2) * (height/2)
    std::vector<std::uint8_t> chroma_r;   // (width/2) * (height/2)
};

// The decoder's state: the picture size, the quantiser matrices and the
// previous picture (the predicted pictures are predicted from it).
class VideoDecoder {
public:
    void configure(std::uint32_t width, std::uint32_t height);

    // Decodes the next picture out of an elementary stream. 'at' is the
    // offset the decoder starts at and is moved past the picture it decoded.
    // Returns false when the stream holds no further picture or when the
    // data does not decode.
    bool decode(const std::uint8_t *data, std::size_t size, std::size_t &at, Picture &out);

    // The pictures decoded so far, and the last error (for the log).
    std::uint64_t pictures() const { return pictures_; }
    std::uint64_t macroblocks() const { return macroblocks_; }
    const char *error() const { return error_; }

private:
    std::uint32_t width_ = 0, height_ = 0;
    std::uint64_t pictures_ = 0;
    std::uint64_t macroblocks_ = 0;
    const char *error_ = nullptr;
    std::uint8_t intra_matrix_[64]{};
    std::uint8_t nonintra_matrix_[64]{};
    Picture previous_;
    bool have_previous_ = false;
};

}  // namespace ps2::media::mpeg2
