#include "mka/content_vfs.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <utility>

namespace mka::content {

namespace {

std::mutex g_root_mutex;
std::filesystem::path g_root;

std::mutex g_volume_mutex;

// One extracted file extent. Field names follow the layout's terms: sector is
// the file's first logical sector and end_sector is the first sector past it.
struct IndexedFile {
    std::uint32_t sector = 0;
    std::uint32_t size = 0;
    std::uint32_t end_sector = 0;
    std::string path;
};

struct VolumeState {
    bool ready = false;
    std::uint64_t sectors = 0;
    std::uint32_t layer1 = 0;
    std::map<std::string, Entry> index;  // files and their parent directories
    std::vector<IndexedFile> files;      // sorted by sector, non-overlapping
    std::map<std::uint32_t, std::array<std::uint8_t, kSectorSize>> metadata;
    std::map<std::string, std::ifstream> handles;  // one open file per extent
};

VolumeState g_volume;

std::string upper(std::string value) {
    for (char &ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

bool inside_root(const std::filesystem::path &root, const std::filesystem::path &candidate) {
    const std::filesystem::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto &part : relative)
        if (part == "..") return false;
    return true;
}

// Retail resource name to the canonical volume spelling: device prefix removed,
// version suffix dropped, '\' made '/', no leading slash, uppercase components.
// A path that climbs, repeats a device separator or names no file is rejected.
bool normalize_name(const std::string &resource_name, std::string &out) {
    std::string path = resource_name;
    const std::size_t colon = path.find(':');
    if (colon != std::string::npos) {
        std::string device = upper(path.substr(0, colon));
        while (!device.empty() && std::isdigit(static_cast<unsigned char>(device.back()))) device.pop_back();
        if (device != "CDROM") return false;
        path.erase(0, colon + 1);
    }
    std::replace(path.begin(), path.end(), '\\', '/');
    const std::size_t version = path.find(';');
    if (version != std::string::npos) path.resize(version);
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    if (path.empty() || path.find('\0') != std::string::npos) return false;

    std::string normalized;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string part = path.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == ".." || part.find(':') != std::string::npos)
            return false;
        if (!normalized.empty()) normalized += '/';
        normalized += upper(part);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    out = std::move(normalized);
    return true;
}

std::optional<std::filesystem::path> resolve_under(
    const std::filesystem::path &root, const std::string &resource_name) {
    std::string normalized;
    if (!normalize_name(resource_name, normalized)) return std::nullopt;

    std::filesystem::path relative;
    std::size_t start = 0;
    while (start < normalized.size()) {
        const std::size_t end = normalized.find('/', start);
        relative /= normalized.substr(start, end == std::string::npos ? end : end - start);
        if (end == std::string::npos) break;
        start = end + 1;
    }

    std::error_code ec;
    const std::filesystem::path candidate = std::filesystem::canonical(root / relative, ec);
    if (ec || !inside_root(root, candidate)) return std::nullopt;
    if (!std::filesystem::is_regular_file(candidate, ec) && !std::filesystem::is_directory(candidate, ec))
        return std::nullopt;
    if (ec) return std::nullopt;
    return candidate;
}

std::filesystem::path host_path_for(const std::filesystem::path &root, const std::string &normalized) {
    std::filesystem::path relative;
    std::size_t start = 0;
    while (start < normalized.size()) {
        const std::size_t end = normalized.find('/', start);
        relative /= normalized.substr(start, end == std::string::npos ? end : end - start);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return root / relative;
}

std::string trim_trailing(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return value;
}

// The sector reader. Caller holds g_volume_mutex and has checked that the
// volume is ready. Uncovered sectors read as zeros, matching the disc.
bool read_sectors_locked(VolumeState &volume, std::uint32_t sector, std::uint32_t count, void *out) {
    auto *dst = static_cast<std::uint8_t *>(out);
    if (count == 0) return static_cast<std::uint64_t>(sector) < volume.sectors;
    std::uint32_t remaining = count;
    bool in_range = true;
    while (remaining) {
        if (static_cast<std::uint64_t>(sector) >= volume.sectors) {
            std::memset(dst, 0, static_cast<std::size_t>(remaining) * kSectorSize);
            in_range = false;
            break;
        }
        const auto next_file = std::upper_bound(volume.files.begin(), volume.files.end(), sector,
            [](std::uint32_t value, const IndexedFile &file) { return value < file.sector; });
        const IndexedFile *file = next_file == volume.files.begin() ? nullptr : &*std::prev(next_file);
        if (file && sector >= file->sector && sector < file->end_sector) {
            const std::uint32_t run = std::min(remaining, file->end_sector - sector);
            const std::uint64_t offset = static_cast<std::uint64_t>(sector - file->sector) * kSectorSize;
            const std::uint64_t run_bytes = static_cast<std::uint64_t>(run) * kSectorSize;
            const std::uint64_t read_bytes =
                offset < file->size ? std::min<std::uint64_t>(run_bytes, file->size - offset) : 0;
            if (read_bytes) {
                auto [handle, inserted] =
                    volume.handles.try_emplace(file->path, file->path, std::ios::in | std::ios::binary);
                (void)inserted;
                std::ifstream &input = handle->second;
                if (!input.is_open()) return false;
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
        const auto metadata = volume.metadata.find(sector);
        if (metadata != volume.metadata.end()) {
            std::memcpy(dst, metadata->second.data(), kSectorSize);
            ++sector;
            --remaining;
            dst += kSectorSize;
            continue;
        }

        std::uint32_t run = remaining;
        if (next_file != volume.files.end() && next_file->sector > sector)
            run = std::min(run, next_file->sector - sector);
        const auto next_metadata = volume.metadata.lower_bound(sector);
        if (next_metadata != volume.metadata.end() && next_metadata->first > sector)
            run = std::min(run, next_metadata->first - sector);
        if (!run) run = 1;
        std::memset(dst, 0, static_cast<std::size_t>(run) * kSectorSize);
        sector += run;
        remaining -= run;
        dst += static_cast<std::size_t>(run) * kSectorSize;
    }
    return in_range;
}

void clear_volume_locked() {
    g_volume.index.clear();
    g_volume.files.clear();
    g_volume.metadata.clear();
    g_volume.handles.clear();
    g_volume.sectors = 0;
    g_volume.layer1 = 0;
    g_volume.ready = false;
}

} // namespace

bool mount_root(const std::filesystem::path &root, std::string *error) {
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::canonical(root, ec);
    if (ec || !std::filesystem::is_directory(canonical, ec) || ec) {
        if (error) *error = "native content root is not an accessible directory";
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_root_mutex);
        g_root = canonical;
    }

    std::error_code layout_error;
    const bool has_layout =
        std::filesystem::is_regular_file(canonical / ".disc_layout", layout_error) && !layout_error;
    if (!has_layout) {
        std::lock_guard<std::mutex> lock(g_volume_mutex);
        clear_volume_locked();
        std::fprintf(stderr,
                     "[native-vfs] no .disc_layout under %s: the sector-addressed volume is unavailable and CDVD "
                     "cannot serve media\n",
                     canonical.string().c_str());
        if (error) error->clear();
        return true;
    }
    std::string volume_error;
    if (!index_volume(&volume_error)) {
        if (error) *error = volume_error;
        return false;
    }
    if (error) error->clear();
    return true;
}

void unmount() {
    {
        std::lock_guard<std::mutex> lock(g_root_mutex);
        g_root.clear();
    }
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    clear_volume_locked();
}

std::optional<std::filesystem::path> resolve(const std::string &resource_name) {
    std::filesystem::path root;
    {
        std::lock_guard<std::mutex> lock(g_root_mutex);
        root = g_root;
    }
    if (root.empty()) {
        const char *env_root = std::getenv("PS2_NATIVE_VFS_ROOT");
        if (env_root && *env_root) {
            std::error_code ec;
            const auto p = std::filesystem::canonical(env_root, ec);
            if (!ec && std::filesystem::is_directory(p, ec)) {
                root = p;
            }
        }
    }
    if (root.empty()) return std::nullopt;
    return resolve_under(root, resource_name);
}

bool read_file(const std::string &resource_name, std::vector<std::uint8_t> &out) {
    const auto path = resolve(resource_name);
    if (!path) return false;
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(*path, ec);
    if (ec || size > std::numeric_limits<std::uint32_t>::max()) return false;
    std::ifstream file(*path, std::ios::binary);
    if (!file) return false;
    out.resize(static_cast<std::size_t>(size));
    if (!out.empty()) file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!file || file.gcount() != static_cast<std::streamsize>(out.size())) {
        out.clear();
        return false;
    }
    return true;
}

std::filesystem::path mounted_root() {
    std::lock_guard<std::mutex> lock(g_root_mutex);
    return g_root;
}

bool index_volume(std::string *error) {
    std::filesystem::path root;
    {
        std::lock_guard<std::mutex> lock(g_root_mutex);
        root = g_root;
    }
    if (root.empty()) {
        if (error) *error = "native content root is not mounted";
        return false;
    }

    // A failed re-index must not leave the previous volume serving reads, so
    // the live volume is cleared before any parsing starts.
    {
        std::lock_guard<std::mutex> lock(g_volume_mutex);
        clear_volume_locked();
    }

    VolumeState volume;
    std::ifstream layout(root / ".disc_layout");
    if (!layout) {
        if (error) *error = "cannot open .disc_layout in the native content root";
        return false;
    }
    std::string kind;
    while (layout >> kind) {
        if (kind == "sectors") {
            if (!(layout >> volume.sectors)) {
                if (error) *error = ".disc_layout has a malformed sectors record";
                return false;
            }
        } else if (kind == "layer1") {
            if (!(layout >> volume.layer1)) {
                if (error) *error = ".disc_layout has a malformed layer1 record";
                return false;
            }
        } else if (kind == "file") {
            std::uint32_t sector = 0, size = 0;
            std::string path;
            if (!(layout >> sector >> size)) {
                if (error) *error = ".disc_layout has a malformed file record";
                return false;
            }
            std::getline(layout, path);
            const std::size_t first = path.find_first_not_of(" \t");
            if (first == std::string::npos) {
                if (error) *error = ".disc_layout has a file record without a path";
                return false;
            }
            path = trim_trailing(upper(path.substr(first)));
            if (path.empty() || path.front() == '/' || path.find('\\') != std::string::npos ||
                path.find(':') != std::string::npos || path.find("..") != std::string::npos) {
                if (error) *error = ".disc_layout has an unsafe file path: " + path;
                return false;
            }
            const std::uint64_t sectors = (static_cast<std::uint64_t>(size) + kSectorSize - 1) / kSectorSize;
            if (static_cast<std::uint64_t>(sector) + sectors > volume.sectors) {
                if (error) *error = ".disc_layout file extent runs past the volume: " + path;
                return false;
            }
            const Entry entry{sector, size, false};
            volume.index[path] = entry;
            for (std::size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
                volume.index.try_emplace(path.substr(0, slash), Entry{0, 0, true});
        } else {
            if (error) *error = ".disc_layout starts with an unknown record: " + kind;
            return false;
        }
    }
    if (!layout.eof()) {
        if (error) *error = ".disc_layout could not be read to the end";
        return false;
    }
    if (volume.sectors == 0) {
        if (error) *error = ".disc_layout records no sectors";
        return false;
    }
    if (volume.layer1 >= volume.sectors) {
        if (error) *error = ".disc_layout places layer 1 past the volume";
        return false;
    }

    // Every indexed file must exist in the mounted content at exactly its
    // recorded size. A missing or truncated file would make CDVD hand the game
    // short data, so it is a mount error rather than a silent zero fill.
    for (const auto &[path, entry] : volume.index) {
        if (entry.directory) continue;
        std::error_code ec;
        const std::filesystem::path host = host_path_for(root, path);
        if (!std::filesystem::is_regular_file(host, ec) || ec) {
            if (error) *error = "extracted file missing from the content root: " + path;
            return false;
        }
        const std::uintmax_t host_size = std::filesystem::file_size(host, ec);
        if (ec || host_size != entry.size) {
            if (error) *error = "extracted file size differs from .disc_layout: " + path;
            return false;
        }
        const std::uint64_t end = static_cast<std::uint64_t>(entry.sector) +
                                  (static_cast<std::uint64_t>(entry.size) + kSectorSize - 1) / kSectorSize;
        volume.files.push_back(
            IndexedFile{entry.sector, entry.size, static_cast<std::uint32_t>(end), host.string()});
    }
    std::sort(volume.files.begin(), volume.files.end(),
              [](const IndexedFile &a, const IndexedFile &b) { return a.sector < b.sector; });
    for (std::size_t i = 1; i < volume.files.size(); ++i) {
        if (volume.files[i].sector < volume.files[i - 1].end_sector) {
            if (error)
                *error = "overlapping extracted file extents: " + volume.files[i - 1].path + " and " +
                         volume.files[i].path;
            return false;
        }
    }

    std::ifstream metadata(root / ".disc_meta", std::ios::binary);
    if (!metadata) {
        if (error) *error = "cannot open .disc_meta in the native content root";
        return false;
    }
    while (metadata.peek() != std::char_traits<char>::eof()) {
        std::uint8_t word[4]{};
        metadata.read(reinterpret_cast<char *>(word), sizeof(word));
        if (metadata.gcount() != static_cast<std::streamsize>(sizeof(word))) {
            if (error) *error = ".disc_meta ends inside a sector record";
            return false;
        }
        const std::uint32_t sector = static_cast<std::uint32_t>(word[0]) |
            (static_cast<std::uint32_t>(word[1]) << 8u) | (static_cast<std::uint32_t>(word[2]) << 16u) |
            (static_cast<std::uint32_t>(word[3]) << 24u);
        if (sector >= volume.sectors) {
            if (error) *error = ".disc_meta records a sector past the volume";
            return false;
        }
        std::array<std::uint8_t, kSectorSize> data{};
        metadata.read(reinterpret_cast<char *>(data.data()), data.size());
        if (metadata.gcount() != static_cast<std::streamsize>(data.size())) {
            if (error) *error = ".disc_meta ends inside a metadata sector";
            return false;
        }
        if (!volume.metadata.emplace(sector, data).second) {
            if (error) *error = ".disc_meta records a duplicate metadata sector";
            return false;
        }
    }
    for (const auto &[sector, data] : volume.metadata) {
        (void)data;
        const auto next = std::upper_bound(volume.files.begin(), volume.files.end(), sector,
            [](std::uint32_t value, const IndexedFile &file) { return value < file.sector; });
        if (next == volume.files.begin()) continue;
        const IndexedFile &file = *std::prev(next);
        if (sector >= file.sector && sector < file.end_sector) {
            if (error) *error = "metadata sector overlaps extracted file " + file.path;
            return false;
        }
    }

    volume.ready = true;
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    g_volume = std::move(volume);
    std::fprintf(stderr,
                 "[native-vfs] volume: %zu files, %zu metadata sectors, %llu sectors, layer 1 at %u\n",
                 g_volume.files.size(), g_volume.metadata.size(),
                 static_cast<unsigned long long>(g_volume.sectors), g_volume.layer1);
    return true;
}

bool volume_ready() {
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    return g_volume.ready;
}

std::uint64_t sector_count() {
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    return g_volume.sectors;
}

std::uint32_t layer1_start() {
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    return g_volume.layer1;
}

std::optional<Entry> find(const std::string &resource_name) {
    std::string normalized;
    if (!normalize_name(resource_name, normalized)) return std::nullopt;
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    if (!g_volume.ready) return std::nullopt;
    const auto it = g_volume.index.find(normalized);
    if (it == g_volume.index.end()) return std::nullopt;
    return it->second;
}

bool read_sectors(std::uint32_t sector, std::uint32_t count, void *out) {
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    if (!g_volume.ready) {
        if (count) std::memset(out, 0, static_cast<std::size_t>(count) * kSectorSize);
        return false;
    }
    return read_sectors_locked(g_volume, sector, count, out);
}

bool read_entry(const Entry &entry, std::vector<std::uint8_t> &out) {
    if (entry.directory) return false;
    const std::uint32_t sectors = (entry.size + kSectorSize - 1) / kSectorSize;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(sectors) * kSectorSize);
    std::lock_guard<std::mutex> lock(g_volume_mutex);
    if (!g_volume.ready) return false;
    if (!read_sectors_locked(g_volume, entry.sector, sectors, raw.data())) return false;
    out.assign(raw.begin(), raw.begin() + entry.size);
    return true;
}

} // namespace mka::content
