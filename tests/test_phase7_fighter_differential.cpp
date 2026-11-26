#include "mka/phase7_fighter_native.hpp"
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
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
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-return]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    const std::string plant = argc >= 3 ? argv[2] : "";

    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().filename().string().rfind("cap_00194370_", 0) == 0) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) return 2;

    unsigned tested = 0;
    for (const auto &p : files) {
        Capture cap;
        if (!load_capture(p, cap)) return 2;
        mka::stage_a::GuestMemory mem(cap.entry_ram, cap.entry_spr);
        const std::uint32_t a0 = cap.entry_cpu.r[4].w[0];
        const std::uint32_t a1 = cap.entry_cpu.r[5].w[0];
        if (a0 != 0u || a1 != 0u) {
            std::fprintf(stderr, "SKIP non-null path in %s: a0=0x%08x a1=0x%08x\n",
                         p.filename().string().c_str(), a0, a1);
            continue;
        }
        std::uint32_t ret = mka::phase7::fighter_state_null_probe_00194370(mem, a0, a1);
        if (plant == "--planted-return") ret ^= 1u;
        if (ret != cap.exit_cpu.r[2].w[0]) {
            std::fprintf(stderr, "FAIL 00194370 return mismatch in %s: %u vs %u\n",
                         p.filename().string().c_str(), ret, cap.exit_cpu.r[2].w[0]);
            return 1;
        }
        ++tested;
    }

    std::printf("PASS_PHASE7_FIGHTER_NULL_PROBE probe_00194370=%u\n", tested);
    return tested == 0u ? 1 : 0;
}
