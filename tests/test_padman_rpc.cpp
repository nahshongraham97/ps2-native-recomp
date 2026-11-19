#include "iop/padman.hpp"
#include "iop/sio2.hpp"
#include "mka/controller_input.hpp"
#include "mka/haptics.hpp"
#include "ps2/memory.hpp"
#include "ps2/system.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    const bool truncated = argc > 1 && std::string(argv[1]) == "strict-truncated";
    const bool invalid_slot = argc > 1 && std::string(argv[1]) == "strict-slot";
    constexpr std::uint32_t args = 0x1000, reply = 0x3000, area = 0x2000;
    auto complete=mka::input::digital_state(0x4100,true,{{17,29,73,201}},73,0);
    complete.pressure[14]=37;
    ps2::set_pad_state(complete);
    ps2::publish_controller_frame();
    auto complete_copy=mka::input::controllers().sample(0);
    if(!complete_copy||complete_copy->state.pressure!=complete.pressure||complete_copy->state.axes!=complete.axes)return 1;
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
    // Production is always strict. Run rejection in a separate process; the
    // harness requires its abort exit and named boundary diagnostic.
    if (truncated) {
        ps2::iop::padman::handle_rpc(1, args, 4, reply, 24);
        std::cerr << "strict truncated request unexpectedly returned\n";
        return 1;
    }

    ps2::st32(args + 4, 0); // port
    ps2::st32(args + 16, area);
    ps2::st32(args + 8, 1); // multitap slot is not supported by the host
    if(invalid_slot) {
        ps2::iop::padman::handle_rpc(1,args,20,reply,24);
        std::cerr<<"strict invalid slot unexpectedly returned\n";
        return 1;
    }
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
    // Opposing SIO2 values prove input is read from the host service alone.
    ps2::iop::sio2::set_connected(0, false);
    ps2::iop::sio2::set_buttons(0, 0x8000);
    ps2::iop::padman::frame_tick();
    if (ps2::ld32(area + 128 + 88) != 1 || ps2::ld8(area + 128 + 3) != 0xBE ||
        ps2::ld8(area + 128 + 114) != 1 ||
        ps2::ld32(area + 88) != 0 || ps2::ld8(area+128+2)!=0xFFu ||
        ps2::ld8(area+128+4)!=17u || ps2::ld8(area+128+7)!=201u ||
        ps2::ld8(area+128+18)!=73u || ps2::ld8(area+128+14)!=37u) {
        std::cerr << "first pad update did not publish the expected frame and button state\n";
        return 1;
    }
    const std::array<std::uint8_t, 6> alignment{{0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF}};
    const std::array<std::uint8_t,6> opposing_alignment{{0x01,0x00,0xFF,0xFF,0xFF,0xFF}};
    ps2::iop::sio2::set_actuator_alignment(0,opposing_alignment);
    ps2::st32(args, 0x08); // padSetActAlign
    ps2::st32(args + 4, 0);
    ps2::st32(args + 8, 0);
    for (std::uint32_t i = 0; i < alignment.size(); ++i) ps2::st8(args + 12 + i, alignment[i]);
    ps2::st32(reply + 20, 0xFFFFFFFFu);
    if (!ps2::iop::padman::handle_rpc(1, args, 20, reply, 24) || ps2::ld32(reply + 20) != 1u ||
        !mka::input::haptics().sample(0) || mka::input::haptics().sample(0)->alignment != alignment ||
        ps2::iop::sio2::actuator_alignment(0) != opposing_alignment) {
        std::cerr << "padSetActAlign did not apply native alignment independently of SIO2\n";
        return 1;
    }
    for (std::uint32_t i = 0; i < alignment.size(); ++i) {
        if (ps2::ld8(area + 128 + 40 + i) != alignment[i]) {
            std::cerr << "padSetActAlign did not publish the map in the active EE record\n";
            return 1;
        }
    }
    if(!mka::input::controllers().publish(0,mka::input::digital_state(0,true)))return 1;
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
    if(!mka::input::controllers().publish(1,mka::input::digital_state(0,true)))return 1;
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
