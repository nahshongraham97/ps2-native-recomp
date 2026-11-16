#pragma once

#include <cstdint>
#include <string>

namespace ps2::state {
class Stream;
}

namespace ps2::iop {
struct Cpu;
}

namespace ps2::iop::kernel {

// Reads no farther than the size observed at open; false indicates host I/O
// failure or a file shortened since then. Partial bytes are reported.
bool mwfilefr_read_host_range(const std::string &path, std::uint64_t open_size,
                              std::uint64_t offset, std::uint8_t *out,
                              std::uint32_t requested, std::uint32_t &transferred);

// Registers the host C++ service for MWFILEFR's observed SIF RPC endpoint.
// The module descriptor calls this before EE clients bind to the endpoint.
void install_native_mwfilefr_service();
// Reads a byte range from a disc path through the native file service's path
// resolution (the native sound service reads the retail sound bank this way).
bool mwfilefr_read_guest_range(const std::string &guest_path, std::uint64_t offset,
                               std::uint8_t *out, std::uint32_t requested);
// Native C++ implementation of the MWFILEFR mwfile v1.03 ordinal 4 open API.
void mwfilefr_open_import(ps2::iop::Cpu &c);
// Native C++ implementation of the MWFILEFR mwfile v1.03 ordinal 5 close API.
void mwfilefr_close_import(ps2::iop::Cpu &c);
// Native C++ implementation of the MWFILEFR mwfile v1.03 ordinal 10 read API.
void mwfilefr_read_import(ps2::iop::Cpu &c);
// The retail IRX also exports ordinals 8, 9 and 14; the native service now
// provides them (measured no-ops that log their arguments) so a call can no
// longer fail silently.
void mwfilefr_ordinal8_import(ps2::iop::Cpu &c);
void mwfilefr_ordinal9_import(ps2::iop::Cpu &c);
void mwfilefr_ordinal14_import(ps2::iop::Cpu &c);
// MWFILEFR ordinal 13's exported MIPS32 thunk reaches a leaf that only returns;
// its return-value register is therefore preserved rather than synthesized.
void mwfilefr_ordinal13_return_import(ps2::iop::Cpu &c);
// MWFILEFR ordinal 16's worker wakeup is owned by the native job scheduler.
void mwfilefr_ordinal16_worker_dispatch_import(ps2::iop::Cpu &c);
void register_native_mwfilefr_job();
void mwfilefr_state(ps2::state::Stream &s);

} // namespace ps2::iop::kernel
