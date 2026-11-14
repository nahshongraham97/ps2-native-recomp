#pragma once

#include <cstdint>

namespace ps2::state {
class Stream;
}

namespace ps2::iop::padman {

bool handle_rpc(std::uint32_t method, std::uint32_t args, std::uint32_t args_size,
                std::uint32_t ret, std::uint32_t ret_size);

void reset();
void activate();
void frame_tick();
void state(ps2::state::Stream &s);
void restore_sif_overrides();

} // namespace ps2::iop::padman
