#pragma once

#include <cstdint>

namespace ps2::iop::audio_native {

void reset();
bool has_active_stream(int core);
void start_stream(int core, std::uint32_t mode, std::uint32_t iop_address, std::uint32_t bytes, std::uint32_t start_address);
void stop_stream(int core);
std::uint32_t get_active_buffer(int core);
std::uint32_t get_current_address(int core);

void render_stream_sample(int core, const bool input_wet[2], std::int32_t dry[2], std::int32_t wet[2],
                          std::int16_t &raw_l, std::int16_t &raw_r, const std::uint16_t bvol[2]);

} // namespace ps2::iop::audio_native
