#pragma once

#include <cstdint>
#include <cstddef>

namespace ps2::iop::audio_native {

void reset();

// ADMA Stream Interface (Slice 1)
bool has_active_stream(int core);
void start_stream(int core, std::uint32_t mode, std::uint32_t iop_address, std::uint32_t bytes, std::uint32_t start_address);
void stop_stream(int core);
std::uint32_t get_active_buffer(int core);
std::uint32_t get_next_fill_address(int core);
std::uint32_t get_current_address(int core);

// Native Voice Interface (Slice 2)
void key_on(int core, std::uint32_t mask);
void key_off(int core, std::uint32_t mask);
bool set_voice_param(int core, int voice, std::uint32_t index, std::uint16_t value);
bool get_voice_param(int core, int voice, std::uint32_t index, std::uint16_t &value);
bool set_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t value);
bool get_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t &value);
bool set_switch(int core, std::uint32_t index, std::uint32_t value);
bool get_switch(int core, std::uint32_t index, std::uint32_t &value);
void set_master_volume(int core, std::uint16_t left, std::uint16_t right);
std::uint16_t *sound_ram();

// Unified Native Mixer (Slice 3)
void render_stream_sample(int core, const bool input_wet[2], std::int32_t dry[2], std::int32_t wet[2],
                          std::int16_t &raw_l, std::int16_t &raw_r, const std::uint16_t bvol[2]);
void render_voices_batch(int core, std::size_t batch_frames, std::int32_t (*dry)[2], std::int32_t (*wet)[2]);

// Frame hook: called once per rendered audio batch with the number of frames
// it covers. The native sound service uses it to advance its sequencer's note
// timing in step with the mixer.
void set_frame_hook(void (*hook)(std::size_t frames));

} // namespace ps2::iop::audio_native
