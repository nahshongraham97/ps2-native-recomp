#include "mka/phase7_input_native.hpp"
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

struct Capture {
    std::uint32_t pc{};
    std::uint32_t entry{};
    std::uint64_t frame{};
    ps2::Cpu entry_cpu{};
    ps2::Cpu exit_cpu{};
    std::vector<std::byte> entry_ram;
    std::vector<std::byte> exit_ram;
    std::vector<std::byte> entry_spr;
    std::vector<std::byte> exit_spr;
    std::uint32_t store_count{};
};

bool load_capture(const std::filesystem::path &path, Capture &cap) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char magic[8]{};
    in.read(magic, 8);
    if (std::memcmp(magic, "PS2CAP01", 8) != 0) return false;
    std::uint32_t cpu_sz = 0, ram_sz = 0, spr_sz = 0;
    in.read(reinterpret_cast<char *>(&cap.pc), 4);
    in.read(reinterpret_cast<char *>(&cap.entry), 4);
    in.read(reinterpret_cast<char *>(&cap.frame), 8);
    in.read(reinterpret_cast<char *>(&cpu_sz), 4);
    in.read(reinterpret_cast<char *>(&ram_sz), 4);
    in.read(reinterpret_cast<char *>(&spr_sz), 4);
    if (cpu_sz != sizeof(ps2::Cpu) || ram_sz != ps2::kRamSize || spr_sz != ps2::kScratchpadSize) return false;
    cap.entry_ram.resize(ram_sz);
    cap.exit_ram.resize(ram_sz);
    cap.entry_spr.resize(spr_sz);
    cap.exit_spr.resize(spr_sz);
    in.read(reinterpret_cast<char *>(&cap.entry_cpu), cpu_sz);
    in.read(reinterpret_cast<char *>(cap.entry_ram.data()), ram_sz);
    in.read(reinterpret_cast<char *>(cap.entry_spr.data()), spr_sz);
    in.read(reinterpret_cast<char *>(&cap.exit_cpu), cpu_sz);
    in.read(reinterpret_cast<char *>(cap.exit_ram.data()), ram_sz);
    in.read(reinterpret_cast<char *>(cap.exit_spr.data()), spr_sz);
    in.read(reinterpret_cast<char *>(&cap.store_count), 4);
    return bool(in);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-store]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    const std::string plant = argc >= 3 ? argv[2] : "";

    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().filename().string().rfind("cap_001637d0_", 0) == 0) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) return 2;

    unsigned idle_tested = 0, active_seen = 0;
    std::set<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>> slot_flag_shapes;

    for (const auto &p : files) {
        Capture cap;
        if (!load_capture(p, cap)) return 2;

        mka::stage_a::GuestMemory mem(cap.entry_ram, cap.entry_spr);
        const std::uint8_t f0 = mem.load<std::uint8_t>(0x00660080u + 460u);
        const std::uint8_t f1 = mem.load<std::uint8_t>(0x00660080u + 32u + 460u);
        const std::uint8_t f2 = mem.load<std::uint8_t>(0x00660080u + 64u + 460u);
        slot_flag_shapes.insert({f0, f1, f2});

        std::uint32_t hits = mka::phase7::pad_scan_001637d0(mem, [&](std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {});
        if (plant == "--planted-store") {
            mem.store<std::uint32_t>(0x00660080u, 0xDEADBEEFu);
        }

        if (hits > 0u) {
            std::fprintf(stderr, "FAIL 001637D0 active slot path unimplemented in %s: hits=%u\n",
                         p.filename().string().c_str(), hits);
            return 1;
        }
        // Check full 32 MiB RAM equivalence outside dead prologue stack [sp-96, sp)
        const std::uint32_t sp_lo = cap.entry_cpu.r[29].w[0] - 96u;
        const std::uint32_t sp_hi = cap.entry_cpu.r[29].w[0];
        for (std::uint32_t a = 0; a < ps2::kRamSize; ++a) {
            if (a >= sp_lo && a < sp_hi) continue;
            if (cap.entry_ram[a] != cap.exit_ram[a]) {
                std::fprintf(stderr, "FAIL 001637D0 RAM mismatch at 0x%08X in %s\n", a, p.filename().string().c_str());
                return 1;
            }
        }
        ++idle_tested;
    }

    if (idle_tested == 0u) {
        std::fprintf(stderr, "FAIL 001637D0 zero idle captures tested\n");
        return 1;
    }
    std::printf("PASS_PHASE7_INPUT_IDLE_ONLY idle_captures=%u active_captures=%u slot_flag_shapes=%zu\n",
                idle_tested, active_seen, slot_flag_shapes.size());
    return 0;
}
