// Test-only extraction through the actual runtime serializer, never tag scans.
// Link against the pinned R192 runtime; do not boot, mount media/cards or run().
#include "ps2/system.hpp"
#include "iop/iop.hpp"
#include "iop/kernel.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

static_assert(sizeof(ps2::iop::Cpu) == 156, "Requires the frozen R192 IOP Cpu ABI");
static_assert(offsetof(ps2::iop::Cpu, pc) == 136);
static_assert(offsetof(ps2::iop::Cpu, status) == 144);

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    const std::filesystem::path output(argv[2]);
    if (std::filesystem::exists(output) || !std::filesystem::create_directories(output)) return 3;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 4;
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
    if (bytes.empty()) return 5;
    // The byte overload calls machine_state(Reader) only. The path overload's
    // quick/card note is intentionally not used. Default memcard path is empty.
    if (!ps2::g_options.memcard_path.empty()) return 6;
    // Actual SIF state rebinding requires the original host RPC registrations.
    // install_sif registers callbacks only; it neither runs IOP instructions nor
    // mounts a disc/card. No placeholder RPC callbacks are invented here.
    ps2::iop::kernel::install_sif();
    if (!ps2::load_state(bytes.data(), bytes.size())) return 7;
    if (!ps2::g_options.memcard_path.empty()) return 8;
    const auto write = [&](const char *name, const void *data, std::size_t size) {
        std::ofstream stream(output/name, std::ios::binary);
        stream.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        return static_cast<bool>(stream);
    };
    if (!write("iop-cpu.bin", &ps2::iop::g_cpu, sizeof(ps2::iop::g_cpu)) ||
        !write("iop-ram.bin", ps2::iop::g_ram, ps2::iop::kRamSize)) return 9;
    std::ofstream meta(output/"modules.txt");
    for (const auto &[id, module] : ps2::iop::kernel::modules())
        meta << id << ' ' << module.name << ' ' << std::hex << module.base << ' '
             << module.size << ' ' << module.entry << ' ' << module.gp << '\n';
    if (!meta) return 10;
    std::printf("frame=%llu iop_pc=%08X iop_cpu_size=%zu iop_ram_size=%u modules=%zu no_boot_or_execution=1 no_mounted_card=1\n",
                static_cast<unsigned long long>(ps2::frame_count()), ps2::iop::g_cpu.pc,
                sizeof(ps2::iop::g_cpu), ps2::iop::kRamSize, ps2::iop::kernel::modules().size());
    return 0;
}
