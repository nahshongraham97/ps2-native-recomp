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

struct Capture {
    std::uint32_t pc{};
    std::uint32_t entry{};
    std::uint64_t frame{};
    ps2::Cpu entry_cpu{};
    ps2::Cpu exit_cpu{};
    std::vector<std::byte> entry_ram;
    std::vector<std::byte> exit_ram;
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
    in.read(reinterpret_cast<char *>(&cap.entry_cpu), cpu_sz);
    in.read(reinterpret_cast<char *>(cap.entry_ram.data()), ram_sz);
    in.seekg(spr_sz, std::ios::cur);
    in.read(reinterpret_cast<char *>(&cap.exit_cpu), cpu_sz);
    in.read(reinterpret_cast<char *>(cap.exit_ram.data()), ram_sz);
    return bool(in);
}

std::uint32_t rd32(const std::vector<std::byte> &ram, std::uint32_t addr) {
    std::uint32_t v = 0;
    std::memcpy(&v, &ram[addr], 4);
    return v;
}

std::uint8_t rd8(const std::vector<std::byte> &ram, std::uint32_t addr) {
    return static_cast<std::uint8_t>(ram[addr]);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <capture_root> [--planted-window|--planted-reciprocal|--planted-frame-flags|--planted-dirty-link|--planted-dirty-next|--planted-dirty-prev|--planted-head-prev|--planted-dirty-check|--planted-return]\n", argv[0]);
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
    if (files.empty()) return 2;

    unsigned cam_tested = 0, frame_tested = 0, dirty_tested = 0;
    unsigned frame_clean_root = 0, frame_dirty_root = 0;
    unsigned frame_empty_list = 0, frame_populated_list = 0;
    unsigned cam_parent_null = 0, cam_parent_clean_root = 0, cam_parent_dirty_root = 0;
    unsigned cam_parent_empty_list = 0, cam_parent_populated_list = 0;
    std::set<std::tuple<std::uint32_t, std::uint32_t, bool>> cam_shapes;
    std::set<std::tuple<std::uint8_t, std::uint8_t>> frame_shapes;

    for (const auto &p : files) {
        Capture cap;
        if (!load_capture(p, cap)) return 2;

        if (cap.pc == 0x001A70F0u) {
            const std::uint32_t f_addr = cap.entry_cpu.r[4].w[0];
            const std::uint32_t root_addr = rd32(cap.entry_ram, f_addr + 160u);
            mka::phase7::NativeRwFrame root_obj{};
            root_obj.private_flags = rd8(cap.entry_ram, root_addr + 3u);
            mka::phase7::NativeRwFrame frame_obj{};
            frame_obj.private_flags = rd8(cap.entry_ram, f_addr + 3u);
            frame_obj.root = (root_addr == f_addr) ? &frame_obj : &root_obj;

            std::uint8_t res = mka::phase7::frame_dirty_stage_b(frame_obj);
            if (plant == "--planted-dirty-check") res ^= 1u;
            if (res != cap.exit_cpu.r[2].w[0]) {
                std::fprintf(stderr, "FAIL StageB 001A70F0 mismatch in %s: %u vs %u\n", p.filename().string().c_str(), res, cap.exit_cpu.r[2].w[0]);
                return 1;
            }
            ++dirty_tested;
        } else if (cap.pc == 0x001A7510u) {
            const std::uint32_t f_addr = cap.entry_cpu.r[4].w[0];
            const std::uint32_t root_addr = rd32(cap.entry_ram, f_addr + 160u);
            const std::uint32_t head_addr = rd32(cap.entry_ram, 0x0069F34Cu);
            const bool empty_list = (head_addr == 0x0069F34Cu);

            mka::phase7::NativeRwFrame root_obj{};
            root_obj.private_flags = rd8(cap.entry_ram, root_addr + 3u);
            mka::phase7::NativeRwFrame frame_obj{};
            frame_obj.private_flags = rd8(cap.entry_ram, f_addr + 3u);
            frame_obj.root = (root_addr == f_addr) ? &frame_obj : &root_obj;
            mka::phase7::NativeRwFrame *const actual_root = frame_obj.root;
            const std::uint8_t entry_root_flags = actual_root->private_flags;

            mka::phase7::NativeRwFrame sentinel_obj{};
            mka::phase7::NativeRwFrame old_head_obj{};
            mka::phase7::NativeRwFrame *const initial_head = empty_list ? &sentinel_obj : &old_head_obj;
            mka::phase7::NativeRwDirtyList dirty_list{};
            dirty_list.head = initial_head;
            dirty_list.sentinel = &sentinel_obj;

            frame_shapes.insert({static_cast<std::uint8_t>(entry_root_flags & 3u), frame_obj.private_flags});

            auto *ret = mka::phase7::frame_update_objects_stage_b(frame_obj, dirty_list);
            if (plant == "--planted-frame-flags") frame_obj.private_flags = 0u;
            if (plant == "--planted-dirty-link") dirty_list.head = nullptr;
            if (plant == "--planted-dirty-next") actual_root->dirty_next = &frame_obj + 1;
            if (plant == "--planted-dirty-prev") actual_root->dirty_prev = nullptr;
            if (plant == "--planted-head-prev") initial_head->dirty_prev = nullptr;
            if (plant == "--planted-return") ret = nullptr;

            if (ret != &frame_obj || cap.exit_cpu.r[2].w[0] != f_addr) {
                std::fprintf(stderr, "FAIL StageB 001A7510 return mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            if (frame_obj.private_flags != rd8(cap.exit_ram, f_addr + 3u) ||
                actual_root->private_flags != rd8(cap.exit_ram, root_addr + 3u)) {
                std::fprintf(stderr, "FAIL StageB 001A7510 flags mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            const std::uint32_t exit_head = rd32(cap.exit_ram, 0x0069F34Cu);
            const std::uint32_t exit_root_next = rd32(cap.exit_ram, root_addr + 8u);
            const std::uint32_t exit_root_prev = rd32(cap.exit_ram, root_addr + 12u);
            const std::uint32_t exit_head_prev = rd32(cap.exit_ram, head_addr + 4u);
            if ((entry_root_flags & 3u) == 0u) {
                ++frame_clean_root;
                if (empty_list) ++frame_empty_list;
                else ++frame_populated_list;
                if (actual_root->dirty_next != initial_head || exit_root_next != head_addr) {
                    std::fprintf(stderr, "FAIL StageB 001A7510 dirty_next / root+8 mismatch in %s\n", p.filename().string().c_str());
                    return 1;
                }
                if (actual_root->dirty_prev != &dirty_list.head || exit_root_prev != 0x0069F34Cu) {
                    std::fprintf(stderr, "FAIL StageB 001A7510 dirty_prev / root+12 mismatch in %s\n", p.filename().string().c_str());
                    return 1;
                }
                if (initial_head->dirty_prev != &actual_root->dirty_next || exit_head_prev != root_addr + 8u) {
                    std::fprintf(stderr, "FAIL StageB 001A7510 head+4 dirty_prev mismatch in %s\n", p.filename().string().c_str());
                    return 1;
                }
                if (dirty_list.head != actual_root || exit_head != root_addr + 8u) {
                    std::fprintf(stderr, "FAIL StageB 001A7510 dirty head mismatch in %s\n", p.filename().string().c_str());
                    return 1;
                }
            } else {
                ++frame_dirty_root;
                if (actual_root->dirty_next != nullptr || exit_root_next != rd32(cap.entry_ram, root_addr + 8u) ||
                    actual_root->dirty_prev != nullptr || exit_root_prev != rd32(cap.entry_ram, root_addr + 12u) ||
                    initial_head->dirty_prev != nullptr || exit_head_prev != rd32(cap.entry_ram, head_addr + 4u) ||
                    dirty_list.head != initial_head || exit_head != head_addr) {
                    std::fprintf(stderr, "FAIL StageB 001A7510 dirty list mutated when already dirty in %s\n", p.filename().string().c_str());
                    return 1;
                }
            }
            ++frame_tested;
        } else if (cap.pc == 0x001A64A0u) {
            const std::uint32_t cam_addr = cap.entry_cpu.r[4].w[0];
            const std::uint32_t win_addr = cap.entry_cpu.r[5].w[0];
            const std::uint32_t parent_addr = rd32(cap.entry_ram, cam_addr + 4u);

            mka::phase7::NativeRwV2d win{rd32(cap.entry_ram, win_addr), rd32(cap.entry_ram, win_addr + 4u)};
            cam_shapes.insert({win.x_bits, win.y_bits, parent_addr != 0u});

            mka::phase7::NativeRwFrame parent_obj{}, parent_root_obj{}, sentinel_obj{}, old_head_obj{};
            mka::phase7::NativeRwDirtyList dirty_list{};
            std::uint32_t root_addr = 0;
            std::uint32_t head_addr = 0;
            std::uint8_t entry_parent_root_flags = 0;
            mka::phase7::NativeRwFrame *initial_head = nullptr;
            if (parent_addr != 0u) {
                root_addr = rd32(cap.entry_ram, parent_addr + 160u);
                parent_root_obj.private_flags = rd8(cap.entry_ram, root_addr + 3u);
                parent_obj.private_flags = rd8(cap.entry_ram, parent_addr + 3u);
                parent_obj.root = (root_addr == parent_addr) ? &parent_obj : &parent_root_obj;
                entry_parent_root_flags = parent_obj.root->private_flags;
                head_addr = rd32(cap.entry_ram, 0x0069F34Cu);
                initial_head = (head_addr == 0x0069F34Cu) ? &sentinel_obj : &old_head_obj;
                dirty_list.head = initial_head;
                dirty_list.sentinel = &sentinel_obj;
            }

            mka::phase7::NativeRwCamera cam{};
            cam.parent = (parent_addr != 0u) ? &parent_obj : nullptr;
            std::uint32_t fcr31 = cap.entry_cpu.fcr31;
            auto *ret = mka::phase7::camera_set_view_window_stage_b(cam, win, dirty_list, fcr31);

            if (plant == "--planted-window") cam.view_window.x_bits ^= 1u;
            if (plant == "--planted-reciprocal") cam.inverse_window.x_bits = 0x3F800000u;
            if (plant == "--planted-return") ret = nullptr;

            if (ret != &cam || cap.exit_cpu.r[2].w[0] != cam_addr || fcr31 != cap.exit_cpu.fcr31) {
                std::fprintf(stderr, "FAIL StageB 001A64A0 return/fcr31 mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            if (cam.view_window.x_bits != rd32(cap.exit_ram, cam_addr + 104u) ||
                cam.view_window.y_bits != rd32(cap.exit_ram, cam_addr + 108u) ||
                cam.inverse_window.x_bits != rd32(cap.exit_ram, cam_addr + 112u) ||
                cam.inverse_window.y_bits != rd32(cap.exit_ram, cam_addr + 116u)) {
                std::fprintf(stderr, "FAIL StageB 001A64A0 window/inverse mismatch in %s\n", p.filename().string().c_str());
                return 1;
            }
            if (parent_addr != 0u) {
                auto *const p_root = parent_obj.root;
                if (parent_obj.private_flags != rd8(cap.exit_ram, parent_addr + 3u) ||
                    p_root->private_flags != rd8(cap.exit_ram, root_addr + 3u)) {
                    std::fprintf(stderr, "FAIL StageB 001A64A0 parent flags mismatch in %s\n", p.filename().string().c_str());
                    return 1;
                }
                const std::uint32_t exit_head = rd32(cap.exit_ram, 0x0069F34Cu);
                const std::uint32_t exit_root_next = rd32(cap.exit_ram, root_addr + 8u);
                const std::uint32_t exit_root_prev = rd32(cap.exit_ram, root_addr + 12u);
                const std::uint32_t exit_head_prev = rd32(cap.exit_ram, head_addr + 4u);
                if ((entry_parent_root_flags & 3u) == 0u) {
                    ++cam_parent_clean_root;
                    if (head_addr == 0x0069F34Cu) ++cam_parent_empty_list;
                    else ++cam_parent_populated_list;
                    if (p_root->dirty_next != initial_head || exit_root_next != head_addr ||
                        p_root->dirty_prev != &dirty_list.head || exit_root_prev != 0x0069F34Cu ||
                        initial_head->dirty_prev != &p_root->dirty_next || exit_head_prev != root_addr + 8u ||
                        dirty_list.head != p_root || exit_head != root_addr + 8u) {
                        std::fprintf(stderr, "FAIL StageB 001A64A0 parent dirty list mismatch in %s\n", p.filename().string().c_str());
                        return 1;
                    }
                } else {
                    ++cam_parent_dirty_root;
                    if (p_root->dirty_next != nullptr || exit_root_next != rd32(cap.entry_ram, root_addr + 8u) ||
                        p_root->dirty_prev != nullptr || exit_root_prev != rd32(cap.entry_ram, root_addr + 12u) ||
                        initial_head->dirty_prev != nullptr || exit_head_prev != rd32(cap.entry_ram, head_addr + 4u) ||
                        dirty_list.head != initial_head || exit_head != head_addr) {
                        std::fprintf(stderr, "FAIL StageB 001A64A0 parent dirty list mutated when dirty in %s\n", p.filename().string().c_str());
                        return 1;
                    }
                }
            } else {
                ++cam_parent_null;
            }
            ++cam_tested;
        }
    }

    std::printf(
        "PASS_PHASE7_CAMERA_STAGE_B cam_001a64a0=%u cam_shapes=%zu "
        "(parent_clean=%u parent_dirty=%u parent_null=%u parent_empty_list=%u parent_populated_list=%u) "
        "frame_001a7510=%u frame_shapes=%zu "
        "(clean_root=%u dirty_root=%u empty_list=%u populated_list=%u) "
        "dirty_001a70f0=%u\n",
        cam_tested, cam_shapes.size(),
        cam_parent_clean_root, cam_parent_dirty_root, cam_parent_null,
        cam_parent_empty_list, cam_parent_populated_list,
        frame_tested, frame_shapes.size(),
        frame_clean_root, frame_dirty_root, frame_empty_list, frame_populated_list,
        dirty_tested);
    return 0;
}
