#include "mka/stage_a_guest_memory.hpp"
#include "mka/phase6_track_a.hpp"
#include "native_test_sha256.hpp"

#include <array>
#include <limits>
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
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    const auto length = in.tellg();
    constexpr std::uint64_t fixed_payload = 36 + 2 * (sizeof(ps2::Cpu) + ps2::kRamSize + ps2::kScratchpadSize) + 4;
    if (length < 0 || static_cast<std::uint64_t>(length) < fixed_payload + 32) return false;
    const auto payload_bytes = static_cast<std::uint64_t>(length) - 32;
    // Authenticate every serialized byte, including header/counts, before using it.
    in.seekg(0);
    NativeTestSha256 hash;
    std::array<char, 65536> chunk{};
    for (std::uint64_t remaining = payload_bytes; remaining;) {
        const auto count = static_cast<std::streamsize>(std::min<std::uint64_t>(remaining, chunk.size()));
        if (!in.read(chunk.data(), count)) return false;
        hash.add(chunk.data(), static_cast<std::size_t>(count));
        remaining -= count;
    }
    std::array<unsigned char, 32> trailer{};
    if (!in.read(reinterpret_cast<char *>(trailer.data()), trailer.size())) return false;
    constexpr char hex[] = "0123456789abcdef";
    std::string expected_hash;
    for (auto byte : trailer) { expected_hash.push_back(hex[byte >> 4]); expected_hash.push_back(hex[byte & 15]); }
    if (hash.finish() != expected_hash) return false;
    in.seekg(0);
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
    // Exact length bounds the multiplication and allocation; no ignored suffix.
    if (!in || payload_bytes != fixed_payload + std::uint64_t{sc} * sizeof(StoreRec)) return false;
    cap.stores.resize(sc);
    if (sc > 0) {
        in.read(reinterpret_cast<char *>(cap.stores.data()), sc * sizeof(StoreRec));
    }
    return bool(in) && static_cast<std::uint64_t>(in.tellg()) == payload_bytes;
}

// Test adapter only: native void has no scheduler or emulated state. Original
// JR RA / delay NOP control and instruction budget are verified separately.
bool stage_b_state_matches(const Capture &cap, ps2::Cpu observed,
                           const std::vector<std::byte> &ram,
                           const std::vector<std::byte> &spr,
                           unsigned instruction_count, const std::string &plant) {
    observed.pc = cap.entry_cpu.r[31].w[0];
    observed.budget = static_cast<std::int32_t>(static_cast<std::uint32_t>(cap.entry_cpu.budget) - instruction_count);
    if (plant == "--planted-stage-b-identity") observed.r[3].d[1] ^= 1;
    if (plant == "--planted-stage-b-control") observed.pc ^= 4;
    return std::memcmp(&observed, &cap.exit_cpu, sizeof(observed)) == 0 &&
           ram == cap.exit_ram && spr == cap.exit_spr && cap.stores.empty();
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
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-return|--planted-store|--planted-167cf0|--planted-stage-b-return|--planted-stage-b-identity|--planted-stage-b-control]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    const std::string plant = argc >= 3 ? argv[2] : "";
    if (argc > 3 || (!plant.empty() && plant != "--planted-return" && plant != "--planted-store" &&
        plant != "--planted-167cf0" && plant != "--planted-stage-b-return" &&
        plant != "--planted-stage-b-identity" && plant != "--planted-stage-b-control" && plant != "--planted-partial-exit" && plant != "--planted-stage-b-ram" && plant != "--planted-stage-b-spr")) return 2;
    unsigned stage_b_16ae_calls = 0, stage_b_2e17_calls = 0;
    unsigned p16ae = 0, p1c42 = 0, p2e17 = 0, r167c = 0;
    std::set<std::uint64_t> shapes_16ae;
    std::set<std::pair<std::uint64_t, std::uint64_t>> shapes_1c42;
    std::set<std::int64_t> shapes_2e17;
    std::set<std::tuple<std::uint64_t, std::uint64_t, std::uint64_t>> shapes_167c;

    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().filename().string().rfind("cap_", 0) != 0) continue;
        Capture cap;
        if (!load_capture(entry.path(), cap)) { std::fprintf(stderr, "FAIL capture authentication/format: %s\n", entry.path().string().c_str()); return 2; }
        if (plant == "--planted-partial-exit") cap.exit_cpu.pc ^= 4;
        if (cap.exit_cpu.pc != cap.entry_cpu.r[31].w[0] ||
            std::memcmp(&cap.entry_cpu.r[29], &cap.exit_cpu.r[29], sizeof(ps2::Reg128)) != 0) {
            std::fprintf(stderr, "FAIL incomplete ordinary-return boundary for 0x%08x\n", cap.pc); return 1;
        }
        std::vector<StoreRec> obs_stores;
        mka::stage_a::GuestMemory mem(
            cap.entry_ram, cap.entry_spr,
            {&obs_stores, [](void *ctx, const mka::stage_a::Access &acc) {
                 static_cast<std::vector<StoreRec> *>(ctx)->push_back({acc.address, static_cast<std::uint32_t>(acc.value)});
             }});
        if (cap.pc == 0x0016AE10u) {
            t_0016ae10::kernel_in in{cap.entry_cpu.r[2].d[0], cap.entry_cpu.r[4].d[0], cap.entry_cpu.r[31].d[0]};
            auto out = t_0016ae10::kernel(mem, in, 64);
            const auto object_address = std::uint64_t{cap.entry_cpu.r[4].w[0]} + 20u;
            if (object_address >= cap.entry_ram.size()) return 2;
            const mka::phase6_track_a::NativeObj16AE10 sb_obj{static_cast<std::uint8_t>(cap.entry_ram[object_address])};
            auto sb_r2 = mka::phase6_track_a::fn_0016ae10_stage_b(sb_obj);
            ++stage_b_16ae_calls;
            if (plant == "--planted-stage-b-return") sb_r2 ^= 1;
            auto sb_cpu = cap.entry_cpu;
            sb_cpu.r[2].d[0] = sb_r2;
            if (!stage_b_state_matches(cap, sb_cpu, cap.entry_ram, cap.entry_spr, 5, "")) {
                std::fprintf(stderr, "FAIL 0016AE10 Stage B whole-state/control mismatch\n"); return 1;
            }
            if (plant == "--planted-return") out.r2 ^= 1ull;
            shapes_16ae.insert(out.r2);
            if (out.r2 != cap.exit_cpu.r[2].d[0] || sb_r2 != cap.exit_cpu.r[2].d[0] || out.budget != 59 || !check_stores(obs_stores, cap.stores)) {
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
            auto sb_ram = cap.entry_ram;
            auto sb_spr = cap.entry_spr;
            mka::phase6_track_a::fn_002e1710_stage_b();
            ++stage_b_2e17_calls;
            if (plant == "--planted-stage-b-ram") sb_ram.back() ^= std::byte{1};
            if (plant == "--planted-stage-b-spr") sb_spr.back() ^= std::byte{1};
            // Capture the actual native call's preserved whole state. No single
            // return register or a successful void expression establishes this.
            if (!stage_b_state_matches(cap, cap.entry_cpu, sb_ram, sb_spr, 2, plant)) {
                std::fprintf(stderr, "FAIL 002E1710 Stage B whole-state/control mismatch\n"); return 1;
            }
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
            if (plant == "--planted-167cf0") out.r3 ^= 1ull;
            shapes_167c.insert({out.r3, out.r4, out.r5});
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
        if (cap.entry_ram != cap.exit_ram || cap.entry_spr != cap.exit_spr) {
            std::fprintf(stderr, "FAIL RAM/scratchpad mismatch for 0x%08x\n", cap.pc);
            return 1;
        }
    }
    if (p16ae != 30 || p1c42 != 30 || p2e17 != 30 || r167c != 6 || stage_b_16ae_calls != 30 || stage_b_2e17_calls != 30) return 1;
    std::printf(
        "PASS_TRACK_A_TRANSFORMED_UNITS 001c4210=%u(shapes=%zu) 0016ae10=%u(StageA+StageB,shapes=%zu) 002e1710=%u(StageA+StageB,shapes=%zu) 00167cf0_control_mode=%u(shapes=%zu)\n",
        p1c42, shapes_1c42.size(), p16ae, shapes_16ae.size(), p2e17, shapes_2e17.size(), r167c, shapes_167c.size());
    return 0;
}
