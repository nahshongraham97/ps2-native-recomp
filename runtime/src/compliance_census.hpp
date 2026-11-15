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
void card_rpc(std::uint32_t method, bool ok);
void cdvd_sector_read(std::uint32_t sectors);
void report();

} // namespace ps2::census
