#pragma once

#include "kernel.hpp"

namespace ps2::iop::dev9_offline {

void reset();
kernel::NativeIopModule make_module();

} // namespace ps2::iop::dev9_offline
