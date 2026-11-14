#include "ps2/port_config.hpp"

#include "ps2/native_gl.hpp"
#include "ps2/vu.hpp"
#include "gs/native_renderer.hpp"
#include "iop/kernel.hpp"

#include <cstdio>

namespace ps2::port_config {

Flags current() {
    Flags flags;
    flags.native_gl = gs::native_gl::selected();
    flags.packet_native = gs::native_renderer::selected();
    flags.packet_strict = gs::native_renderer::strict();
    flags.vu_aot_strict = vu::strict_aot_enabled();
    flags.iop_native_strict = iop::kernel::native_strict();
    return flags;
}

const char *describe(const Flags &flags, char *buffer, std::size_t size) {
    if (!buffer || size == 0) return "";
    std::snprintf(buffer, size, "native-gl=%d packets=%d packets-strict=%d vu-aot-strict=%d iop-native-strict=%d",
                  flags.native_gl ? 1 : 0, flags.packet_native ? 1 : 0, flags.packet_strict ? 1 : 0,
                  flags.vu_aot_strict ? 1 : 0, flags.iop_native_strict ? 1 : 0);
    return buffer;
}

} // namespace ps2::port_config
