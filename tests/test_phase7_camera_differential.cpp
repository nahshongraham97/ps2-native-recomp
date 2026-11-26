#include "mka/phase7_camera_native.hpp"
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
    bool operator==(const StoreRec &) const = default;
};

struct Capture {
    std::uint32_t pc{};
    std::uint32_t entry{};
    std::uint64_t frame{};
    ps2::Cpu entry_cpu{};
    ps2::Cpu exit_cpu{};
    std::vector<std::byte> entry_ram;
    std::vector<std::byte> entry_spr;
    std::vector<std::byte> exit_ram;
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
    cap.entry_spr.resize(spr_sz);
    cap.exit_ram.resize(ram_sz);
    cap.exit_spr.resize(spr_sz);

    in.read(reinterpret_cast<char *>(&cap.entry_cpu), cpu_sz);
    in.read(reinterpret_cast<char *>(cap.entry_ram.data()), ram_sz);
    in.read(reinterpret_cast<char *>(cap.entry_spr.data()), spr_sz);
    in.read(reinterpret_cast<char *>(&cap.exit_cpu), cpu_sz);
    in.read(reinterpret_cast<char *>(cap.exit_ram.data()), ram_sz);
    in.read(reinterpret_cast<char *>(cap.exit_spr.data()), spr_sz);

    std::uint32_t scount = 0;
    in.read(reinterpret_cast<char *>(&scount), 4);
    cap.stores.resize(scount);
    if (scount > 0) in.read(reinterpret_cast<char *>(cap.stores.data()), scount * sizeof(StoreRec));
    return bool(in);
}

std::vector<StoreRec> g_observed;

void observe_store(void *, const mka::stage_a::Access &a) {
    g_observed.push_back({a.address, static_cast<std::uint32_t>(a.value)});
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-window|--planted-reciprocal|--planted-store-order|--planted-frame-flags|--planted-return]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path root(argv[1]);
    const std::string plant = argc >= 3 ? argv[2] : "";

    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().filename().string().rfind("cap_", 0) == 0) {
            files.push_back(entry.path());
        }
    }
    if (files.empty()) {
        std::fprintf(stderr, "FAIL: no capture files found under %s\n", root.string().c_str());
        return 2;
    }

    unsigned cam_tested = 0, frame_tested = 0;
    std::set<std::tuple<std::uint32_t, std::uint32_t, bool>> cam_shapes;
    std::set<std::tuple<std::uint8_t, std::uint8_t>> frame_shapes;

    for (const auto &p : files) {
        Capture cap;
        if (!load_capture(p, cap)) {
            std::fprintf(stderr, "FAIL: could not load %s\n", p.string().c_str());
            return 2;
        }
        if (cap.pc != 0x001A64A0u && cap.pc != 0x001A7510u) continue;

        std::vector<std::byte> ram = cap.entry_ram;
        std::vector<std::byte> spr = cap.entry_spr;
        g_observed.clear();
        mka::stage_a::GuestMemory mem(ram, spr, {nullptr, observe_store});

        if (cap.pc == 0x001A64A0u) {
            const std::uint32_t cam = cap.entry_cpu.r[4].w[0];
            const std::uint32_t win = cap.entry_cpu.r[5].w[0];
            const std::uint32_t sp = cap.entry_cpu.r[29].w[0];
            std::uint32_t wx = 0, wy = 0, parent = 0;
            std::memcpy(&wx, &cap.entry_ram[win], 4);
            std::memcpy(&wy, &cap.entry_ram[win + 4], 4);
            std::memcpy(&parent, &cap.entry_ram[cam + 4], 4);
            cam_shapes.insert({wx, wy, parent != 0});

            std::uint32_t fcr31 = cap.entry_cpu.fcr31;
            std::uint32_t v0 = mka::phase7::camera_set_view_window_001a64a0(mem, cam, win, fcr31);

            if (plant == "--planted-window") mem.store<std::uint32_t>(cam + 104u, wx ^ 1u);
            if (plant == "--planted-reciprocal") mem.store<std::uint32_t>(cam + 112u, 0x3F800000u);
            if (plant == "--planted-store-order" && g_observed.size() >= 2) std::swap(g_observed[0], g_observed[1]);
            if (plant == "--planted-return") v0 ^= 4u;

            if (v0 != cap.exit_cpu.r[2].w[0]) {
                std::fprintf(stderr, "FAIL 001A64A0 v0 mismatch in %s: %08X vs %08X\n", p.filename().string().c_str(), v0, cap.exit_cpu.r[2].w[0]);
                return 1;
            }
            if (fcr31 != cap.exit_cpu.fcr31) {
                std::fprintf(stderr, "FAIL 001A64A0 fcr31 mismatch in %s: %08X vs %08X\n", p.filename().string().c_str(), fcr31, cap.exit_cpu.fcr31);
                return 1;
            }
            // Compare full RAM except the 32-byte guest prologue stack frame [sp-32, sp)
            const std::uint32_t frame_lo = sp - 32u;
            const std::uint32_t frame_hi = sp;
            const bool ram_ok =
                std::memcmp(ram.data(), cap.exit_ram.data(), frame_lo) == 0 &&
                std::memcmp(ram.data() + frame_hi, cap.exit_ram.data() + frame_hi, ps2::kRamSize - frame_hi) == 0;
            if (!ram_ok || std::memcmp(spr.data(), cap.exit_spr.data(), ps2::kScratchpadSize) != 0) {
                std::fprintf(stderr, "FAIL 001A64A0 RAM/SPR mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            std::vector<StoreRec> expected_stores;
            for (const auto &s : cap.stores) {
                if (s.addr >= frame_lo && s.addr < frame_hi) continue;
                expected_stores.push_back(s);
            }
            if (g_observed != expected_stores) {
                std::fprintf(stderr, "FAIL 001A64A0 store-order mismatch in %s (obs=%zu exp=%zu)\n",
                             p.filename().string().c_str(), g_observed.size(), expected_stores.size());
                return 1;
            }
            ++cam_tested;
        } else if (cap.pc == 0x001A7510u) {
            const std::uint32_t frame_addr = cap.entry_cpu.r[4].w[0];
            std::uint32_t root_addr = 0;
            std::memcpy(&root_addr, &cap.entry_ram[frame_addr + 160u], 4);
            const std::uint8_t rf = static_cast<std::uint8_t>(cap.entry_ram[root_addr + 3u]);
            const std::uint8_t sf = static_cast<std::uint8_t>(cap.entry_ram[frame_addr + 3u]);
            frame_shapes.insert({static_cast<std::uint8_t>(rf & 3u), sf});

            std::uint32_t v0 = mka::phase7::frame_update_objects_001a7510(mem, frame_addr);

            if (plant == "--planted-frame-flags") mem.store<std::uint8_t>(frame_addr + 3u, 0u);
            if (plant == "--planted-store-order" && g_observed.size() >= 2) std::swap(g_observed[0], g_observed[1]);
            if (plant == "--planted-return") v0 ^= 4u;

            if (v0 != cap.exit_cpu.r[2].w[0]) {
                std::fprintf(stderr, "FAIL 001A7510 v0 mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            if (std::memcmp(ram.data(), cap.exit_ram.data(), ps2::kRamSize) != 0 ||
                std::memcmp(spr.data(), cap.exit_spr.data(), ps2::kScratchpadSize) != 0) {
                std::fprintf(stderr, "FAIL 001A7510 RAM/SPR mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            if (g_observed != cap.stores) {
                std::fprintf(stderr, "FAIL 001A7510 store-order mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            ++frame_tested;
        }
    }

    std::printf("PASS_PHASE7_CAMERA_DIFFERENTIAL cam_001a64a0_states=%u cam_distinct_shapes=%zu frame_001a7510_states=%u frame_distinct_shapes=%zu\n",
                cam_tested, cam_shapes.size(), frame_tested, frame_shapes.size());
    return 0;
}
