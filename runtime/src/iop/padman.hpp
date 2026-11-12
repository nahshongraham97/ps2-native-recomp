#pragma once

namespace ps2::state {
class Stream;
}

namespace ps2::iop::padman {

void reset();
void activate();
void frame_tick();
void state(ps2::state::Stream &s);
void restore_sif_overrides();

} // namespace ps2::iop::padman
