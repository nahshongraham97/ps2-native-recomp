#pragma once

#include "kernel.hpp"

namespace ps2::iop::adx_native {

// Host-native replacement for CRI_ADXI.IRX (SID 0x90000200).
// Replaces the guest CRI ADX driver without loading guest IOP code
// or executing guest IPU/RSP emulation.
void reset();
void install();
kernel::NativeIopModule make_module();

} // namespace ps2::iop::adx_native
