// Offline SPU2 mixer acceptance (Phase D audio evidence).
//
// The chip must produce non-silent samples when a voice is programmed the way
// the native libsd bridge's SdInit plus a retail game program it: SdInit
// through the real bridge, ADPCM in SPU2 RAM, voice volume, start address,
// key-on, and the master volume the game has to set afterwards. The kernel
// scheduler drives the SPU2 tick; no game, no audio device.
//
// The first phase documents the reference-faithful silence gate: freesd's
// sceSdInit(flag=0) zeroes MVOLL/MVOLR (freesd.c InitCoreVolume), and this
// bridge does the same, so a fully keyed voice is still silent until the game
// sets the master volume. The second phase sets it and requires real output.
// Together they prove the mixer chain (voice -> ADPCM -> envelope -> routing
// -> master volume -> sink) end to end, so an in-game silence must be game
// register state or the host output device, not the mixer itself.

#include "iop/iop.hpp"
#include "iop/kernel.hpp"
#include "iop/spu2.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// The register window the native libsd bridge writes (libsd_native.cpp).
constexpr std::uint32_t kSpuBase = 0x1F900000u;

std::vector<std::int16_t> g_captured;

void capture(const std::int16_t *samples, std::size_t frames) {
    g_captured.insert(g_captured.end(), samples, samples + frames * 2);
}

void put16(std::vector<std::uint8_t> &data, std::size_t at, std::uint16_t value) {
    data[at] = static_cast<std::uint8_t>(value);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put32(std::vector<std::uint8_t> &data, std::size_t at, std::uint32_t value) {
    data[at] = static_cast<std::uint8_t>(value);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
    data[at + 2] = static_cast<std::uint8_t>(value >> 16);
    data[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

// The smallest PT_IOPMOD ELF carrying a module name. Loading it makes the
// loader resolve the name to the registered native replacement and run that
// module's start, which is what publishes its exports (as
// test_iop_native_module.cpp does for LIBSD).
std::vector<std::uint8_t> make_iopmod_elf(const std::string &module_name) {
    std::vector<std::uint8_t> data(256);
    data[0] = 0x7F;
    data[1] = 'E';
    data[2] = 'L';
    data[3] = 'F';
    data[4] = 1; // ELFCLASS32
    data[5] = 1; // ELFDATA2LSB
    put32(data, 28, 52);      // e_phoff
    put16(data, 42, 32);      // e_phentsize
    put16(data, 44, 1);       // e_phnum
    put32(data, 52, 0x70000080u); // PT_IOPMOD
    put32(data, 56, 128);         // IOPMOD record file offset
    for (std::size_t i = 0; i < module_name.size(); ++i) data[154 + i] = static_cast<std::uint8_t>(module_name[i]);
    return data;
}

// One SPU2/PSX ADPCM block is 8 words: header word (shift/filter/flags in the
// low byte, block flags in the high byte) plus 28 nibble samples in 7 words.
// shift=0 and filter=0 decode every nibble directly (nibble << 12), flags=0
// keeps the voice running into the following block.
void write_block(std::uint16_t *ram, std::uint32_t word) {
    ram[word] = 0x0000u;
    for (int i = 1; i < 8; ++i) ram[word + i] = 0x7777u;
}

int peak_since(std::size_t from) {
    int peak = 0;
    for (std::size_t i = from; i < g_captured.size(); ++i)
        peak = std::max(peak, std::abs(static_cast<int>(g_captured[i])));
    return peak;
}

// Drives the IOP clock until the chip has pushed at least `batches` more
// 32-frame batches (one tick = 768*32 IOP cycles; the EE divider is 8).
void run_batches(int batches) {
    const std::size_t want = g_captured.size() + static_cast<std::size_t>(batches) * 32 * 2;
    for (int i = 0; i < 64 && g_captured.size() < want; ++i)
        ps2::iop::run(8ull * 768ull * 32ull * 4ull);
}

} // namespace

int main() {
    // Boots the runtime and installs the production native-module table,
    // including the LIBSD v1.5 bridge. The synthetic load resolves to the
    // native replacement and runs its start, publishing the libsd exports.
    ps2::iop::reset();
    ps2::iop::spu2::set_sink(capture);
    const auto libsd_elf = make_iopmod_elf("LIBSD");
    if (ps2::iop::load_module_buffer(libsd_elf, "buffer", "") <= 0) {
        std::fprintf(stderr, "[FAIL] the LIBSD module did not load\n");
        return 1;
    }

    // SdInit (libsd ordinal 4) through the real bridge: resets the chip,
    // enables the dry routing for every voice, sets voice pitch, zeroes the
    // master volumes (freesd.c does the same for flag 0), and schedules the
    // first SPU2 tick.
    ps2::iop::Cpu init_call{};
    if (!ps2::iop::kernel::invoke_native_import("libsd", 0x0105, 4, init_call)) {
        std::fprintf(stderr, "[FAIL] the native libsd SdInit export is not registered\n");
        return 1;
    }

    // Sound bank: enough non-zero ADPCM for both phases (SdInit cleared the
    // RAM, so the data is written after it). 32 blocks = 896 samples. The
    // bank must avoid words 0x0000..0x1FFF (the SPU2 ring buffers overwrite
    // that range every tick) and 0x2800 (SdInit's voice_data).
    std::uint16_t *ram = ps2::iop::spu2::ram();
    constexpr std::uint32_t kBank = 0x4000u;
    for (std::uint32_t b = 0; b < 32; ++b) write_block(ram, kBank + b * 8);

    // Voice 0 as a game programs it after SdInit: playback pitch, full
    // volume, start address in the same word units decode_block reads,
    // key-on. SdInit leaves pitch at the freesd default 0x3FFF (about 4x
    // rate); games overwrite it, and 0x1000 is 1.0x (48 kHz). Master volume
    // is deliberately still zero here.
    ps2::iop::spu2::write(kSpuBase + 0x004u, 0x1000u, 2);            // pitch 1.0x
    ps2::iop::spu2::write(kSpuBase + 0x000u, 0x3FFFu, 2);            // voll
    ps2::iop::spu2::write(kSpuBase + 0x002u, 0x3FFFu, 2);            // volr
    ps2::iop::spu2::write(kSpuBase + 0x1C0u, kBank >> 16, 2);        // SSA high
    ps2::iop::spu2::write(kSpuBase + 0x1C2u, kBank & 0xFFFFu, 2);    // SSA low
    ps2::iop::spu2::write(kSpuBase + 0x1A0u, 0x0001u, 2);            // KON voice 0

    // Phase 1: the voice is fully programmed but SdInit(0) left MVOL at 0.
    // Reference behaviour (freesd.c InitCoreVolume flag 0), so the output must
    // be silent even though the voice is keyed and its envelope is running.
    run_batches(8);
    const int silent_peak = peak_since(0);
    std::printf("[spu2-mix] after SdInit(0) + keyed voice: %zu frames, peak %d\n",
                g_captured.size() / 2, silent_peak);
    if (silent_peak != 0) {
        std::fprintf(stderr,
                     "[FAIL] expected silence with master volume 0, got peak %d\n",
                     silent_peak);
        return 1;
    }

    // Phase 2: the game sets the master volume; the same voice must become
    // audible without any further programming.
    ps2::iop::spu2::write(kSpuBase + 0x760u, 0x3FFFu, 2); // MVOLL core 0
    ps2::iop::spu2::write(kSpuBase + 0x762u, 0x3FFFu, 2); // MVOLR core 0
    const std::size_t before = g_captured.size();
    run_batches(8);
    const int peak = peak_since(before);
    std::printf("[spu2-mix] after master volume: %zu frames total, peak %d\n",
                g_captured.size() / 2, peak);
    if (std::getenv("SPU2_MIX_DEBUG")) {
        std::printf("[spu2-mix] SSA hi=%04X lo=%04X pitch=%04X voll=%04X MVOL=%04X\n",
                    static_cast<unsigned>(ps2::iop::spu2::read(kSpuBase + 0x1C0u, 2) & 0xFFFFu),
                    static_cast<unsigned>(ps2::iop::spu2::read(kSpuBase + 0x1C2u, 2) & 0xFFFFu),
                    static_cast<unsigned>(ps2::iop::spu2::read(kSpuBase + 0x004u, 2) & 0xFFFFu),
                    static_cast<unsigned>(ps2::iop::spu2::read(kSpuBase + 0x000u, 2) & 0xFFFFu),
                    static_cast<unsigned>(ps2::iop::spu2::read(kSpuBase + 0x760u, 2) & 0xFFFFu));
        std::printf("[spu2-mix] bank at 0x%X (block0 | block8):", kBank);
        for (int i = 0; i < 8; ++i) std::printf(" %04X", ram[kBank + i]);
        std::printf(" |");
        for (int i = 0; i < 8; ++i) std::printf(" %04X", ram[kBank + 64 + i]);
        std::printf("\n[spu2-mix] first samples after MVOL:");
        for (std::size_t i = before; i < before + 32 && i < g_captured.size(); ++i)
            std::printf(" %d", static_cast<int>(g_captured[i]));
        std::printf("\n[spu2-mix] last samples:");
        for (std::size_t i = (g_captured.size() > 32 ? g_captured.size() - 32 : 0); i < g_captured.size(); ++i)
            std::printf(" %d", static_cast<int>(g_captured[i]));
        std::printf("\n");
    }
    if (peak < 4096) {
        std::fprintf(stderr,
                     "[FAIL] mixer output is silent with the master volume set "
                     "(peak %d): voice, ADPCM, envelope, or routing did not run\n",
                     peak);
        return 1;
    }
    std::printf("[PASS] SPU2 produced non-silent samples through the libsd bridge\n");
    return 0;
}
