#pragma once

#include "kernel.hpp"

namespace ps2::iop::libsd_native {

void reset();
kernel::NativeIopModule make_module();
void on_dma_done(std::uint32_t core);

} // namespace ps2::iop::libsd_native
