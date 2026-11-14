#pragma once

#include <cstddef>

namespace ps2::port_config {

// The PS2-hardware replacement choices this process actually runs with, resolved
// from the environment once at startup. A run log states this so a captured
// frame or a crash address is always attributable to one exact configuration;
// without it two runs of different builds cannot be compared.
struct Flags {
    bool native_gl = false;          // native OpenGL GS backend draws the display
    bool packet_native = false;      // GIF packets go to the native packet renderer
    bool packet_strict = false;      // the packet renderer rejects unknown registers
    bool vu_aot_strict = false;      // VU1 must resolve to a compiled program
    bool iop_native_strict = false;  // IOP must resolve to a host-native service
};

Flags current();

// Writes "native-gl=1 packets=1 packets-strict=1 vu-aot-strict=1
// iop-native-strict=1" and returns buffer. Truncates safely.
const char *describe(const Flags &flags, char *buffer, std::size_t size);

} // namespace ps2::port_config
