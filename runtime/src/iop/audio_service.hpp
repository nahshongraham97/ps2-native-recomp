#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace ps2::state {
class Stream;
}

// ---------------------------------------------------------------------------
// The native sound service.
//
// It replaces the emulated SPU2 chip. The voices, the mixer, the reverb, the
// sound memory and the ADMA path are the service's own state; the retail
// sound library's parameter interface is decoded into named fields, and the
// sound device's guest window is served by the service itself. No SPU2 device
// model is left on the accepted route: every sound call the game makes ends
// here, in native code.
// ---------------------------------------------------------------------------
namespace ps2::iop::audio_native {

void reset();

// Keeps the service's mixer running when nothing asks it for sound.
void keep_ticking();

// The host's sound sink: the samples the service mixed.
void set_sink(std::function<void(const std::int16_t *, std::size_t)> sink);

// Diagnostics (PS2_DUMP_AUDIO / PS2_TRACE_SPU2).
void dump_audio(const char *path);
void dump_voices();

// The service's state in a save/load stream.
void state(ps2::state::Stream &s);

// The sound memory (2 MB of 16-bit words).
std::uint16_t *sound_ram();

// ---------------------------------------------------------------------------
// The retail sound library's parameter interface, decoded.
//
// The library addressed a sound device's parameters by an index, a core and a
// voice. The service decodes each index into its own state: the voice
// registers, the mixer's routing, the reverb's ring and volumes, the
// attribute register and the transfer registers.
// ---------------------------------------------------------------------------

// A voice parameter (index 0: volume left, 1: volume right, 2: pitch,
// 3: ADSR1, 4: ADSR2, 5: the envelope level).
bool set_voice_param(int core, int voice, std::uint32_t index, std::uint16_t value);
bool get_voice_param(int core, int voice, std::uint32_t index, std::uint16_t &value);

// A voice's sample address (index 32: start, 33: loop, 34: next).
bool set_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t value);
bool get_voice_addr(int core, int voice, std::uint32_t index, std::uint32_t &value);

// The parameter interface of the library (the voice registers, the mixer's
// routing, the reverb's setup and the attribute registers).
bool set_param(int core, int voice, std::uint32_t index, std::uint16_t value);
bool get_param(int core, int voice, std::uint32_t index, std::uint16_t &value);

// The switch parameters (key on, key off, end flags, the voice mix masks).
bool set_switch(int core, std::uint32_t index, std::uint32_t value);
bool get_switch(int core, std::uint32_t index, std::uint32_t &value);

// The voice address parameters.
bool set_addr(int core, int voice, std::uint32_t index, std::uint32_t value);
bool get_addr(int core, int voice, std::uint32_t index, std::uint32_t &value);

// Keying voices on and off.
void key_on(int core, std::uint32_t mask);
void key_off(int core, std::uint32_t mask);

// The frame hook: called once per mixed batch with the number of frames it
// covers, so a sequencer can advance its note timing with the mixer.
void set_frame_hook(void (*hook)(std::size_t frames));

// ---------------------------------------------------------------------------
// Transfers.
// ---------------------------------------------------------------------------

// Moves words between the IOP's memory and the sound memory.
void dma(int core, std::uint32_t madr, std::uint32_t words, bool to_spu, int channel);
void dma_stop(int core);
std::uint32_t dma_address(int core);

// The ADMA stream (the game streaming sound into the service).
bool has_active_stream(int core);
void start_stream(int core, std::uint32_t mode, std::uint32_t iop_address, std::uint32_t bytes, std::uint32_t start_address);
void stop_stream(int core);
std::uint32_t get_active_buffer(int core);
std::uint32_t get_next_fill_address(int core);
std::uint32_t get_current_address(int core);

// ---------------------------------------------------------------------------
// The sound device's guest window.
//
// The IOP's sound registers live in the guest's memory map. The service serves
// them: this is the service's own interface, decoded into its state, not an
// emulated device behind a boundary.
// ---------------------------------------------------------------------------

std::uint32_t window_read(std::uint32_t address, unsigned size);
void window_write(std::uint32_t address, std::uint32_t value, unsigned size);

} // namespace ps2::iop::audio_native
