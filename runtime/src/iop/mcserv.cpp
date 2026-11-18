#include "mcserv.hpp"
#include "compliance_census.hpp"

#include "kernel.hpp"
#include "sif.hpp"
#include "sio2.hpp"
#include "ps2/memory.hpp"
#include "ps2/system.hpp"
#include "ps2/os.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace ps2::iop::mcserv {

namespace {

constexpr std::uint32_t kSifSid = 0x80000400u;
constexpr std::int32_t kMcSucceed = 0;
constexpr std::int32_t kMcChangedCard = -1;
constexpr std::int32_t kMcNoFormat = -2;
constexpr std::int32_t kMcNoEntry = -4;
constexpr std::int32_t kMcFailDetect2 = -13;
static bool s_card_changed[2][8] = {
    {false, false, false, false, false, false, false, false},
    {false, false, false, false, false, false, false, false}
};
constexpr std::uint32_t kMcDescParamBytes = 48u;
constexpr std::uint32_t kMcRpcStatBytes = 12u;
constexpr std::uint32_t kMcEndParam2Bytes = 192u;
constexpr std::size_t kMcPageBytes = 512u;
constexpr std::size_t kMcSpareBytes = 16u;
constexpr std::size_t kMcPageStride = kMcPageBytes + kMcSpareBytes;
constexpr std::uint32_t kMcPagesPerCluster = 2u;
constexpr std::uint32_t kMcClusterBytes = static_cast<std::uint32_t>(kMcPageBytes * kMcPagesPerCluster);
constexpr std::uint32_t kMcFatEntriesPerCluster = 256u;
constexpr std::uint32_t kMcIndirectFatCount = 32u;

struct McDescParam {
    std::int32_t fd;
    std::int32_t port;
    std::int32_t slot;
    std::int32_t size;
    std::int32_t offset;
    std::int32_t origin;
    std::uint32_t buffer;
    std::uint32_t param;
    std::uint8_t data[16];
};

struct McNameParam {
    std::int32_t port;
    std::int32_t slot;
    std::int32_t flags;
    std::int32_t maxent;
    std::uint32_t ee_dest; // union: mcT / curdir
    char name[1024];
};

struct McEndParam2 {
    std::int32_t type;
    std::int32_t free;
    std::uint32_t dest1;
    std::uint32_t dest2;
    std::uint8_t src1[64];
    std::uint8_t src2[64];
    union {
        std::int32_t formatted;
        std::uint8_t unused[48];
    };
};

struct McEndParam {
    std::int32_t type;
    std::int32_t free;
    std::uint32_t dest1;
    std::uint32_t dest2;
    std::uint8_t src1[16];
    std::uint8_t src2[16];
    std::uint8_t unused[16];
};

static_assert(sizeof(McDescParam) == kMcDescParamBytes);
static_assert(offsetof(McDescParam, offset) == 16u);
static_assert(offsetof(McDescParam, param) == 28u);
static_assert(sizeof(McNameParam) == 1044u);
static_assert(offsetof(McNameParam, ee_dest) == 16u);
static_assert(offsetof(McNameParam, name) == 20u);
static_assert(sizeof(McEndParam2) == kMcEndParam2Bytes);
static_assert(offsetof(McEndParam2, free) == 4u);
static_assert(offsetof(McEndParam2, formatted) == 144u);
static_assert(sizeof(McEndParam) == 64u);

std::uint32_t ee_physical(std::uint32_t address) { return address & 0x1FFFFFFFu; }

bool valid_ee_range(std::uint32_t address, std::uint32_t size) {
    const std::uint32_t physical = ee_physical(address);
    return physical < ps2::kRamSize && size <= ps2::kRamSize - physical;
}

std::uint16_t read_le16(const std::uint8_t *bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t read_le32(const std::uint8_t *bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::int32_t read_lei32(const std::uint8_t *bytes) {
    const std::uint32_t raw = read_le32(bytes);
    std::int32_t value = 0;
    std::memcpy(&value, &raw, sizeof value);
    return value;
}

bool read_card_cluster(const std::vector<std::uint8_t> &image,
                       std::uint32_t cluster,
                       std::uint32_t clusters_per_card,
                       std::array<std::uint8_t, kMcClusterBytes> &out) {
    if (cluster >= clusters_per_card) return false;
    const std::uint64_t first_page = static_cast<std::uint64_t>(cluster) * kMcPagesPerCluster;
    for (std::uint32_t page = 0; page < kMcPagesPerCluster; ++page) {
        const std::uint64_t source = (first_page + page) * kMcPageStride;
        const std::uint64_t destination = static_cast<std::uint64_t>(page) * kMcPageBytes;
        if (source + kMcPageBytes > image.size()) return false;
        std::memcpy(out.data() + destination, image.data() + source, kMcPageBytes);
    }
    return true;
}

bool mc_card_formatted_and_free(const std::vector<std::uint8_t> &image,
                                std::uint32_t &free_clusters) {
    constexpr char kSuperblockMagic[] = "Sony PS2 Memory Card Format ";
    constexpr std::uint32_t kExpectedCardBytes = 8650752u;
    constexpr std::size_t kSuperblockBytes = 384u;
    if (image.size() != kExpectedCardBytes || image.size() < kSuperblockBytes ||
        std::memcmp(image.data(), kSuperblockMagic, sizeof(kSuperblockMagic) - 1u) != 0)
        return false;

    const std::uint8_t *super = image.data();
    const std::uint16_t page_size = read_le16(super + 40u);
    const std::uint16_t pages_per_cluster = read_le16(super + 42u);
    const std::uint16_t block_size_pages = read_le16(super + 44u);
    const std::uint32_t clusters_per_card = read_le32(super + 48u);
    const std::uint32_t alloc_offset = read_le32(super + 52u);
    const std::uint32_t rootdir_cluster = read_le32(super + 60u);
    if (page_size != kMcPageBytes || pages_per_cluster != kMcPagesPerCluster ||
        block_size_pages == 0 || block_size_pages % pages_per_cluster != 0 ||
        clusters_per_card == 0 || alloc_offset >= clusters_per_card)
        return false;

    const std::uint64_t page_count = static_cast<std::uint64_t>(clusters_per_card) * pages_per_cluster;
    if (page_count * kMcPageStride > image.size()) return false;

    // MCMAN checks the root directory's first two records before marking a card formatted.
    const std::uint64_t root_cluster64 = static_cast<std::uint64_t>(alloc_offset) + rootdir_cluster;
    if (root_cluster64 >= clusters_per_card) return false;
    std::array<std::uint8_t, kMcClusterBytes> root{};
    if (!read_card_cluster(image, static_cast<std::uint32_t>(root_cluster64), clusters_per_card, root)) return false;
    const auto entry_name_is = [&root](std::size_t entry, const char *name) {
        const std::size_t name_offset = entry * 512u + 64u;
        const std::size_t length = std::strlen(name);
        return name_offset + length < root.size() &&
               std::memcmp(root.data() + name_offset, name, length) == 0 &&
               root[name_offset + length] == 0;
    };
    if (!entry_name_is(0u, ".") || !entry_name_is(1u, "..")) return false;

    const std::uint32_t clusters_per_block = block_size_pages / pages_per_cluster;
    const std::uint64_t allocatable_target =
        (static_cast<std::uint64_t>(clusters_per_card) * 1000u) / 1024u + 1u;
    std::array<std::int32_t, kMcIndirectFatCount> bad_blocks{};
    for (std::uint32_t i = 0; i < kMcIndirectFatCount; ++i)
        bad_blocks[i] = read_lei32(super + 208u + i * 4u);

    std::uint32_t current_cluster = alloc_offset;
    std::uint64_t valid_cluster_count = 0;
    bool current_block_valid = false;
    while (valid_cluster_count < allocatable_target && current_cluster < clusters_per_card) {
        if (current_cluster == alloc_offset || current_cluster % clusters_per_block == 0u) {
            current_block_valid = true;
            const std::int32_t block = static_cast<std::int32_t>(current_cluster / clusters_per_block);
            for (const std::int32_t bad : bad_blocks)
                if (bad == block) current_block_valid = false;
        }
        if (current_block_valid) ++valid_cluster_count;
        ++current_cluster;
    }
    if (valid_cluster_count != allocatable_target) return false;
    const std::uint32_t max_allocatable_clusters = current_cluster - alloc_offset;

    std::uint32_t free_count = 0;
    std::uint32_t cached_fat_cluster = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint8_t, kMcClusterBytes> indirect_fat{};
    std::array<std::uint8_t, kMcClusterBytes> fat{};
    for (std::uint32_t fat_index = 0; fat_index < max_allocatable_clusters; ++fat_index) {
        const std::uint32_t indirect_index = fat_index / kMcFatEntriesPerCluster;
        const std::uint32_t fat_offset = fat_index % kMcFatEntriesPerCluster;
        if (fat_offset == 0u) {
            const std::uint32_t ifc_index = indirect_index / kMcFatEntriesPerCluster;
            if (ifc_index >= kMcIndirectFatCount) return false;
            const std::uint32_t ifc_cluster = read_le32(super + 80u + ifc_index * 4u);
            if (!read_card_cluster(image, ifc_cluster, clusters_per_card, indirect_fat)) return false;
            const std::uint32_t indirect_offset = indirect_index % kMcFatEntriesPerCluster;
            cached_fat_cluster = read_le32(indirect_fat.data() + indirect_offset * 4u);
            if (!read_card_cluster(image, cached_fat_cluster, clusters_per_card, fat)) return false;
        }

        const std::int32_t entry = read_lei32(fat.data() + fat_offset * 4u);
        const std::uint32_t block = (alloc_offset + fat_index) / clusters_per_block;
        const bool bad_block = std::find(bad_blocks.begin(), bad_blocks.end(), static_cast<std::int32_t>(block)) != bad_blocks.end();
        if (entry >= 0 && !bad_block) ++free_count;
    }
    free_clusters = free_count;
    return true;
}

std::filesystem::path host_save_base();

std::uint32_t host_calculate_free_clusters(std::uint32_t raw_free_clusters) {
    std::error_code ec;
    std::uint64_t host_used_bytes = 0;
    const auto base = host_save_base();
    if (std::filesystem::exists(base, ec)) {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(base, ec)) {
            if (entry.is_regular_file(ec)) {
                const auto path_str = entry.path().string();
                const auto ext = entry.path().extension().string();
                // Never count raw card images (.ps2), quick-save state files, or config files
                if (ext == ".ps2" || ext == ".tmp" || ext == ".ini") continue;
                if (path_str.find("states") != std::string::npos) continue;
                const std::uint64_t sz = entry.file_size(ec);
                host_used_bytes += (sz + kMcClusterBytes - 1u) & ~(static_cast<std::uint64_t>(kMcClusterBytes) - 1u);
            }
        }
    }
    const std::uint32_t host_used_clusters = static_cast<std::uint32_t>(host_used_bytes / kMcClusterBytes);
    if (raw_free_clusters > host_used_clusters) {
        return raw_free_clusters - host_used_clusters;
    }
    return 7800u;
}

std::int32_t mc_get_info2(const McDescParam &request, McEndParam2 &reply) {
    // The native SIO2 backend exposes one card image as EE port 0, slot 0.
    if (request.port != 0 || request.slot != 0) return kMcFailDetect2;

    std::vector<std::uint8_t> image;
    std::uint32_t free_clusters = 0;
    if (sio2::card_image(image, true)) {
        if (!mc_card_formatted_and_free(image, free_clusters)) return kMcNoFormat;
    } else {
        // Native Windows directory-backed memory card (saves/ or AppData)
        // Eliminates reliance on raw 8 MB PS2 memory card image.
        free_clusters = 8100u;
    }

    free_clusters = host_calculate_free_clusters(free_clusters);

    reply.type = 2; // sceMcTypePS2
    reply.free = free_clusters > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                     ? 0
                     : static_cast<std::int32_t>(free_clusters);
    reply.formatted = 1;

    if (request.port < 2 && request.slot < 8) {
        if (s_card_changed[request.port][request.slot]) {
            s_card_changed[request.port][request.slot] = false;
            return kMcChangedCard;
        }
    }
    return kMcSucceed;
}

#pragma pack(push, 1)
struct SceMcStDateTime {
    std::uint8_t resv2 = 0;
    std::uint8_t sec = 0;
    std::uint8_t min = 0;
    std::uint8_t hour = 12;
    std::uint8_t day = 14;
    std::uint8_t month = 11;
    std::uint16_t year = 2026;
};
static_assert(sizeof(SceMcStDateTime) == 8);

struct SceMcTblGetDir {
    SceMcStDateTime created{};
    SceMcStDateTime modified{};
    std::uint32_t file_size = 0;
    std::uint16_t attr_file = 0;
    std::uint16_t reserve1 = 0;
    std::uint32_t reserve2 = 0;
    std::uint32_t paging = 0;
    char entry_name[32] = {};
};
static_assert(sizeof(SceMcTblGetDir) == 64);
#pragma pack(pop)

struct OpenFile {
    bool in_use = false;
    std::filesystem::path path;
    std::vector<std::uint8_t> data;
    std::size_t pos = 0;
    bool dirty = false;
    std::int32_t flags = 0;
};
static constexpr std::size_t kMaxOpenFiles = 16;
static OpenFile s_open_files[kMaxOpenFiles];
static std::string s_current_dir;

std::filesystem::path host_save_base() {
    if (!g_options.memcard_path.empty()) {
        std::filesystem::path p = ps2::os::path(g_options.memcard_path);
        std::filesystem::path save_dir = p.parent_path();
        if (save_dir.filename() != "saves" && save_dir.filename() != "Saves") {
            save_dir /= "saves";
        }
        std::error_code ec;
        std::filesystem::create_directories(save_dir, ec);
        return save_dir;
    }
    std::filesystem::path fallback = "saves";
    std::error_code ec;
    std::filesystem::create_directories(fallback, ec);
    return fallback;
}



bool pattern_matches(const std::string &name, const std::string &pattern) {
    if (pattern.empty() || pattern == "*") return true;
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        return name.rfind(prefix, 0) == 0;
    }
    return name == pattern;
}

std::int32_t mc_get_dir(const McNameParam &req) {
    if (req.port != 0 || req.slot != 0) return kMcFailDetect2;
    std::string name_str = req.name;
    const bool is_abs = (!name_str.empty() && (name_str.front() == '/' || name_str.front() == '\\'));
    std::string dir_part;
    std::string pat_part;
    auto last_slash = name_str.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        dir_part = name_str.substr(0, last_slash);
        pat_part = name_str.substr(last_slash + 1);
        while (!dir_part.empty() && (dir_part.front() == '/' || dir_part.front() == '\\')) {
            dir_part.erase(0, 1);
        }
    } else {
        pat_part = name_str;
    }
    if (pat_part.empty()) pat_part = "*";

    const std::uint32_t maxent = req.maxent > 0 ? static_cast<std::uint32_t>(req.maxent) : 16u;
    const std::uint32_t dest_ee = req.ee_dest;
    if (dest_ee == 0 || !valid_ee_range(dest_ee, sizeof(SceMcTblGetDir))) {
        std::fprintf(stderr, "[iop-native] mc_get_dir invalid EE dest buffer 0x%08X\n", dest_ee);
        return 0;
    }

    std::filesystem::path dir = host_save_base();
    if (is_abs) {
        if (!dir_part.empty()) dir /= dir_part;
    } else {
        if (!dir_part.empty()) dir /= dir_part;
        else if (!s_current_dir.empty()) dir /= s_current_dir;
    }

    std::vector<SceMcTblGetDir> entries;

    if (pattern_matches(".", pat_part) && entries.size() < maxent) {
        SceMcTblGetDir dot{};
        std::strncpy(dot.entry_name, ".", sizeof(dot.entry_name) - 1);
        dot.attr_file = 0x8427; // Subdirectory
        dot.file_size = 2;
        entries.push_back(dot);
    }
    if (pattern_matches("..", pat_part) && entries.size() < maxent) {
        SceMcTblGetDir dotdot{};
        std::strncpy(dotdot.entry_name, "..", sizeof(dotdot.entry_name) - 1);
        dotdot.attr_file = 0x8427; // Subdirectory
        dotdot.file_size = 2;
        entries.push_back(dotdot);
    }

    std::error_code ec;
    if (std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec)) {
        for (const auto &item : std::filesystem::directory_iterator(dir, ec)) {
            const std::string filename = item.path().filename().string();
            if (pattern_matches(filename, pat_part)) {
                SceMcTblGetDir entry{};
                std::strncpy(entry.entry_name, filename.c_str(), sizeof(entry.entry_name) - 1);
                if (item.is_directory(ec)) {
                    entry.attr_file = 0x8427; // Directory (folder | exists | rwx)
                    entry.file_size = 2; // entry count convention
                } else {
                    entry.attr_file = 0x8497; // File (file | closed | exists | rwx)
                    entry.file_size = static_cast<std::uint32_t>(item.file_size(ec));
                }
                entries.push_back(entry);
                if (entries.size() >= maxent) break;
            }
        }
    }

    const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
    const std::uint32_t copy_bytes = count * sizeof(SceMcTblGetDir);
    if (copy_bytes > 0 && valid_ee_range(dest_ee, copy_bytes)) {
        std::memcpy(ps2::fast<std::uint8_t>(ee_physical(dest_ee)), entries.data(), copy_bytes);
    }
    std::fprintf(stderr, "[iop-native] mc_get_dir pattern='%s' (dir='%s') maxent=%u found=%u\n",
                 pat_part.c_str(), dir.string().c_str(), maxent, count);
    return static_cast<std::int32_t>(count);
}

std::int32_t mc_chdir(const McNameParam &req) {
    if (req.port != 0 || req.slot != 0) return kMcFailDetect2;
    std::string target = req.name;
    std::fprintf(stderr, "[iop-native] mc_chdir target='%s' current='%s'\n", target.c_str(), s_current_dir.c_str());
    if (target.empty() || target == "/" || target == ".") {
        if (target == "/") s_current_dir.clear();
    } else if (target == "..") {
        auto idx = s_current_dir.find_last_of("/\\");
        if (idx != std::string::npos) s_current_dir = s_current_dir.substr(0, idx);
        else s_current_dir.clear();
    } else {
        const bool is_abs = (target.front() == '/' || target.front() == '\\');
        while (!target.empty() && (target.front() == '/' || target.front() == '\\')) {
            target.erase(0, 1);
        }
        while (!target.empty() && (target.back() == '/' || target.back() == '\\')) {
            target.pop_back();
        }
        if (is_abs || s_current_dir.empty()) s_current_dir = target;
        else s_current_dir = s_current_dir + "/" + target;
    }
    
    std::filesystem::path p = host_save_base();
    if (!s_current_dir.empty()) p /= s_current_dir;
    std::error_code ec;
    std::filesystem::create_directories(p, ec);

    if (req.ee_dest != 0 && valid_ee_range(req.ee_dest, 1024)) {
        char *out = ps2::fast<char>(ee_physical(req.ee_dest));
        if (out) {
            std::string res = s_current_dir.empty() ? "/" : ("/" + s_current_dir);
            std::strncpy(out, res.c_str(), 1023);
            out[1023] = '\0';
        }
    }
    return 0;
}

std::int32_t mc_open(const McNameParam &req) {
    if (req.port != 0 || req.slot != 0) return kMcFailDetect2;
    std::string filename = req.name;
    if (filename.empty()) {
        std::fprintf(stderr, "[iop-native] mc_open empty filename\n");
        return -1;
    }
    
    const bool is_abs = (filename.front() == '/' || filename.front() == '\\');
    while (!filename.empty() && (filename.front() == '/' || filename.front() == '\\')) {
        filename.erase(0, 1);
    }

    std::filesystem::path full_path = host_save_base();
    if (!is_abs && !s_current_dir.empty()) full_path /= s_current_dir;
    full_path /= filename;

    int slot_idx = -1;
    for (std::size_t i = 0; i < kMaxOpenFiles; ++i) {
        if (!s_open_files[i].in_use) {
            slot_idx = static_cast<int>(i);
            break;
        }
    }
    if (slot_idx < 0) {
        std::fprintf(stderr, "[iop-native] mc_open out of file handles\n");
        return -1;
    }

    OpenFile &f = s_open_files[slot_idx];
    f = OpenFile{};
    f.in_use = true;
    f.path = full_path;
    f.flags = req.flags;

    std::error_code ec;
    if (std::filesystem::exists(full_path, ec)) {
        std::ifstream in(full_path, std::ios::binary);
        if (in) {
            in.seekg(0, std::ios::end);
            const std::size_t sz = static_cast<std::size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            f.data.resize(sz);
            in.read(reinterpret_cast<char *>(f.data.data()), sz);
        }
    } else {
        std::filesystem::create_directories(full_path.parent_path(), ec);
    }

    std::fprintf(stderr, "[iop-native] mc_open fd=%d path='%s' flags=0x%X size=%zu\n",
                 slot_idx, full_path.string().c_str(), req.flags, f.data.size());
    return slot_idx;
}

std::int32_t mc_close(const McDescParam &req) {
    const std::int32_t fd = req.fd;
    if (fd < 0 || static_cast<std::size_t>(fd) >= kMaxOpenFiles || !s_open_files[fd].in_use) {
        std::fprintf(stderr, "[iop-native] mc_close invalid fd=%d\n", fd);
        return -1;
    }

    OpenFile &f = s_open_files[fd];
    if (f.dirty) {
        std::error_code ec;
        std::filesystem::create_directories(f.path.parent_path(), ec);
        std::ofstream out(f.path, std::ios::binary);
        if (out && !f.data.empty()) {
            out.write(reinterpret_cast<const char *>(f.data.data()), f.data.size());
        }
        std::fprintf(stderr, "[iop-native] mc_close flushed dirty fd=%d to '%s' (%zu bytes)\n",
                     fd, f.path.string().c_str(), f.data.size());
    }
    f.in_use = false;
    f.data.clear();
    return 0;
}

std::int32_t mc_seek(const McDescParam &req) {
    const std::int32_t fd = req.fd;
    if (fd < 0 || static_cast<std::size_t>(fd) >= kMaxOpenFiles || !s_open_files[fd].in_use) return -1;
    OpenFile &f = s_open_files[fd];
    const std::int32_t origin = req.origin;
    const std::int32_t offset = req.offset;
    std::int64_t target_pos = 0;
    if (origin == 0) target_pos = offset;
    else if (origin == 1) target_pos = static_cast<std::int64_t>(f.pos) + offset;
    else if (origin == 2) target_pos = static_cast<std::int64_t>(f.data.size()) + offset;

    if (target_pos < 0) target_pos = 0;
    f.pos = static_cast<std::size_t>(target_pos);
    return static_cast<std::int32_t>(f.pos);
}

std::int32_t mc_read(const McDescParam &req) {
    const std::int32_t fd = req.fd;
    if (fd < 0 || static_cast<std::size_t>(fd) >= kMaxOpenFiles || !s_open_files[fd].in_use) return -1;
    OpenFile &f = s_open_files[fd];
    const std::uint32_t buffer = req.buffer;
    const std::uint32_t size = static_cast<std::uint32_t>(std::max(0, req.size));
    if (size == 0) return 0;
    if (!valid_ee_range(buffer, size)) return -1;

    std::size_t available = (f.pos < f.data.size()) ? (f.data.size() - f.pos) : 0;
    std::size_t to_read = std::min<std::size_t>(size, available);
    if (to_read > 0) {
        std::memcpy(ps2::fast<std::uint8_t>(ee_physical(buffer)), f.data.data() + f.pos, to_read);
        f.pos += to_read;
    }
    std::fprintf(stderr, "[iop-native] mc_read fd=%d bytes=%zu/%u\n", fd, to_read, size);
    return static_cast<std::int32_t>(to_read);
}

std::int32_t mc_write(const McDescParam &req) {
    const std::int32_t fd = req.fd;
    if (fd < 0 || static_cast<std::size_t>(fd) >= kMaxOpenFiles || !s_open_files[fd].in_use) return -1;
    OpenFile &f = s_open_files[fd];
    const std::uint32_t buffer = req.buffer;
    const std::uint32_t size = static_cast<std::uint32_t>(std::max(0, req.size));
    if (size == 0) return 0;
    if (!valid_ee_range(buffer, size)) return -1;

    if (f.pos + size > f.data.size()) {
        f.data.resize(f.pos + size);
    }
    std::memcpy(f.data.data() + f.pos, ps2::fast<std::uint8_t>(ee_physical(buffer)), size);
    f.pos += size;
    f.dirty = true;
    std::fprintf(stderr, "[iop-native] mc_write fd=%d bytes=%u (new total %zu)\n", fd, size, f.data.size());
    return static_cast<std::int32_t>(size);
}

std::int32_t mc_flush(const McDescParam &req) {
    const std::int32_t fd = req.fd;
    if (fd < 0 || static_cast<std::size_t>(fd) >= kMaxOpenFiles || !s_open_files[fd].in_use) return -1;
    OpenFile &f = s_open_files[fd];
    if (f.dirty) {
        std::error_code ec;
        std::filesystem::create_directories(f.path.parent_path(), ec);
        std::ofstream out(f.path, std::ios::binary);
        if (out && !f.data.empty()) {
            out.write(reinterpret_cast<const char *>(f.data.data()), f.data.size());
        }
        f.dirty = false;
        std::fprintf(stderr, "[iop-native] mc_flush fd=%d flushed to '%s'\n", fd, f.path.string().c_str());
    }
    return 0;
}

std::int32_t mc_set_file_info(const McNameParam &name_req, const McDescParam &desc_req) {
    const bool valid_name = (name_req.port == 0 && name_req.slot == 0);
    const bool valid_desc = (desc_req.port == 0 && desc_req.slot == 0);
    if (!valid_name && !valid_desc) return kMcFailDetect2;
    std::fprintf(stderr, "[iop-native] mc_set_file_info path='%s'\n", name_req.name);
    return 0;
}

std::int32_t mc_delete(const McNameParam &name_req, const McDescParam &desc_req) {
    const bool valid_name = (name_req.port == 0 && name_req.slot == 0);
    const bool valid_desc = (desc_req.port == 0 && desc_req.slot == 0);
    if (!valid_name && !valid_desc) return kMcFailDetect2;
    std::string name = name_req.name[0] != '\0' ? name_req.name : "";
    if (name.empty()) return 0;
    std::filesystem::path full_path = host_save_base();
    if (name.front() == '/' || name.front() == '\\') {
        full_path /= name.substr(1);
    } else {
        if (!s_current_dir.empty()) full_path /= s_current_dir;
        full_path /= name;
    }
    std::error_code ec;
    std::filesystem::remove_all(full_path, ec);
    std::fprintf(stderr, "[iop-native] mc_delete '%s'\n", full_path.string().c_str());
    return 0;
}

std::int32_t mc_get_ent_space(const McNameParam &name_req, const McDescParam &desc_req) {
    const bool valid_name = (name_req.port == 0 && name_req.slot == 0);
    const bool valid_desc = (desc_req.port == 0 && desc_req.slot == 0);
    if (!valid_name && !valid_desc) return kMcFailDetect2;
    return 1024;
}

std::int32_t mc_format(const McDescParam &req) {
    if (req.port != 0 || req.slot != 0) return kMcFailDetect2;
    std::error_code ec;
    std::filesystem::create_directories(host_save_base(), ec);
    std::fprintf(stderr, "[iop-native] mc_format completed safely for host save directory\n");
    return 0;
}

[[noreturn]] void strict_stop(const char *reason, std::uint32_t method,
                              std::uint32_t args, std::uint32_t args_size,
                              std::uint32_t ret_size) {
    std::fprintf(stderr,
                 "[iop-native-stop] MCSERV SIF 0x%08X RPC method 0x%08X args 0x%08X size %u reply size %u: %s",
                 kSifSid, method, args, args_size, ret_size, reason);
    if (args_size && valid_ee_range(args, std::min(args_size, 48u))) {
        const std::uint8_t *data = ps2::fast<std::uint8_t>(ee_physical(args));
        if (data) {
            const std::uint32_t words = std::min<std::uint32_t>(args_size / 4u, 12u);
            std::fprintf(stderr, " request");
            for (std::uint32_t i = 0; i < words; ++i) {
                std::uint32_t word = 0;
                std::memcpy(&word, data + 4u * i, sizeof word);
                std::fprintf(stderr, " %08X", word);
            }
        }
    }
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
    std::abort();
}

bool mcsrv_rpc(std::uint32_t method, std::uint32_t args,
               std::uint32_t args_size, std::uint32_t ret,
               std::uint32_t ret_size) {
    if (!valid_ee_range(args, args_size) || !valid_ee_range(ret, ret_size))
        strict_stop("RPC buffer outside EE RAM", method, args, args_size, ret_size);

    // The recovered retail FUN_0011bfa0 calls XMCMAN method 0xFE with a 48-byte
    // request and a 12-byte reply. It rejects version words below 0x20A and
    // 0x20E. Advertise those minimum versions so the retail initialization
    // path can continue; this handshake does not imply that every card RPC is
    // implemented below.
    if (method == 0xFEu) {
        if (args_size != kMcDescParamBytes || ret_size != kMcRpcStatBytes)
            strict_stop("unexpected XMCMAN init buffer sizes", method, args, args_size, ret_size);

        std::uint8_t *reply = ps2::fast<std::uint8_t>(ee_physical(ret));
        if (!reply) strict_stop("XMCMAN init reply is outside EE RAM", method, args, args_size, ret_size);
        const std::uint32_t stat[3] = {
            0u,
            0x20Au,
            0x20Eu,
        };
        std::memcpy(reply, stat, sizeof stat);
        std::fprintf(stderr,
                     "[iop-native] MCSERV XMCMAN init accepted with retail minimum versions 0x20A/0x20E; reply 0x%08X; unsupported card RPCs remain fail-closed\n",
                     ee_physical(ret));
        ps2::census::card_rpc(method, true);
        return true;
    }

    // Legacy MCSERV CMD_INIT delegates to McRetOnly, which returns success.
    // Initialization alone does not imply that card operations are available;
    // unimplemented RPCs below still stop rather than fabricating results.
    if (method == 0x70u) {
        if (args_size != kMcDescParamBytes || ret_size != sizeof(std::uint32_t))
            strict_stop("unexpected legacy init buffer sizes", method, args, args_size, ret_size);
        std::uint8_t *reply = ps2::fast<std::uint8_t>(ee_physical(ret));
        if (!reply) strict_stop("legacy init reply is outside EE RAM", method, args, args_size, ret_size);
        const std::uint32_t result = 0;
        std::memcpy(reply, &result, sizeof result);
        std::fprintf(stderr, "[iop-native] MCSERV legacy init completed; card operations remain unavailable\n");
        return true;
    }

    // Legacy MCSERV CMD_GETINFO (0x78) returns mcEndParam_t rather than the
    // extended mcEndParam2_t used by XMCSERV CMD_GETINFO (0x01).
    if (method == 0x78u) {
        if (args_size != kMcDescParamBytes || ret_size != sizeof(std::int32_t))
            strict_stop("unexpected legacy GetInfo buffer sizes", method, args, args_size, ret_size);
        const std::uint8_t *request_bytes = ps2::fast<std::uint8_t>(ee_physical(args));
        if (!request_bytes)
            strict_stop("legacy GetInfo request is outside EE RAM", method, args, args_size, ret_size);
        McDescParam request{};
        std::memcpy(&request, request_bytes, sizeof request);
        if (request.param == 0 || !valid_ee_range(request.param, sizeof(McEndParam)))
            strict_stop("legacy GetInfo output is outside EE RAM", method, args, args_size, ret_size);
        std::uint8_t *aux_bytes = ps2::fast<std::uint8_t>(ee_physical(request.param));
        std::uint8_t *status_bytes = ps2::fast<std::uint8_t>(ee_physical(ret));
        if (!aux_bytes || !status_bytes)
            strict_stop("legacy GetInfo result is outside EE RAM", method, args, args_size, ret_size);

        McEndParam aux{};
        std::int32_t result = kMcFailDetect2;
        if (request.port == 0 && request.slot == 0) {
            std::vector<std::uint8_t> image;
            std::uint32_t free_clusters = 0;
            if (sio2::card_image(image, true)) {
                if (mc_card_formatted_and_free(image, free_clusters)) {
                    result = kMcSucceed;
                } else {
                    result = kMcNoFormat;
                }
            } else {
                // Native Windows directory-backed memory card
                result = kMcSucceed;
                free_clusters = 8100u;
            }
            if (result == kMcSucceed) {
                free_clusters = host_calculate_free_clusters(free_clusters);
                if (request.size > 0) aux.type = 2; // sceMcTypePS2
                if (request.offset != 0)
                    aux.free = free_clusters > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                                   ? 0
                                   : static_cast<std::int32_t>(free_clusters);
                if (s_card_changed[0][0]) {
                    s_card_changed[0][0] = false;
                    result = kMcChangedCard;
                }
            }
        }
        std::memcpy(aux_bytes, &aux, sizeof aux);
        std::memcpy(status_bytes, &result, sizeof result);
        std::fprintf(stderr,
                     "[iop-native] MCSERV legacy GetInfo port=%d slot=%d status=%d free_clusters=%d type_requested=%d\n",
                     request.port, request.slot, result, aux.free, request.size > 0);
        ps2::census::card_rpc(method, result == kMcSucceed || result == kMcChangedCard);
        return true;
    }

    // XMCSERV CMD_GETINFO (0x01) returns a 4-byte status and writes its
    // mcEndParam2_t payload through the EE pointer in mcDescParam_t::param.
    if (method == 0x01u) {
        if (args_size != kMcDescParamBytes || ret_size != sizeof(std::int32_t))
            strict_stop("unexpected XMCSERV GetInfo2 buffer sizes", method, args, args_size, ret_size);
        const std::uint8_t *request_bytes = ps2::fast<std::uint8_t>(ee_physical(args));
        if (!request_bytes)
            strict_stop("XMCSERV GetInfo2 request is outside EE RAM", method, args, args_size, ret_size);
        McDescParam request{};
        std::memcpy(&request, request_bytes, sizeof request);
        if (request.param == 0 || !valid_ee_range(request.param, kMcEndParam2Bytes))
            strict_stop("XMCSERV GetInfo2 output is outside EE RAM", method, args, args_size, ret_size);
        std::uint8_t *aux_bytes = ps2::fast<std::uint8_t>(ee_physical(request.param));
        std::uint8_t *status_bytes = ps2::fast<std::uint8_t>(ee_physical(ret));
        if (!aux_bytes || !status_bytes)
            strict_stop("XMCSERV GetInfo2 result is outside EE RAM", method, args, args_size, ret_size);

        McEndParam2 aux{};
        const std::int32_t result = mc_get_info2(request, aux);
        std::memcpy(aux_bytes, &aux, sizeof aux);
        std::memcpy(status_bytes, &result, sizeof result);

        std::int32_t ee_status = 0;
        std::int32_t ee_type = 0;
        std::int32_t ee_free = 0;
        std::int32_t ee_formatted = 0;
        std::memcpy(&ee_status, status_bytes, sizeof(ee_status));
        std::memcpy(&ee_type, aux_bytes, sizeof(ee_type));
        std::memcpy(&ee_free, aux_bytes + 4, sizeof(ee_free));
        std::memcpy(&ee_formatted, aux_bytes + 144, sizeof(ee_formatted));

        std::fprintf(stderr,
                     "[iop-native] MCSERV GetInfo2 port=%d slot=%d status=%d free_clusters=%d type=%d format=%d | EE verified: status=%d free=%d type=%d format=%d (req: fd=%d size=%d off=%d orig=%d buf=0x%x param=0x%x)\n",
                     request.port, request.slot, result, aux.free, aux.type, aux.formatted,
                     ee_status, ee_free, ee_type, ee_formatted,
                     request.fd, request.size, request.offset, request.origin, request.buffer, request.param);
        ps2::census::card_rpc(method, result == kMcSucceed || result == kMcChangedCard);
        return true;
    }

    // Host file-level save handlers: decode McDescParam and McNameParam safely
    McDescParam desc_req{};
    if (args_size >= kMcDescParamBytes && valid_ee_range(args, kMcDescParamBytes)) {
        const std::uint8_t *request_bytes = ps2::fast<std::uint8_t>(ee_physical(args));
        if (request_bytes) std::memcpy(&desc_req, request_bytes, sizeof(desc_req));
    }

    McNameParam name_req{};
    if (args_size >= sizeof(McNameParam) && valid_ee_range(args, sizeof(McNameParam))) {
        const std::uint8_t *request_bytes = ps2::fast<std::uint8_t>(ee_physical(args));
        if (request_bytes) std::memcpy(&name_req, request_bytes, sizeof(name_req));
    } else if (args_size > 0 && valid_ee_range(args, std::min<std::uint32_t>(args_size, sizeof(name_req)))) {
        const std::uint8_t *request_bytes = ps2::fast<std::uint8_t>(ee_physical(args));
        if (request_bytes) std::memcpy(&name_req, request_bytes, std::min<std::uint32_t>(args_size, sizeof(name_req)));
    }
    name_req.name[sizeof(name_req.name) - 1] = '\0';

    std::int32_t result = 0;
    bool handled = false;

    if (method == 0x02u || method == 0x71u) {
        result = mc_open(name_req);
        handled = true;
    } else if (method == 0x03u || method == 0x72u) {
        result = mc_close(desc_req);
        handled = true;
    } else if (method == 0x04u || method == 0x75u) {
        result = mc_seek(desc_req);
        handled = true;
    } else if (method == 0x05u || method == 0x73u) {
        result = mc_read(desc_req);
        handled = true;
    } else if (method == 0x06u || method == 0x74u) {
        result = mc_write(desc_req);
        handled = true;
    } else if (method == 0x0Au || method == 0x7Au) {
        result = mc_flush(desc_req);
        handled = true;
    } else if (method == 0x0Cu || method == 0x0Du || method == 0x76u || method == 0x7Bu) {
        // Robust shape-based dispatch between GetDir (directory listing) and Chdir (directory change):
        // GetDir carries maxent > 0 and ee_dest pointing to a destination SceMcTblGetDir array.
        // Chdir carries only a directory path (maxent <= 0 or ee_dest is null/string buffer).
        const bool is_get_dir = (name_req.maxent > 0) && (name_req.ee_dest != 0) &&
                                valid_ee_range(name_req.ee_dest, sizeof(SceMcTblGetDir));
        if (is_get_dir) {
            std::fprintf(stderr,
                         "[iop-native] MCSERV RPC method 0x%02X resolved by shape to mc_get_dir (maxent=%d dest=0x%08X name='%s')\n",
                         method, name_req.maxent, name_req.ee_dest, name_req.name);
            result = mc_get_dir(name_req);
        } else {
            std::fprintf(stderr,
                         "[iop-native] MCSERV RPC method 0x%02X resolved by shape to mc_chdir (maxent=%d dest=0x%08X name='%s')\n",
                         method, name_req.maxent, name_req.ee_dest, name_req.name);
            result = mc_chdir(name_req);
        }
        handled = true;
    } else if (method == 0x0Eu || method == 0x7Cu) {
        result = mc_set_file_info(name_req, desc_req);
        handled = true;
    } else if (method == 0x0Fu || method == 0x79u) {
        result = mc_delete(name_req, desc_req);
        handled = true;
    } else if (method == 0x10u || method == 0x77u) {
        result = mc_format(desc_req);
        handled = true;
    } else if (method == 0x11u || method == 0x80u) {
        result = 0;
        handled = true;
    } else if (method == 0x12u || method == 0x7Eu) {
        result = mc_get_ent_space(name_req, desc_req);
        handled = true;
    }

    if (handled) {
        if (ret && ret_size >= sizeof(std::int32_t) && valid_ee_range(ret, sizeof(std::int32_t))) {
            std::uint8_t *status_bytes = ps2::fast<std::uint8_t>(ee_physical(ret));
            if (status_bytes) std::memcpy(status_bytes, &result, sizeof(result));
        }
        std::fprintf(stderr, "[iop-native] MCSERV RPC method 0x%02X handled natively -> result %d\n", method, result);
        ps2::census::card_rpc(method, result >= 0 || result == kMcNoEntry);
        return true;
    }

    ps2::census::card_rpc(method, false);
    strict_stop("card RPC has no native host implementation yet", method,
                args, args_size, ret_size);
}

} // namespace

void reset() {
    s_current_dir.clear();
    for (auto &f : s_open_files) {
        f.in_use = false;
        f.data.clear();
        f.path.clear();
    }
    sif::unregister_hle_override(kSifSid);
}

void activate() {
    sif::register_hle_override(kSifSid, mcsrv_rpc);
    std::fprintf(stderr,
                 "[iop-native] MCSERV host SIF replacement active at 0x%08X; GetInfo is native, unsupported card RPCs fail closed\n",
                 kSifSid);
}

void restore_sif_override() {
    const auto &loaded = kernel::modules();
    const bool module_loaded = std::any_of(loaded.begin(), loaded.end(), [](const auto &entry) {
        return entry.second.name == "MCSERV";
    });
    if (module_loaded) {
        sif::register_hle_override(kSifSid, mcsrv_rpc);
    } else {
        sif::unregister_hle_override(kSifSid);
    }
}

} // namespace ps2::iop::mcserv
