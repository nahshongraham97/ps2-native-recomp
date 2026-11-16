#pragma once
#include <cstdint>

namespace ps2::census {

void iop_guest_instructions(std::uint64_t count);        // exact retired guest IOP instructions
void iop_guest_sample(std::uint32_t pc);                  // sampled PC for module attribution
void ee_live_translation(std::uint32_t pc);               // guest EE translated at runtime
void vu_interpreter_fallback(int unit, std::uint32_t pc); // run_fast()
enum class RpcKind { Native, Guest, Unknown };
void sif_rpc(std::uint32_t sid, std::uint32_t method, RpcKind kind);
void gs_transfer(std::uint32_t base, std::uint32_t pixels, bool applied);
void gs_primitive(std::uint32_t fbp);
void spu2_key_on(int core, int voice);
void spu2_key_off(int core, int voice);
void spu2_dma(int core, std::uint32_t bytes, bool to_spu);
void spu2_peak_sample(std::int16_t left, std::int16_t right);
void audio_native_frame_mixed();
void audio_native_block_consumed();
// One operation at the emulated SPU2 device boundary (spu2::read/write/dma). The acceptance gate
// forbids an emulated SPU2 on the accepted route: every sound platform call must terminate in the
// native audio service, not in a PS2 chip register file. This counter is that violation's magnitude
// and must reach 0.
void spu2_device_op();
void card_rpc(std::uint32_t method, bool ok);
void cdvd_sector_read(std::uint32_t sectors);
void report();

// The host registers a hook that drains its log writer, so a shutdown report reaches the log
// file before the process dies. The runtime cannot call the host's writer directly, and without
// it a census printed from a signal handler is lost: the bytes are still in the pipe that the
// log thread had not drained. The hook is called from report() and from every shutdown path.
void set_exit_flush_hook(void (*hook)());

} // namespace ps2::census
