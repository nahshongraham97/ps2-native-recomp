#include "mka/phase7_fighter_native.hpp"
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

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-return|--planted-a2-store]\n", argv[0]);
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

    unsigned tested = 0, branch_degen = 0, branch_early_reject = 0;
    std::set<std::tuple<std::uint8_t, std::uint32_t>> behavioral_shapes;
    for (const auto &p : files) {
        Capture cap;
        if (!load_capture(p, cap)) return 2;
        std::vector<StoreRec> obs_stores;
        mka::stage_a::GuestMemory mem(
            cap.entry_ram, cap.entry_spr,
            {&obs_stores, [](void *ctx, const mka::stage_a::Access &acc) {
                 static_cast<std::vector<StoreRec> *>(ctx)->push_back({acc.address, static_cast<std::uint32_t>(acc.value)});
             }});
        ps2::Cpu cpu = cap.entry_cpu;
        std::uint32_t rad = 0;
        std::memcpy(&rad, &cap.entry_ram[cpu.r[4].w[0] + 32u], 4);

        auto res = mka::phase7::fighter_tri_plane_test_00194370(
            mem, cpu, cpu.r[4].w[0], cpu.r[5].w[0], cpu.r[6].w[0]);
        if (res.branch == mka::phase7::FighterTriBranch::degenerate_normal) ++branch_degen;
        else if (res.branch == mka::phase7::FighterTriBranch::early_reject) ++branch_early_reject;
        behavioral_shapes.insert({static_cast<std::uint8_t>(res.branch), rad});

        std::uint32_t ret = res.ret;
        if (plant == "--planted-return") ret ^= 1u;
        if (plant == "--planted-a2-store") mem.store<std::uint32_t>(cpu.r[6].w[0] + 12u, 1u);

        if (ret != cap.exit_cpu.r[2].w[0]) {
            std::fprintf(stderr, "FAIL 00194370 return mismatch in %s: %u vs %u\n",
                         p.filename().string().c_str(), ret, cap.exit_cpu.r[2].w[0]);
            return 1;
        }
        // Compare native stores in order against captured stores (excluding prologue callee-saves in [sp-128, sp))
        const std::uint32_t sp_lo = cap.entry_cpu.r[29].w[0] - 128u;
        const std::uint32_t sp_hi = cap.entry_cpu.r[29].w[0];
        std::vector<StoreRec> cap_non_stack, obs_non_stack;
        for (const auto &s : cap.stores) {
            if (s.addr >= sp_lo && s.addr < sp_hi) continue;
            cap_non_stack.push_back(s);
        }
        for (const auto &s : obs_stores) {
            if (s.addr >= sp_lo && s.addr < sp_hi) continue;
            obs_non_stack.push_back(s);
        }
        if (obs_non_stack.size() != cap_non_stack.size()) {
            std::fprintf(stderr, "FAIL 00194370 non-stack store count mismatch in %s: %zu vs %zu\n",
                         p.filename().string().c_str(), obs_non_stack.size(), cap_non_stack.size());
            return 1;
        }
        for (std::size_t i = 0; i < obs_non_stack.size(); ++i) {
            if (obs_non_stack[i].addr != cap_non_stack[i].addr || obs_non_stack[i].val != cap_non_stack[i].val) {
                std::fprintf(stderr, "FAIL 00194370 store[%zu] mismatch in %s: (0x%08x=0x%08x) vs (0x%08x=0x%08x)\n",
                             i, p.filename().string().c_str(),
                             obs_non_stack[i].addr, obs_non_stack[i].val,
                             cap_non_stack[i].addr, cap_non_stack[i].val);
                return 1;
            }
        }
        // Compare full 32 MiB RAM outside dead stack [sp-128, sp)
        for (std::uint32_t a = 0; a < ps2::kRamSize; ++a) {
            if (a >= sp_lo && a < sp_hi) continue;
            if (cap.entry_ram[a] != cap.exit_ram[a]) {
                std::fprintf(stderr, "FAIL 00194370 RAM mismatch at 0x%08X in %s\n", a, p.filename().string().c_str());
                return 1;
            }
        }
        ++tested;
    }

    if (tested == 0u) {
        std::fprintf(stderr, "FAIL 00194370 zero captures tested\n");
        return 1;
    }
    std::printf("PASS_PHASE7_FIGHTER_ACTIVE_00194370 captures=%u behavioral_shapes=%zu branch_degenerate=%u branch_early_reject=%u\n",
                tested, behavioral_shapes.size(), branch_degen, branch_early_reject);
    return 0;
}
