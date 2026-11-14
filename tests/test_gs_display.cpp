// Contract test for the VRAM display decoder used by movie presentation.
// It writes known pixels through the same page-addressed helpers the guest
// uses and checks the decoded RGBA8 byte order, row order and 16-bit expansion.

#include "gs/gs.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::uint32_t byte_at(std::uint32_t pixel, int index) {
    return (pixel >> (index * 8)) & 0xFFu;
}

} // namespace

int main() {
    // read_pixel/write_pixel use the GS page-offset tables, which init() builds.
    // Keep WebGPU out of this test: the decoder contract is CPU-only.
#if defined(_WIN32)
    SetEnvironmentVariableA("PS2_GS", "off");
    SetEnvironmentVariableA("PS2_NATIVE_GL", "0");
#else
    setenv("PS2_GS", "off", 1);
    setenv("PS2_NATIVE_GL", "0", 1);
#endif
    ps2::gs::init();

    constexpr std::uint32_t fbp = 16;   // 8192-byte page index, as DISPFB.FBP
    constexpr std::uint32_t fbw = 10;   // 10 * 64 = 640 pixels per row
    constexpr std::uint32_t dbx = 4;
    constexpr std::uint32_t dby = 2;
    constexpr std::uint32_t width = 3;
    constexpr std::uint32_t height = 2;

    // PSMCT32: memory byte order is R, G, B, A for each pixel. Fill the whole
    // rectangle with a position-dependent red so row order is observable.
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
            ps2::gs::write_pixel(ps2::gs::PSMCT32, fbp * 32u, fbw, dbx + x, dby + y,
                                 0xFF000000u | (y * width + x));

    std::uint32_t pixels[width * height] = {};
    check(ps2::gs::read_display_rgba(ps2::gs::PSMCT32, fbp, fbw, dbx, dby, width, height, false, pixels),
          "PSMCT32 rectangle is presentable");
    bool rows_match = true;
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
            rows_match = rows_match && pixels[y * width + x] == (0xFF000000u | (y * width + x));
    check(rows_match, "rows are decoded top-down without a vertical flip");

    const std::uint32_t top_left = 0xFF112233u;   // A=FF B=11 G=22 R=33
    ps2::gs::write_pixel(ps2::gs::PSMCT32, fbp * 32u, fbw, dbx, dby, top_left);
    check(ps2::gs::read_display_rgba(ps2::gs::PSMCT32, fbp, fbw, dbx, dby, 1, 1, false, pixels),
          "single PSMCT32 pixel is presentable");
    check(byte_at(pixels[0], 0) == 0x33u && byte_at(pixels[0], 1) == 0x22u &&
          byte_at(pixels[0], 2) == 0x11u && byte_at(pixels[0], 3) == 0xFFu,
          "PSMCT32 low byte is red and alpha is opaque");

    // PSMCT24 has no alpha channel of its own; presentation makes it opaque.
    ps2::gs::write_pixel(ps2::gs::PSMCT24, fbp * 32u, fbw, dbx, dby, 0xFFABCDEFu);
    std::uint32_t ct24 = 0;
    check(ps2::gs::read_display_rgba(ps2::gs::PSMCT24, fbp, fbw, dbx, dby, 1, 1, false, &ct24),
          "PSMCT24 rectangle is presentable");
    check(ct24 == 0xFFABCDEFu, "PSMCT24 keeps 24-bit color and becomes opaque");

    // PSMCT16: 5-bit channels expand to 8 bits; the stored alpha bit is not
    // allowed to make the display translucent.
    ps2::gs::write_pixel(ps2::gs::PSMCT16, fbp * 32u, fbw, dbx, dby, 0x7C1Fu);  // R=31 G=0 B=31
    std::uint32_t ct16 = 0;
    check(ps2::gs::read_display_rgba(ps2::gs::PSMCT16, fbp, fbw, dbx, dby, 1, 1, false, &ct16),
          "PSMCT16 rectangle is presentable");
    check(ct16 == 0xFFF800F8u, "PSMCT16 expands 5-bit channels and stays opaque");

    // The halved-field path duplicates lines instead of reading past the field.
    ps2::gs::write_pixel(ps2::gs::PSMCT32, fbp * 32u, fbw, dbx, dby, 0xFF00FF00u);
    std::uint32_t half[2] = {};
    check(ps2::gs::read_display_rgba(ps2::gs::PSMCT32, fbp, fbw, dbx, dby, 1, 2, true, half),
          "halved-field rectangle is presentable");
    check(half[0] == 0xFF00FF00u && half[1] == 0xFF00FF00u,
          "halved-field rows repeat one source line");

    std::uint32_t untouched = 0xDEADBEEFu;
    check(!ps2::gs::read_display_rgba(ps2::gs::PSMT8, fbp, fbw, dbx, dby, 1, 1, false, &untouched),
          "an unpresentable pixel format is rejected");
    check(untouched == 0xDEADBEEFu, "a rejected format leaves the destination alone");

    if (failures) {
        std::fprintf(stderr, "%d display decoder check(s) failed\n", failures);
        return 1;
    }
    std::fprintf(stderr, "display decoder: all checks passed\n");
    return 0;
}
