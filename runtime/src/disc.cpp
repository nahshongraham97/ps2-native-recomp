#include "ps2/disc.hpp"
#include "ps2/hitch.hpp"
#include "ps2/os.hpp"
#include "load_stats.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>

namespace ps2::disc {

namespace {

std::FILE *g_file = nullptr;
Reader g_reader;
Probe g_probe;
std::uint64_t g_size = 0;
std::uint32_t g_layer1 = 0;
std::map<std::string, Entry> g_index;
std::map<std::pair<std::uint32_t, std::uint32_t>, std::string> g_file_paths_by_extent;
std::string g_extracted_root;
struct ExtractedFile {
    std::uint32_t sector = 0;
    std::uint32_t size = 0;
    std::uint32_t end_sector = 0;
    std::string path;
};
std::vector<ExtractedFile> g_extracted_files;
std::map<std::uint32_t, std::array<std::uint8_t, kSector>> g_extracted_meta;
std::map<std::string, std::ifstream> g_extracted_handles;
bool g_extracted_ready = false;
bool g_indexed = false;  // the reads while the file system is being read are not the game's

std::uint32_t u32(const std::uint8_t *p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

std::string upper(std::string s) {
    for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void walk(std::uint32_t base, std::uint32_t lba, std::uint32_t size, const std::string &prefix, int depth) {
    if (depth > 16) return;
    std::vector<std::uint8_t> data((size + kSector - 1) / kSector * kSector);
    if (!read(base + lba, static_cast<std::uint32_t>(data.size() / kSector), data.data())) return;
    std::size_t off = 0;
    while (off < size) {
        const std::uint8_t length = data[off];
        if (!length) {
            off = (off / kSector + 1) * kSector;
            continue;
        }
        const std::uint8_t *rec = &data[off];
        off += length;
        const std::uint8_t name_len = rec[32];
        std::string name(reinterpret_cast<const char *>(rec + 33), name_len);
        if (name_len == 1 && (name[0] == 0 || name[0] == 1)) continue;
        const auto semi = name.find(';');
        if (semi != std::string::npos) name = name.substr(0, semi);
        const std::string path = prefix + upper(name);
        const bool dir = rec[25] & 2;
        const Entry entry{base + u32(rec + 2), u32(rec + 10), dir};
        g_index[path] = entry;
        if (!dir) g_file_paths_by_extent[{entry.sector, entry.size}] = path;
        if (dir) walk(base, u32(rec + 2), u32(rec + 10), path + "/", depth + 1);
    }
}

void index_layer(std::uint32_t pvd_sector, std::uint32_t base) {
    std::uint8_t pvd[kSector];
    if (!read(pvd_sector, 1, pvd) || std::memcmp(pvd, "\x01" "CD001", 6) != 0) return;
    walk(base, u32(pvd + 158), u32(pvd + 166), "", 0);
}

void self_check();
bool open_extracted(const std::string &root, const std::string &name);
bool read_extracted_sectors(std::uint32_t sector, std::uint32_t count, void *out);
std::optional<std::string> extracted_path_for_indexed_file(const std::string &indexed_path, const Entry &entry);
std::string configured_extracted_root();

void clear_source_state() {
    g_reader = {};
    g_probe = {};
    g_size = 0;
    g_layer1 = 0;
    g_index.clear();
    g_file_paths_by_extent.clear();
    g_extracted_root.clear();
    g_extracted_files.clear();
    g_extracted_meta.clear();
    g_extracted_handles.clear();
    g_extracted_ready = false;
    g_indexed = false;
}

}

bool open(const std::string &path) {
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    clear_source_state();
    const bool strict = extracted_files_strict();
    std::string extracted_root = os::env("PS2_DISC_FILES");
    if (extracted_root.empty()) {
        const std::filesystem::path adjacent = std::filesystem::path(path).parent_path() / "disc-files";
        std::error_code ec;
        if (std::filesystem::is_regular_file(adjacent / ".disc_layout", ec) && !ec &&
            std::filesystem::is_regular_file(adjacent / ".disc_meta", ec) && !ec)
            extracted_root = adjacent.string();
    }
    if (!extracted_root.empty()) {
        g_extracted_root = extracted_root;
        if (open_extracted(extracted_root, path)) {
            std::fprintf(stderr, "[disc] %s: %zu files, %llu sectors, layer 1 at %u (filesystem source)\n", path.c_str(), g_index.size(),
                         static_cast<unsigned long long>(g_size / kSector), g_layer1);
            if (!os::env("PS2_DISC_CHECK").empty()) self_check();
            return true;
        }
        if (strict) {
            std::fprintf(stderr, "[disc-vfs-stop] extracted layout could not be opened from %s\n", extracted_root.c_str());
            clear_source_state();
            return false;
        }
        clear_source_state();
        std::fprintf(stderr, "[disc-vfs] extracted layout unavailable; using the ISO reader\n");
    }
    if (strict) {
        std::fprintf(stderr, "[disc-vfs-stop] strict filesystem mode requires PS2_DISC_FILES or an adjacent disc-files layout\n");
        return false;
    }
    g_file = os::open(path, "rb");
    if (!g_file) return false;
#if defined(_WIN32)
    _fseeki64(g_file, 0, SEEK_END);
    const std::uint64_t size = static_cast<std::uint64_t>(_ftelli64(g_file));
#else
    fseeko(g_file, 0, SEEK_END);
    const std::uint64_t size = static_cast<std::uint64_t>(ftello(g_file));
#endif
    std::FILE *file = g_file;
    return open(path, size, [file](std::uint64_t offset, void *out, std::size_t bytes) {
#if defined(_WIN32)
        _fseeki64(file, static_cast<long long>(offset), SEEK_SET);
#else
        fseeko(file, static_cast<off_t>(offset), SEEK_SET);
#endif
        return std::fread(out, 1, bytes, file);
    });
}

bool open(const std::string &name, std::uint64_t size, Reader reader, Probe probe) {
    clear_source_state();
    if (extracted_files_strict()) {
        std::fprintf(stderr, "[disc-vfs-stop] strict filesystem mode rejects raw disc readers (%s)\n", name.c_str());
        return false;
    }
    g_reader = std::move(reader);
    g_probe = std::move(probe);
    g_size = size;
    g_index.clear();
    g_file_paths_by_extent.clear();
    g_extracted_files.clear();
    g_extracted_meta.clear();
    g_extracted_handles.clear();
    g_extracted_ready = false;
    g_indexed = false;
    index_layer(16, 0);
    std::uint8_t pvd[kSector];
    read(16, 1, pvd);
    const std::uint32_t layer0 = u32(pvd + 80);
    for (std::uint32_t s = layer0; s < layer0 + 64 && static_cast<std::uint64_t>(s) * kSector < g_size; ++s) {
        std::uint8_t probe[kSector];
        if (read(s, 1, probe) && std::memcmp(probe, "\x01" "CD001", 6) == 0) {
            g_layer1 = s - 16;
            index_layer(s, g_layer1);
            break;
        }
    }
    std::fprintf(stderr, "[disc] %s: %zu files, %llu sectors, layer 1 at %u\n", name.c_str(), g_index.size(),
                 static_cast<unsigned long long>(g_size / kSector), g_layer1);
    if (extracted_files_enabled())
        std::fprintf(stderr, "[disc-vfs] extracted file root %s%s\n", os::env("PS2_DISC_FILES").c_str(),
                     extracted_files_strict() ? " (strict file reads)" : "");
    g_indexed = true;
    if (!os::env("PS2_DISC_CHECK").empty()) self_check();
    return true;
}

bool is_open() { return g_extracted_ready || static_cast<bool>(g_reader); }

bool read(std::uint32_t sector, std::uint32_t count, void *out) {
    if (g_extracted_ready) return read_extracted_sectors(sector, count, out);
    if (extracted_files_strict()) {
        if (count) std::memset(out, 0, static_cast<std::size_t>(count) * kSector);
        static bool reported = false;
        if (!reported) {
            std::fprintf(stderr, "[disc-vfs-stop] strict filesystem read requested without an extracted source\n");
            reported = true;
        }
        return false;
    }
    if (!g_reader) return false;
    const cost::Timer time_it(cost::kDisc);
    const std::size_t want = static_cast<std::size_t>(count) * kSector;
    if (cost::g_on) cost::g_disc_bytes += want;
    const auto started = hitch::on() ? hitch::Clock::now() : hitch::Clock::time_point{};
    const std::size_t got = g_reader(static_cast<std::uint64_t>(sector) * kSector, out, want);
    if (hitch::on()) {  // PS2_HITCH_LOG=<ms>: a disc read that held the game up (one third of the limit, since a frame has other work too)
        const double ms = hitch::ms_since(started);
        if (ms > hitch::threshold_ms() / 3) std::fprintf(stderr, "[hitch] disc read of %u sectors at sector %u (%.1f MB) took %.1f ms\n", count, sector, want / 1048576.0, ms);
    }
    if (got < want) std::memset(static_cast<std::uint8_t *>(out) + got, 0, want - got);
    // A game on a two-layer disc reads the second layer with the same sector numbers, which simply continue: the layer
    // boundary is not a gap in the image. Say so once, and say it when a read comes back short (the rest is filled with
    // zeros, which would otherwise go unnoticed). PS2_TRACE_DISC=1 prints every read in layer 1 and past the 4 GiB line.
    static const bool trace = !os::env("PS2_TRACE_DISC").empty();
    static std::uint32_t short_reads = 0, layer1_reads = 0;
    if (g_indexed && g_layer1 && count && sector + count > g_layer1 && (!layer1_reads++ || trace))
        std::fprintf(stderr, "[disc] read in layer 1: sector %u count %u (read %u)\n", sector, count, layer1_reads);
    if (got < want && count && short_reads++ < 20)
        std::fprintf(stderr, "[disc] short read: sector %u count %u, %zu of %zu bytes (the rest is zero)\n", sector, count, got, want);
    return got > 0 || !count;
}

namespace {

// FNV-1a of one sector, to compare with the same sector read by another program (scripts/disc.py reads the image with
// plain Python). The 4 GiB line lies in the second layer of a two-layer disc, and a read there is the one a 32-bit offset
// would get wrong.
std::uint64_t checksum(std::uint32_t sector) {
    std::uint8_t data[kSector];
    read(sector, 1, data);
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (const std::uint8_t b : data) h = (h ^ b) * 0x100000001B3ull;
    return h;
}

void self_check() {
    const std::uint32_t last = static_cast<std::uint32_t>(g_size / kSector ? g_size / kSector - 1 : 0);
    for (const std::uint32_t s : {0u, 16u, g_layer1, g_layer1 + 16u, (1u << 21) - 1u, 1u << 21, (1u << 21) + 1u, last})
        if (s <= last) std::fprintf(stderr, "[disc-check] sector %u fnv1a %016llx\n", s, static_cast<unsigned long long>(checksum(s)));
    for (const auto &[path, entry] : g_index)
        if (g_layer1 && !entry.directory && entry.sector >= g_layer1) {
            const std::uint32_t end = entry.sector + (entry.size ? (entry.size - 1) / kSector : 0);
            std::fprintf(stderr, "[disc-check] layer 1 file %s: sectors %u to %u, first %016llx, last %016llx\n", path.c_str(), entry.sector, end,
                         static_cast<unsigned long long>(checksum(entry.sector)), static_cast<unsigned long long>(checksum(end)));
        }
}

}

bool ready(std::uint32_t sector, std::uint32_t count) {
    if (extracted_files_strict() && !g_extracted_ready) return false;
    return !g_probe || g_probe(static_cast<std::uint64_t>(sector) * kSector, static_cast<std::size_t>(count) * kSector);
}

std::optional<Entry> find(const std::string &raw) {
    std::string path = raw;
    const auto colon = path.find(':');
    if (colon != std::string::npos) path = path.substr(colon + 1);
    std::replace(path.begin(), path.end(), '\\', '/');
    const auto semi = path.find(';');
    if (semi != std::string::npos) path = path.substr(0, semi);
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    auto it = g_index.find(upper(path));
    if (it == g_index.end()) return std::nullopt;
    return it->second;
}

bool extracted_files_enabled() { return !configured_extracted_root().empty(); }

bool extracted_files_strict() {
    const std::string value = upper(os::env("PS2_DISC_FILES_STRICT"));
    return !value.empty() && value != "0" && value != "OFF" && value != "FALSE" && value != "NO";
}

namespace {

std::string configured_extracted_root() {
    return g_extracted_root.empty() ? os::env("PS2_DISC_FILES") : g_extracted_root;
}

std::optional<std::string> native_overlay_path_for(const std::string &raw) {
    const std::string root_setting = os::env("PS2_NATIVE_VFS_ROOT");
    if (root_setting.empty()) return std::nullopt;

    std::string path = raw;
    const auto colon = path.find(':');
    if (colon == std::string::npos) return std::nullopt;
    std::string device = upper(path.substr(0, colon));
    while (!device.empty() && std::isdigit(static_cast<unsigned char>(device.back()))) device.pop_back();
    if (device != "CDROM") return std::nullopt;

    path.erase(0, colon + 1);
    std::replace(path.begin(), path.end(), '\\', '/');
    const auto semi = path.find(';');
    if (semi != std::string::npos) path.resize(semi);
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    if (path.empty() || path.find('\0') != std::string::npos) return std::nullopt;

    std::filesystem::path relative;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string component = path.substr(start, end == std::string::npos ? end : end - start);
        if (component.empty() || component == "." || component == ".." || component.find(':') != std::string::npos)
            return std::nullopt;
        relative /= upper(component);
        if (end == std::string::npos) break;
        start = end + 1;
    }

    std::error_code ec;
    const std::filesystem::path root = std::filesystem::canonical(root_setting, ec);
    if (ec || !std::filesystem::is_directory(root, ec) || ec) return std::nullopt;
    const std::filesystem::path candidate = std::filesystem::canonical(root / relative, ec);
    if (ec || !std::filesystem::is_regular_file(candidate, ec) || ec) return std::nullopt;

    const std::filesystem::path within_root = candidate.lexically_relative(root);
    if (within_root.empty() || within_root.is_absolute()) return std::nullopt;
    for (const auto &component : within_root)
        if (component == "..") return std::nullopt;
    return candidate.string();
}

std::optional<std::string> extracted_path_for_indexed_file(const std::string &indexed_path, const Entry &entry) {
    const std::string root = configured_extracted_root();
    if (entry.directory || root.empty()) return std::nullopt;
    std::filesystem::path relative;
    std::size_t start = 0;
    while (start < indexed_path.size()) {
        const std::size_t end = indexed_path.find('/', start);
        const std::string component = indexed_path.substr(start, end == std::string::npos ? end : end - start);
        if (component.empty() || component == "." || component == ".." || component.find(':') != std::string::npos)
            return std::nullopt;
        relative /= component;
        if (end == std::string::npos) break;
        start = end + 1;
    }

    std::error_code ec;
    const std::filesystem::path candidate = std::filesystem::path(root) / relative;
    if (!std::filesystem::is_regular_file(candidate, ec) || ec) return std::nullopt;
    const std::uintmax_t host_size = std::filesystem::file_size(candidate, ec);
    if (ec || host_size != entry.size) return std::nullopt;
    return candidate.string();
}

bool read_host_file(const std::string &path, std::vector<std::uint8_t> &out, std::uint32_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    out.resize(size);
    if (size) file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(size));
    if (!file || static_cast<std::uint32_t>(file.gcount()) != size) {
        out.clear();
        return false;
    }
    return true;
}

bool open_extracted(const std::string &root, const std::string &name) {
    g_extracted_root = root;
    g_index.clear();
    g_file_paths_by_extent.clear();
    g_extracted_files.clear();
    g_extracted_meta.clear();
    g_extracted_handles.clear();
    g_extracted_ready = false;
    g_indexed = false;

    std::ifstream layout(std::filesystem::path(root) / ".disc_layout");
    if (!layout) return false;
    std::uint64_t total_sectors = 0;
    std::uint32_t layer1 = 0;
    std::string kind;
    while (layout >> kind) {
        if (kind == "sectors") {
            if (!(layout >> total_sectors)) return false;
        } else if (kind == "layer1") {
            if (!(layout >> layer1)) return false;
        } else if (kind == "file") {
            std::uint32_t sector = 0, size = 0;
            std::string path;
            if (!(layout >> sector >> size)) return false;
            std::getline(layout, path);
            const auto first = path.find_first_not_of(" \t");
            if (first == std::string::npos) return false;
            path = upper(path.substr(first));
            if (path.find('\\') != std::string::npos || path.front() == '/' || path.find("..") != std::string::npos)
                return false;
            const std::uint64_t sectors = (static_cast<std::uint64_t>(size) + kSector - 1) / kSector;
            if (static_cast<std::uint64_t>(sector) + sectors > total_sectors) return false;
            const Entry entry{sector, size, false};
            g_index[path] = entry;
            g_file_paths_by_extent[{sector, size}] = path;
            for (std::size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
                g_index.try_emplace(path.substr(0, slash), Entry{0, 0, true});
        } else {
            return false;
        }
    }
    if (!layout.eof() || total_sectors == 0 || layer1 >= total_sectors) return false;

    for (const auto &[path, entry] : g_index) {
        if (entry.directory) continue;
        const auto host_path = extracted_path_for_indexed_file(path, entry);
        if (!host_path) {
            std::fprintf(stderr, "[disc-vfs] missing or wrong-sized extracted file: %s\n", path.c_str());
            return false;
        }
        const std::uint64_t end = static_cast<std::uint64_t>(entry.sector) + (entry.size + kSector - 1) / kSector;
        g_extracted_files.push_back(ExtractedFile{entry.sector, entry.size, static_cast<std::uint32_t>(end), *host_path});
    }
    std::sort(g_extracted_files.begin(), g_extracted_files.end(), [](const ExtractedFile &a, const ExtractedFile &b) {
        return a.sector < b.sector;
    });
    for (std::size_t i = 1; i < g_extracted_files.size(); ++i) {
        if (g_extracted_files[i].sector < g_extracted_files[i - 1].end_sector) {
            std::fprintf(stderr, "[disc-vfs] overlapping extracted file extents: %s and %s\n",
                         g_extracted_files[i - 1].path.c_str(), g_extracted_files[i].path.c_str());
            return false;
        }
    }

    std::ifstream metadata(std::filesystem::path(root) / ".disc_meta", std::ios::binary);
    if (!metadata) return false;
    while (metadata.peek() != std::char_traits<char>::eof()) {
        std::uint8_t word[4]{};
        metadata.read(reinterpret_cast<char *>(word), sizeof(word));
        if (metadata.gcount() != static_cast<std::streamsize>(sizeof(word))) return false;
        const std::uint32_t sector = static_cast<std::uint32_t>(word[0]) |
            (static_cast<std::uint32_t>(word[1]) << 8u) | (static_cast<std::uint32_t>(word[2]) << 16u) |
            (static_cast<std::uint32_t>(word[3]) << 24u);
        if (sector >= total_sectors) return false;
        std::array<std::uint8_t, kSector> data{};
        metadata.read(reinterpret_cast<char *>(data.data()), data.size());
        if (metadata.gcount() != static_cast<std::streamsize>(data.size())) return false;
        if (!g_extracted_meta.emplace(sector, data).second) {
            std::fprintf(stderr, "[disc-vfs] duplicate extracted metadata sector %u\n", sector);
            return false;
        }
    }
    for (const auto &[sector, data] : g_extracted_meta) {
        (void)data;
        const auto next = std::upper_bound(g_extracted_files.begin(), g_extracted_files.end(), sector,
            [](std::uint32_t value, const ExtractedFile &file) { return value < file.sector; });
        if (next != g_extracted_files.begin()) {
            const ExtractedFile &file = *std::prev(next);
            if (sector >= file.sector && sector < file.end_sector) {
                std::fprintf(stderr, "[disc-vfs] metadata sector %u overlaps extracted file %s\n", sector, file.path.c_str());
                return false;
            }
        }
    }

    g_reader = {};
    g_probe = {};
    g_size = total_sectors * kSector;
    g_layer1 = layer1;
    g_indexed = true;
    g_extracted_ready = true;
    if (!os::env("PS2_TRACE_DISC_FILES").empty())
        std::fprintf(stderr, "[disc-vfs] opened %s: %zu files, %zu nonzero metadata sectors\n", root.c_str(),
                     g_extracted_files.size(), g_extracted_meta.size());
    (void)name;
    return true;
}

bool read_extracted_sectors(std::uint32_t sector, std::uint32_t count, void *out) {
    if (count) std::memset(out, 0, static_cast<std::size_t>(count) * kSector);
    auto *dst = static_cast<std::uint8_t *>(out);
    std::uint32_t remaining = count;
    bool in_range = true;
    while (remaining) {
        if (static_cast<std::uint64_t>(sector) >= g_size / kSector) {
            std::memset(dst, 0, static_cast<std::size_t>(remaining) * kSector);
            in_range = false;
            break;
        }
        const auto next_file = std::upper_bound(g_extracted_files.begin(), g_extracted_files.end(), sector,
            [](std::uint32_t value, const ExtractedFile &file) { return value < file.sector; });
        const ExtractedFile *file = next_file == g_extracted_files.begin() ? nullptr : &*std::prev(next_file);
        if (file && sector >= file->sector && sector < file->end_sector) {
            const std::uint32_t run = std::min(remaining, file->end_sector - sector);
            const std::uint64_t offset = static_cast<std::uint64_t>(sector - file->sector) * kSector;
            const std::uint64_t run_bytes = static_cast<std::uint64_t>(run) * kSector;
            const std::uint64_t read_bytes = offset < file->size ? std::min<std::uint64_t>(run_bytes, file->size - offset) : 0;
            if (read_bytes) {
                auto [handle, inserted] = g_extracted_handles.try_emplace(file->path, file->path, std::ios::binary);
                (void)inserted;
                std::ifstream &input = handle->second;
                input.clear();
                input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
                input.read(reinterpret_cast<char *>(dst), static_cast<std::streamsize>(read_bytes));
                if (input.gcount() != static_cast<std::streamsize>(read_bytes)) return false;
            }
            if (read_bytes < run_bytes)
                std::memset(dst + read_bytes, 0, static_cast<std::size_t>(run_bytes - read_bytes));
            sector += run;
            remaining -= run;
            dst += run_bytes;
            continue;
        }
        const auto meta = g_extracted_meta.find(sector);
        if (meta != g_extracted_meta.end()) {
            std::memcpy(dst, meta->second.data(), kSector);
            ++sector;
            --remaining;
            dst += kSector;
            continue;
        }

        std::uint32_t run = remaining;
        if (next_file != g_extracted_files.end() && next_file->sector > sector)
            run = std::min(run, next_file->sector - sector);
        const auto next_meta = g_extracted_meta.lower_bound(sector);
        if (next_meta != g_extracted_meta.end() && next_meta->first > sector)
            run = std::min(run, next_meta->first - sector);
        if (!run) run = 1;
        std::memset(dst, 0, static_cast<std::size_t>(run) * kSector);
        sector += run;
        remaining -= run;
        dst += static_cast<std::size_t>(run) * kSector;
    }
    if (!os::env("PS2_TRACE_DISC_FILES").empty())
        std::fprintf(stderr, "[disc-vfs] read sectors from extracted filesystem\n");
    return in_range;
}

}

std::optional<std::string> extracted_file_path(const std::string &raw) {
    const auto entry = find(raw);
    if (!entry) return std::nullopt;
    std::string path = raw;
    const auto colon = path.find(':');
    if (colon != std::string::npos) path = path.substr(colon + 1);
    std::replace(path.begin(), path.end(), '\\', '/');
    const auto semi = path.find(';');
    if (semi != std::string::npos) path = path.substr(0, semi);
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    return extracted_path_for_indexed_file(upper(path), *entry);
}

std::optional<std::string> extracted_file_path(std::uint32_t sector, std::uint32_t size) {
    const auto it = g_file_paths_by_extent.find({sector, size});
    if (it == g_file_paths_by_extent.end()) return std::nullopt;
    const auto indexed = g_index.find(it->second);
    if (indexed == g_index.end()) return std::nullopt;
    return extracted_path_for_indexed_file(it->second, indexed->second);
}

std::optional<std::string> native_vfs_path(const std::string &path) {
    return native_overlay_path_for(path);
}

bool read_file(const std::string &path, std::vector<std::uint8_t> &out) {
    if (const auto host_path = native_vfs_path(path)) {
        std::error_code ec;
        const std::uintmax_t host_size = std::filesystem::file_size(*host_path, ec);
        if (ec || host_size > std::numeric_limits<std::uint32_t>::max()) return false;
        if (read_host_file(*host_path, out, static_cast<std::uint32_t>(host_size))) {
            if (!os::env("PS2_TRACE_DISC_FILES").empty())
                std::fprintf(stderr, "[disc-vfs-overlay] read %s (%llu bytes) from %s\n", path.c_str(),
                             static_cast<unsigned long long>(host_size), host_path->c_str());
            return true;
        }
        return false;
    }

    const auto entry = find(path);
    if (!entry || entry->directory) return false;
    if (const auto host_path = extracted_file_path(path)) {
        if (read_host_file(*host_path, out, entry->size)) {
            if (!os::env("PS2_TRACE_DISC_FILES").empty())
                std::fprintf(stderr, "[disc-vfs] read %s (%u bytes) from %s\n", path.c_str(), entry->size, host_path->c_str());
            return true;
        }
    }
    if (extracted_files_strict()) {
        std::fprintf(stderr, "[disc-vfs-stop] extracted file unavailable or size mismatch: %s\n", path.c_str());
        return false;
    }
    out.resize((entry->size + kSector - 1) / kSector * kSector);
    if (!read(entry->sector, static_cast<std::uint32_t>(out.size() / kSector), out.data())) {
        out.clear();
        return false;
    }
    out.resize(entry->size);
    return true;
}

std::uint32_t layer1_start() { return g_layer1; }
std::uint64_t sector_count() { return g_size / kSector; }

}
