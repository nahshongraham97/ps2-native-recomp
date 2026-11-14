#pragma once

#include "kernel.hpp"

namespace ps2::iop::libsd_native {

void reset();
kernel::NativeIopModule make_module();

} // namespace ps2::iop::libsd_native
