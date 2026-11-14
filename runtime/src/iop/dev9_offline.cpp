#include "dev9_offline.hpp"

#include <cstdio>
#include <utility>

namespace ps2::iop::dev9_offline {

namespace {

void register_interrupt(Cpu &) {
    // This offline replacement has no DEV9 interrupt source. The SDK export
    // has a void result, so accepting a registration is the authentic ABI.
}

void dma_transfer(Cpu &cpu) {
    // PS2SDK's SpdDmaTransfer returns -1 for an unavailable device or when its
    // required DMA callbacks are not registered. No host DEV9 device exists
    // here, so report that same offline error instead of fabricating a DMA.
    cpu.r[2] = 0xFFFFFFFFu;
}

void interrupt_enable(Cpu &) {
    // No DEV9 interrupt source is installed in offline mode.
}

void interrupt_disable(Cpu &) {
    // No DEV9 interrupt source is installed in offline mode.
}

void get_ethernet_id(Cpu &cpu) {
    // The PS2SDK driver returns -1 when its EEPROM data is not initialized.
    // Leave the caller's output buffer untouched, matching that path.
    cpu.r[2] = 0xFFFFFFFFu;
}

void register_pre_dma_callback(Cpu &) {
    // Registrations are accepted by the void SDK ABI; offline DMA never fires.
}

void register_post_dma_callback(Cpu &) {
    // Registrations are accepted by the void SDK ABI; offline DMA never fires.
}

} // namespace

void reset() {}

kernel::NativeIopModule make_module() {
    kernel::NativeIopModule module;
    module.name = "DEV9";
    module.start = [](const std::string &) {
        std::fprintf(stderr, "[iop-native] DEV9 offline replacement active; no network device is attached\n");
        return true;
    };

    kernel::NativeIopLibrary library;
    library.name = "dev9";
    library.version = 0x0109u;
    library.exports = {
        {4, register_interrupt},
        {5, dma_transfer},
        {7, interrupt_enable},
        {8, interrupt_disable},
        {9, get_ethernet_id},
        {12, register_pre_dma_callback},
        {13, register_post_dma_callback},
    };
    module.libraries.push_back(std::move(library));
    return module;
}

} // namespace ps2::iop::dev9_offline
