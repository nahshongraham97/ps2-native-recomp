#include "iop/padman.hpp"
#include "iop/sio2.hpp"
#include "ps2/memory.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    const bool strict = argc > 1 && std::string(argv[1]) == "strict-truncated";
#ifdef _WIN32
    _putenv_s("PS2_IOP_NATIVE_STRICT", strict ? "1" : "0");
#else
    setenv("PS2_IOP_NATIVE_STRICT", strict ? "1" : "0", 1);
#endif
    constexpr std::uint32_t args = 0x1000, reply = 0x3000, area = 0x2000;
    ps2::iop::padman::activate();
    ps2::st32(args, 0x10); // padInit
    if (!ps2::iop::padman::handle_rpc(1, args, 4, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;

    ps2::st32(args, 0x0C); // padGetPortMax
    if (!ps2::iop::padman::handle_rpc(1, args, 4, reply, 24) || ps2::ld32(reply + 12) != 2) return 1;
    ps2::st32(args, 0x0D); // padGetSlotMax
    ps2::st32(args + 4, 1);
    if (!ps2::iop::padman::handle_rpc(1, args, 8, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;
    ps2::st32(args + 4, 2);
    if (!ps2::iop::padman::handle_rpc(1, args, 8, reply, 24) || ps2::ld32(reply + 12) != 0) return 1;

    ps2::st32(0, 0x12345678);
    ps2::st32(args, 1); // padPortOpen; its area argument is missing
    // In strict mode this call must terminate at the service boundary.
    if (!ps2::iop::padman::handle_rpc(1, args, 4, reply, 24) || ps2::ld32(reply + 12) != 0 ||
        ps2::ld32(0) != 0x12345678) {
        std::cerr << "truncated padPortOpen was accepted or overwrote EE address zero\n";
        return 1;
    }
    if (strict) {
        std::cerr << "strict truncated request unexpectedly returned\n";
        return 1;
    }

    ps2::st32(args + 4, 0); // port
    ps2::st32(args + 16, area);
    ps2::st32(args + 8, 1); // multitap slot is not supported by the host
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 0) return 1;
    ps2::st32(args + 8, 0); // ordinary controller slot
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 1 ||
        ps2::ld8(area) != 0xFF || ps2::ld8(area + 128) != 0xFF) {
        std::cerr << "complete padPortOpen did not initialize both records\n";
        return 1;
    }
    ps2::st32(args, 0x06); // padSetMainMode: select the already-published DualShock mode
    ps2::st32(args + 4, 0);
    ps2::st32(args + 8, 0);
    ps2::st32(args + 12, 1);
    ps2::st32(args + 16, 1);
    ps2::st32(reply + 20, 0xFFFFFFFFu);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 20) != 1u) {
        std::cerr << "padSetMainMode did not write padModeResult.result at +20\n";
        return 1;
    }
    ps2::iop::sio2::set_connected(0, true);
    ps2::iop::sio2::set_buttons(0, 0x4000);
    ps2::iop::padman::frame_tick();
    if (ps2::ld32(area + 128 + 88) != 1 || ps2::ld8(area + 128 + 3) != 0xBF ||
        ps2::ld8(area + 128 + 114) != 1 ||
        ps2::ld32(area + 88) != 0) {
        std::cerr << "first pad update did not publish the expected frame and button state\n";
        return 1;
    }
    const std::array<std::uint8_t, 6> alignment{{0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF}};
    ps2::st32(args, 0x08); // padSetActAlign
    ps2::st32(args + 4, 0);
    ps2::st32(args + 8, 0);
    for (std::uint32_t i = 0; i < alignment.size(); ++i) ps2::st8(args + 12 + i, alignment[i]);
    ps2::st32(reply + 20, 0xFFFFFFFFu);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 20) != 1u ||
        ps2::iop::sio2::actuator_alignment(0) != alignment) {
        std::cerr << "padSetActAlign did not return success and apply the SIO2 actuator map\n";
        return 1;
    }
    for (std::uint32_t i = 0; i < alignment.size(); ++i) {
        if (ps2::ld8(area + 128 + 40 + i) != alignment[i]) {
            std::cerr << "padSetActAlign did not publish the map in the active EE record\n";
            return 1;
        }
    }
    ps2::iop::sio2::set_buttons(0, 0);
    ps2::iop::padman::frame_tick();
    if (ps2::ld32(area + 88) != 2 || ps2::ld8(area + 3) != 0xFF ||
        ps2::ld8(area + 40) != alignment[0] || ps2::ld8(area + 41) != alignment[1] ||
        ps2::ld32(area + 128 + 88) != 1) {
        std::cerr << "second pad update did not alternate records\n";
        return 1;
    }
    ps2::st32(args, 0x0E); // synchronous padPortClose
    ps2::st32(args + 16, 1);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 1) {
        std::cerr << "synchronous close failed\n";
        return 1;
    }
    ps2::iop::padman::frame_tick();
    if (ps2::ld32(area + 88) != 2 || ps2::ld32(area + 128 + 88) != 1) {
        std::cerr << "closed pad continued to publish records\n";
        return 1;
    }
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 0) {
        std::cerr << "closing an already closed pad did not fail\n";
        return 1;
    }
    ps2::st32(args, 0x01);
    ps2::st32(args + 16, area);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;
    constexpr std::uint32_t area2 = area + 256;
    ps2::st32(args + 4, 1);
    ps2::st32(args + 16, area2);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;
    ps2::iop::sio2::set_connected(1, true);
    ps2::iop::padman::frame_tick();
    ps2::st32(args, 0x0F); // padEnd
    if (!ps2::iop::padman::handle_rpc(1, args, 4, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;
    ps2::iop::padman::frame_tick();
    if (ps2::ld32(area + 128 + 88) != 1 || ps2::ld32(area + 88) != 0 ||
        ps2::ld32(area2 + 128 + 88) != 1 || ps2::ld32(area2 + 88) != 0) {
        std::cerr << "padEnd did not stop updates\n";
        return 1;
    }
    ps2::st32(args, 0x10); // padInit after padEnd
    if (!ps2::iop::padman::handle_rpc(1, args, 4, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;
    ps2::st32(args, 0x01);
    ps2::st32(args + 4, 0);
    ps2::st32(args + 16, area);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 12) != 1) return 1;
    ps2::iop::padman::frame_tick();
    if (ps2::ld32(area + 128 + 88) != 1) {
        std::cerr << "pad did not resume after reinitialization\n";
        return 1;
    }
    if (ps2::ld32(area2 + 128 + 88) != 1 || ps2::ld32(area2 + 88) != 0) {
        std::cerr << "reinitialization reopened a previously closed second port\n";
        return 1;
    }
    return 0;
}
