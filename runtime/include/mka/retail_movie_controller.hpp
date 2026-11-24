#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include "mka/retail_display_service.hpp"
#include "mka/movie_gl_device.hpp"
#include "mka/movie_color.hpp"

namespace mka::retail {

// 33-entry movie descriptor structure recovered from Xbox 0x00579828 and PS2 0x005a24c0.
// Offset +0x00: Standard 4:3 filename (*N.SFD), width +0x04, height +0x08.
// Offset +0x0c: Widescreen 16:9 filename (*W.SFD), width +0x10, height +0x14.
// Offset +0x18: PAL60 4:3 filename (*6N.SFD) - PS2 only, 0 on Xbox.
// Offset +0x1c: PAL60 16:9 filename (*6W.SFD) - PS2 only, 0 on Xbox.
struct RetailMovieDescriptor {
    const char* norm_name;
    std::uint32_t norm_w;
    std::uint32_t norm_h;
    const char* wide_name;
    std::uint32_t wide_w;
    std::uint32_t wide_h;
    const char* pal60_norm_name; // 6N (PS2 European 60Hz 4:3 variant)
    const char* pal60_wide_name; // 6W (PS2 European 60Hz 16:9 variant)
};

// Returns the authentic 33-entry retail movie descriptor table (indices 0..32)
std::span<const RetailMovieDescriptor, 33> retail_movie_descriptor_table() noexcept;

// Retail movie variant selection (recovered from Xbox 0x0025d5b0 and PS2 0x00181fd0).
// Clamps movie_id to 0..32 (Xbox 0x0025d5b4).
// If widescreen_active is true and wide_name is non-null, selects wide_name with 16:9 aspect.
// Otherwise falls back to norm_name with 4:3 aspect.
struct RetailMovieSelection {
    std::uint32_t movie_id{0};
    const char* filename{nullptr};
    std::uint32_t width{0};
    std::uint32_t height{0};
    DisplayAspectMode aspect_mode{DisplayAspectMode::standard_4_3};
    bool is_widescreen{false};
};

RetailMovieSelection select_retail_movie_variant(std::uint32_t movie_id, bool widescreen_active) noexcept;

// Retail movie frame presentation result.
struct RetailMoviePlaybackResult {
    bool success{false};
    RetailMovieSelection selection{};
    video::MovieGpuError gpu_error{video::MovieGpuError::none};
    video::MovieColorError color_error{video::MovieColorError::none};
    DisplayServiceError display_error{DisplayServiceError::none};
    DisplayViewportRect destination_rect{};
};

// Retail movie frame playback controller (recovered from PS2 0x00181f90 / Xbox 0x0025d380).
// Drives the production display service and NativeMovieGlDevice for retail movie_id.
RetailMoviePlaybackResult play_retail_movie_frame(
    std::uint32_t movie_id,
    NativeProductionDisplayService& display,
    video::NativeMovieGlDevice& movie_device,
    const video::NativeMovieColorTable& color_table,
    const video::MovieVideoFrame& raw_frame,
    bool widescreen_active);

} // namespace mka::retail
