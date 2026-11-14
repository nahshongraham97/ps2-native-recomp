#pragma once

#include "kernel.hpp"

namespace ps2::iop::usbd_offline {

// Host-native USBD service for a machine with no attached IOP USB devices.
// It replaces USBD.IRX without loading or executing the guest module.
void reset();
kernel::NativeIopModule make_module();

} // namespace ps2::iop::usbd_offline
