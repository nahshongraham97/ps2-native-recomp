#include "mka/stage_a_guest_memory.hpp"
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
#include <utility>
#include <vector>

namespace t_001c4210 {
#include "../tests/generated_track_a/SLUS_214.10_001c4210_native.hpp"
}
namespace t_0016ae10 {
#include "../tests/generated_track_a/SLUS_214.10_0016ae10_native.hpp"
}
namespace t_002e1710 {
#include "../tests/generated_track_a/SLUS_214.10_002e1710_native.hpp"
}
namespace t_00167cf0 {
#include "../tests/generated_track_a/SLUS_214.10_00167cf0_native.hpp"
}

namespace {

struct StoreRec {
    std::uint32_t addr;
    std::uint32_t val;
};

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
    std::vector<StoreRec> stores;
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
    std::uint32_t sc = 0;
    in.read(reinterpret_cast<char *>(&sc), 4);
    cap.stores.resize(sc);
    if (sc > 0) {
        in.read(reinterpret_cast<char *>(cap.stores.data()), sc * sizeof(StoreRec));
    }
    return bool(in);
}

bool check_stores(const std::vector<StoreRec> &obs, const std::vector<StoreRec> &cap) {
    if (obs.size() != cap.size()) return false;
    for (std::size_t i = 0; i < obs.size(); ++i) {
        if (obs[i].addr != cap[i].addr || obs[i].val != cap[i].val) return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-return|--planted-store]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    const std::string plant = argc >= 3 ? argv[2] : "";
    unsigned p16ae = 0, p1c42 = 0, p2e17 = 0, r167c = 0;
    std::set<std::uint64_t> shapes_16ae;
    std::set<std::pair<std::uint64_t, std::uint64_t>> shapes_1c42;
    std::set<std::int64_t> shapes_2e17;

    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().filename().string().rfind("cap_", 0) != 0) continue;
        Capture cap;
        if (!load_capture(entry.path(), cap)) return 2;
        std::vector<StoreRec> obs_stores;
        mka::stage_a::GuestMemory mem(
            cap.entry_ram, cap.entry_spr,
            {&obs_stores, [](void *ctx, const mka::stage_a::Access &acc) {
                 static_cast<std::vector<StoreRec> *>(ctx)->push_back({acc.address, static_cast<std::uint32_t>(acc.value)});
             }});
        if (cap.pc == 0x0016AE10u) {
            t_0016ae10::kernel_in in{cap.entry_cpu.r[2].d[0], cap.entry_cpu.r[4].d[0], cap.entry_cpu.r[31].d[0]};
            auto out = t_0016ae10::kernel(mem, in, 64);
            if (plant == "--planted-return") out.r2 ^= 1ull;
            shapes_16ae.insert(out.r2);
            if (out.r2 != cap.exit_cpu.r[2].d[0] || out.budget != 59 || !check_stores(obs_stores, cap.stores)) {
                std::fprintf(stderr, "FAIL 0016AE10 mismatch in %s\n", entry.path().filename().string().c_str());
                return 1;
            }
            ++p16ae;
        } else if (cap.pc == 0x001C4210u) {
            t_001c4210::kernel_in in{
                cap.entry_cpu.r[2].d[0], cap.entry_cpu.r[3].d[0], cap.entry_cpu.r[4].d[0],
                cap.entry_cpu.r[5].d[0], cap.entry_cpu.r[6].d[0], cap.entry_cpu.r[28].d[0],
                cap.entry_cpu.r[31].d[0]};
            auto out = t_001c4210::kernel(mem, in, 64);
            if (plant == "--planted-return") out.r2 ^= 1ull;
            if (plant == "--planted-store") mem.store<std::uint32_t>(static_cast<std::uint32_t>(in.r6), 0xDEADBEEFu);
            std::uint32_t s0 = 0, s1 = 0;
            if (obs_stores.size() >= 2) { s0 = obs_stores[0].val; s1 = obs_stores[1].val; }
            shapes_1c42.insert({s0, s1});
            if (out.r2 != cap.exit_cpu.r[2].d[0] ||
                out.r3 != cap.exit_cpu.r[3].d[0] ||
                out.r4 != cap.exit_cpu.r[4].d[0] ||
                out.budget != 56 ||
                !check_stores(obs_stores, cap.stores)) {
                std::fprintf(stderr, "FAIL 001C4210 mismatch in %s\n", entry.path().filename().string().c_str());
                return 1;
            }
            ++p1c42;
        } else if (cap.pc == 0x002E1710u) {
            t_002e1710::kernel_in in{cap.entry_cpu.r[31].d[0]};
            auto out = t_002e1710::kernel(mem, in, 64);
            if (plant == "--planted-return") out.budget -= 1;
            shapes_2e17.insert(out.budget);
            if (out.budget != 62 || cap.entry_cpu.r[2].d[0] != cap.exit_cpu.r[2].d[0] || !check_stores(obs_stores, cap.stores)) {
                std::fprintf(stderr, "FAIL 002E1710 mismatch in %s\n", entry.path().filename().string().c_str());
                return 1;
            }
            ++p2e17;
        } else if (cap.pc == 0x00167CF0u) {
            t_00167cf0::kernel_in in{
                cap.entry_cpu.r[3].d[0], cap.entry_cpu.r[4].d[0], cap.entry_cpu.r[5].d[0],
                cap.entry_cpu.r[6].d[0], cap.entry_cpu.r[7].d[0], cap.entry_cpu.r[8].d[0],
                cap.entry_cpu.r[31].d[0]};
            auto out = t_00167cf0::kernel(mem, in, 256);
            if (out.r3 != cap.exit_cpu.r[3].d[0] || out.r4 != cap.exit_cpu.r[4].d[0] ||
                out.r5 != cap.exit_cpu.r[5].d[0] || out.r6 != cap.exit_cpu.r[6].d[0] ||
                out.r7 != cap.exit_cpu.r[7].d[0] || out.r8 != cap.exit_cpu.r[8].d[0] ||
                !check_stores(obs_stores, cap.stores)) {
                std::fprintf(stderr, "FAIL 00167CF0 mismatch in %s\n", entry.path().filename().string().c_str());
                return 1;
            }
            ++r167c;
        } else {
            continue;
        }
        if (std::memcmp(cap.entry_ram.data(), cap.exit_ram.data(), ps2::kRamSize) != 0) {
            std::fprintf(stderr, "FAIL RAM mismatch for 0x%08x\n", cap.pc);
            return 1;
        }
    }
    if (p16ae == 0 || p1c42 == 0 || p2e17 == 0 || r167c == 0) return 1;
    std::printf(
        "PASS_TRACK_A_TRANSFORMED_UNITS 001c4210=%u(shapes=%zu) 0016ae10=%u(shapes=%zu) 002e1710=%u(shapes=%zu) 00167cf0=%u\n",
        p1c42, shapes_1c42.size(), p16ae, shapes_16ae.size(), p2e17, shapes_2e17.size(), r167c);
    return 0;
}
