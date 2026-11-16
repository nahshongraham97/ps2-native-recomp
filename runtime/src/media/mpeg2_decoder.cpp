#include "media/mpeg2_decoder.hpp"

#include "media/vlc.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ps2::media::mpeg2 {

namespace {

using vlc::DctLookup;
using vlc::Lookup;

// The default quantiser matrices of the standard (ISO/IEC 13818-2, the
// "default_intra_quantiser_matrix" and "default_non_intra_quantiser_matrix").
constexpr std::uint8_t kDefaultIntra[64] = {
    8,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34, 34, 38, 22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58, 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83,
};

constexpr std::uint8_t kDefaultNonIntra[64] = {
    16, 17, 18, 19, 20, 21, 22, 23, 18, 19, 20, 21, 22, 23, 24, 25, 19, 20, 21, 22, 23, 24, 25, 27, 20, 21, 22, 23, 25, 26, 27, 28,
    21, 22, 23, 24, 26, 27, 28, 30, 22, 23, 24, 26, 27, 29, 30, 32, 23, 25, 26, 28, 29, 31, 33, 35, 25, 27, 28, 30, 32, 33, 36, 38,
};

// The bit reader over the elementary stream.
class Bits {
public:
    Bits(const std::uint8_t *data, std::size_t size, std::size_t bit) : data_(data), size_(size), bit_(bit) {}

    std::size_t bit() const { return bit_; }
    void seek(std::size_t bit) { bit_ = bit; }
    std::size_t available() const { return size_ * 8 > bit_ ? size_ * 8 - bit_ : 0; }

    std::uint32_t peek(std::size_t n) const {
        if (!n) return 0;
        std::uint64_t v = 0;
        const std::size_t byte = bit_ >> 3;
        for (std::size_t i = 0; i < 8; ++i) v = (v << 8) | (byte + i < size_ ? data_[byte + i] : 0u);
        return static_cast<std::uint32_t>((v << (bit_ & 7u)) >> (64 - n));
    }

    std::uint32_t get(std::size_t n) {
        const std::uint32_t v = peek(n);
        bit_ += n;
        return v;
    }

    void skip(std::size_t n) { bit_ += n; }

    // Reads a variable length code. Returns false when the bits do not hold
    // one.
    bool decode(const Lookup &l, std::uint32_t &value) {
        if (available() < l.bits) return false;
        const vlc::Entry &e = l.entries[peek(l.bits)];
        if (!e.length) return false;
        bit_ += e.length;
        value = e.value;
        return true;
    }

    bool decode(const DctLookup &l, std::uint8_t &run, std::int16_t &level) {
        if (available() < l.bits) return false;
        const vlc::DctEntry &e = l.entries[peek(l.bits)];
        if (!e.length) return false;
        bit_ += e.length;
        run = e.run;
        level = get(1) ? static_cast<std::int16_t>(-e.level) : e.level;
        return true;
    }

private:
    const std::uint8_t *data_;
    std::size_t size_;
    std::size_t bit_;
};

// The decoder's working state for one picture.
struct Slice {
    std::uint32_t quantiser = 1;
    bool intra = true;
    std::uint8_t coded_block_pattern = 0x3Fu;
};

// The escape is marked in the table by this run value.
constexpr std::uint8_t kEscapeRun = 255;

// The inverse DCT of the standard (a float one, as the IPU model does).
void idct(const int *in, std::int16_t *out, const double (&cosines)[8][8]) {
    double rows[8][8];
    for (int v = 0; v < 8; ++v)
        for (int x = 0; x < 8; ++x) {
            double s = 0;
            for (int u = 0; u < 8; ++u) s += cosines[x][u] * in[v * 8 + u];
            rows[v][x] = s;
        }
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            double s = 0;
            for (int v = 0; v < 8; ++v) s += cosines[y][v] * rows[v][x];
            out[y * 8 + x] = static_cast<std::int16_t>(std::clamp(static_cast<int>(std::floor(s + 0.5)), -256, 255));
        }
}

}  // namespace

void VideoDecoder::configure(std::uint32_t width, std::uint32_t height) {
    width_ = width;
    height_ = height;
    std::memcpy(intra_matrix_, kDefaultIntra, sizeof intra_matrix_);
    std::memcpy(nonintra_matrix_, kDefaultNonIntra, sizeof nonintra_matrix_);
    previous_ = Picture{};
    have_previous_ = false;
    pictures_ = 0;
    error_ = nullptr;
}

bool VideoDecoder::decode(const std::uint8_t *data, std::size_t size, std::size_t &at, Picture &out) {
    if (!data || size < 4) {
        error_ = "no data";
        return false;
    }
    // Find the next picture header.
    std::size_t at_code = at;
    for (;;) {
        while (at_code + 4 <= size && !(data[at_code] == 0x00 && data[at_code + 1] == 0x00 && data[at_code + 2] == 0x01))
            ++at_code;
        if (at_code + 4 > size) {
            error_ = "no picture header";
            return false;
        }
        const std::uint8_t code = data[at_code + 3];
        if (code >= 0x00 && code <= 0x1F) break;   // a picture header
        at_code += 4;
    }

    // The picture header: the temporal reference, the coding type, the vbv
    // delay and the two full-pel flags and the two f_codes.
    Bits bits(data, size, (at_code + 4) * 8);
    bits.get(10);
    const std::uint32_t coding_type = bits.get(3);
    bits.get(16);
    bits.get(1);
    bits.get(1);
    bits.get(3);
    bits.get(3);

    if (coding_type != 1) {
        // The predicted pictures need the motion compensation; the decoder
        // stops at the picture it cannot decode.
        error_ = "the predicted pictures are not decoded yet";
        at = at_code;
        return false;
    }
    // The picture coding extension follows the picture header in an MPEG-2
    // stream. It carries the f_codes and the scan flags, and the slices come
    // after it.
    {
        std::size_t at_extension = bits.bit() / 8;
        while (at_extension + 4 <= size &&
               !(data[at_extension] == 0x00 && data[at_extension + 1] == 0x00 && data[at_extension + 2] == 0x01))
            ++at_extension;
        if (at_extension + 4 <= size && data[at_extension + 3] == 0xB5) {
            std::size_t at_next = at_extension + 4;
            while (at_next + 4 <= size &&
                   !(data[at_next] == 0x00 && data[at_next + 1] == 0x00 && data[at_next + 2] == 0x01))
                ++at_next;
            bits.seek(at_next * 8);
        } else {
            bits.seek(at_extension * 8);
        }
    }
    if (width_ == 0 || height_ == 0) {
        error_ = "the picture size is not known";
        at = at_code;
        return false;
    }

    out = Picture{};
    out.width = width_;
    out.height = height_;
    out.luma.assign(static_cast<std::size_t>(width_) * height_, 0);
    out.chroma_b.assign(static_cast<std::size_t>(width_ / 2) * (height_ / 2), 0);
    out.chroma_r.assign(static_cast<std::size_t>(width_ / 2) * (height_ / 2), 0);

    const std::uint32_t macroblocks_x = (width_ + 15) / 16;
    const std::uint32_t macroblocks_y = (height_ + 15) / 16;
    std::int16_t dc[3] = {128, 128, 128};
    std::uint32_t macroblock = 0;
    std::size_t picture_end = size;
    bool failed = false;

    // The slices of the picture.
    for (;;) {
        // The next start code after the current position.
        std::size_t at_slice = bits.bit() / 8;
        while (at_slice + 4 <= picture_end &&
               !(data[at_slice] == 0x00 && data[at_slice + 1] == 0x00 && data[at_slice + 2] == 0x01))
            ++at_slice;
        if (at_slice + 4 > picture_end) break;
        const std::uint8_t code = data[at_slice + 3];
        if (code == 0xB7) break;                    // the sequence end
        if (code < 0x01 || code > 0xAF) {           // not a slice
            bits.seek((at_slice + 4) * 8);
            continue;
        }
        bits.seek((at_slice + 4) * 8);

        // The slice header: the vertical position, the priority breakpoint,
        // the quantiser scale code, the extra slice flag and the intra slice
        // flag.
        Slice slice;
        bits.get(8);
        bits.get(7);
        slice.quantiser = bits.get(5);
        if (bits.get(1)) bits.get(8);              // the extra slice information
        slice.intra = bits.get(1) != 0;
        if (!slice.intra) {
            // The coded block pattern and the two sets of reserved bits.
            std::uint32_t cbp = 0;
            if (!bits.decode(vlc::tables().cbp, cbp)) {
                failed = true;
                break;
            }
            slice.coded_block_pattern = static_cast<std::uint8_t>(cbp);
            bits.get(5);
            bits.get(5);
        }
        // The DC predictors restart at every slice.
        dc[0] = dc[1] = dc[2] = 128;

        // The macroblocks of the slice.
        for (;;) {
            // The macroblock address increment: the skipped macroblocks (the
            // codes 0x22 and 0x23) and the macroblock itself.
            std::uint32_t increment = 0;
            if (!bits.decode(vlc::tables().mbai, increment)) {
                failed = true;
                break;
            }
            if (increment == 0x22 || increment == 0x23) {
                macroblock += (increment == 0x22) ? 1u : 2u;
                continue;
            }
            macroblock += increment - 1u;
            if (macroblock >= macroblocks_x * macroblocks_y) break;

            // The macroblock type: the quantiser follows when the type says so.
            std::uint32_t type = 0;
            if (!bits.decode(vlc::tables().mb_i, type)) {
                failed = true;
                break;
            }
            std::uint32_t quantiser = slice.quantiser;
            if (type & 0x10u) quantiser = bits.get(5);

            // The macroblock's six blocks.
            std::int16_t blocks[6][64] = {};
            const std::uint8_t cbp = slice.intra ? 0x3Fu : slice.coded_block_pattern;
            for (int b = 0; b < 6; ++b) {
                if (!(cbp & (32u >> b))) continue;
                const int channel = b < 4 ? 0 : b - 3;
                int coef[64] = {};
                int index = 0;
                const DctLookup &table = slice.intra ? vlc::tables().dct1 : vlc::tables().dct0;
                const bool table_one = slice.intra;   // the MPEG-2 intra table
                if (slice.intra) {
                    // The DC difference, then the AC coefficients.
                    std::uint32_t size = 0;
                    if (!bits.decode(channel ? vlc::tables().dc_chroma : vlc::tables().dc_luma, size)) {
                        failed = true;
                        break;
                    }
                    int diff = 0;
                    if (size) {
                        const int v = static_cast<int>(bits.get(size));
                        diff = v < (1 << (size - 1)) ? v + 1 - (1 << size) : v;
                    }
                    dc[channel] = static_cast<std::int16_t>(dc[channel] + diff);
                    coef[0] = dc[channel];
                    index = 1;
                } else if (bits.peek(1)) {
                    coef[0] = bits.get(2) & 1u ? -1 : 1;
                    index = 1;
                }
                if (failed) break;
                for (;;) {
                    if (index) {
                        if (table_one ? bits.peek(4) == 6u : bits.peek(2) == 2u) {
                            bits.skip(table_one ? 4 : 2);
                            break;
                        }
                    }
                    std::uint8_t run = 0;
                    std::int16_t level = 0;
                    if (!bits.decode(table, run, level)) {
                        failed = true;
                        break;
                    }
                    if (run == kEscapeRun) {
                        run = static_cast<std::uint8_t>(bits.get(6));
                        int v = static_cast<int>(bits.get(12));
                        level = static_cast<std::int16_t>(v & 0x800 ? v - 0x1000 : v);
                    }                    index += run;
                    if (index >= 64) {
                        failed = true;
                        break;
                    }
                    coef[index++] = level;
                }
                if (failed) break;

                // The dequantisation and the inverse DCT.
                const std::uint8_t *weights = slice.intra ? intra_matrix_ : nonintra_matrix_;
                const std::uint8_t *scan = ipu::tables::kZigzag;
                int f[64] = {};
                int sum = 0;
                for (int k = 0; k < 64; ++k) {
                    const int v = coef[k];
                    const int r = scan[k];
                    int x;
                    if (slice.intra && k == 0) {
                        x = v * 8;
                    } else {
                        if (!v) continue;
                        const int sign = v > 0 ? 1 : -1;
                        x = slice.intra ? (v * weights[r] * quantiser * 2) / 32
                                        : ((2 * v + sign) * weights[r] * quantiser) / 32;
                    }
                    x = std::clamp(x, -2048, 2047);
                    f[r] = x;
                    sum += x;
                }
                if (!(sum & 1)) f[63] += f[63] & 1 ? -1 : 1;
                idct(f, blocks[b], vlc::tables().cosines);
            }
            if (failed) break;

            ++macroblocks_;
            // The macroblock into the picture: the four luma blocks and the
            // two chroma blocks.
            const std::uint32_t mb_x = (macroblock % macroblocks_x) * 16;
            const std::uint32_t mb_y = (macroblock / macroblocks_x) * 16;
            for (int i = 0; i < 16; ++i)
                for (int j = 0; j < 16; ++j) {
                    const std::uint32_t x = mb_x + j, y = mb_y + i;
                    if (x >= width_ || y >= height_) continue;
                    const int block = (i < 8 ? 0 : 2) + (j < 8 ? 0 : 1);
                    out.luma[static_cast<std::size_t>(y) * width_ + x] =
                        static_cast<std::uint8_t>(std::clamp<int>(blocks[block][(i % 8) * 8 + (j % 8)], 0, 255));
                }
            for (int i = 0; i < 8; ++i)
                for (int j = 0; j < 8; ++j) {
                    const std::uint32_t x = mb_x / 2 + j, y = mb_y / 2 + i;
                    if (x >= width_ / 2 || y >= height_ / 2) continue;
                    const std::size_t at_chroma = static_cast<std::size_t>(y) * (width_ / 2) + x;
                    out.chroma_b[at_chroma] = static_cast<std::uint8_t>(std::clamp<int>(blocks[4][i * 8 + j], 0, 255));
                    out.chroma_r[at_chroma] = static_cast<std::uint8_t>(std::clamp<int>(blocks[5][i * 8 + j], 0, 255));
                }
        }
        if (failed) break;
    }

    if (failed) {
        if (!error_) error_ = "the picture did not decode";
        at = at_code;
        return false;
    }
    ++pictures_;
    at = bits.bit() / 8;
    previous_ = out;
    have_previous_ = true;
    return true;
}

}  // namespace ps2::media::mpeg2
