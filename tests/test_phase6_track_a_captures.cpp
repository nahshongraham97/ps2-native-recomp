#include "mka/stage_a_guest_memory.hpp"
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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
    return bool(in);
}

std::uint32_t fn_0016ae10(mka::stage_a::GuestMemory &mem, std::uint32_t a0) {
    const std::uint8_t b = mem.load<std::uint8_t>(a0 + 20u);
    return (b >> 5) & 1u;
}

std::uint32_t fn_001c4210(mka::stage_a::GuestMemory &mem, std::uint32_t gp, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const std::uint32_t base = mem.load<std::uint32_t>(gp - 23200u);
    const std::uint32_t ptr = a0 + base;
    mem.store<std::uint32_t>(a2, mem.load<std::uint32_t>(ptr + 8u));
    mem.store<std::uint32_t>(a1, mem.load<std::uint32_t>(ptr + 12u));
    return a0;
}

void fn_002e1710() {}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    const std::filesystem::path root(argv[1]);
    unsigned p16ae = 0, p1c42 = 0, p2e17 = 0;

    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().filename().string().rfind("cap_", 0) != 0) continue;
        Capture cap;
        if (!load_capture(entry.path(), cap)) return 2;
        mka::stage_a::GuestMemory mem(cap.entry_ram, cap.entry_spr);
        if (cap.pc == 0x0016AE10u) {
            const std::uint32_t v0 = fn_0016ae10(mem, cap.entry_cpu.r[4].w[0]);
            if (v0 != cap.exit_cpu.r[2].w[0]) return 1;
            ++p16ae;
        } else if (cap.pc == 0x001C4210u) {
            const std::uint32_t v0 = fn_001c4210(mem, cap.entry_cpu.r[28].w[0], cap.entry_cpu.r[4].w[0], cap.entry_cpu.r[5].w[0], cap.entry_cpu.r[6].w[0]);
            if (v0 != cap.exit_cpu.r[2].w[0]) return 1;
            ++p1c42;
        } else if (cap.pc == 0x002E1710u) {
            fn_002e1710();
            ++p2e17;
        } else {
            continue;
        }
        if (std::memcmp(cap.entry_ram.data(), cap.exit_ram.data(), ps2::kRamSize) != 0) {
            std::fprintf(stderr, "FAIL RAM mismatch for 0x%08x\n", cap.pc);
            return 1;
        }
    }
    if (p16ae == 0 || p1c42 == 0 || p2e17 == 0) return 1;
    std::printf("PASS_TRACK_A_CAPTURES 001c4210=%u 0016ae10=%u 002e1710=%u\n", p1c42, p16ae, p2e17);
    return 0;
}
