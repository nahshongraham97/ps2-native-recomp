#include "mka/content_vfs.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <mutex>

namespace mka::content {

namespace {

std::mutex g_root_mutex;
std::filesystem::path g_root;

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

std::optional<std::filesystem::path> resolve_under(
    const std::filesystem::path &root, const std::string &resource_name) {
    std::string path = resource_name;
    const std::size_t colon = path.find(':');
    if (colon != std::string::npos) {
        std::string device = upper(path.substr(0, colon));
        while (!device.empty() && std::isdigit(static_cast<unsigned char>(device.back()))) device.pop_back();
        if (device != "CDROM") return std::nullopt;
        path.erase(0, colon + 1);
    }
    std::replace(path.begin(), path.end(), '\\', '/');
    const std::size_t version = path.find(';');
    if (version != std::string::npos) path.resize(version);
    while (!path.empty() && path.front() == '/') path.erase(path.begin());
    if (path.empty() || path.find('\0') != std::string::npos) return std::nullopt;

    std::filesystem::path relative;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = path.find('/', start);
        const std::string part = path.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == ".." || part.find(':') != std::string::npos)
            return std::nullopt;
        relative /= upper(part);
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

} // namespace

bool mount_root(const std::filesystem::path &root, std::string *error) {
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::canonical(root, ec);
    if (ec || !std::filesystem::is_directory(canonical, ec) || ec) {
        if (error) *error = "native content root is not an accessible directory";
        return false;
    }
    std::lock_guard<std::mutex> lock(g_root_mutex);
    g_root = canonical;
    if (error) error->clear();
    return true;
}

void unmount() {
    std::lock_guard<std::mutex> lock(g_root_mutex);
    g_root.clear();
}

std::optional<std::filesystem::path> resolve(const std::string &resource_name) {
    std::filesystem::path root;
    {
        std::lock_guard<std::mutex> lock(g_root_mutex);
        root = g_root;
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

} // namespace mka::content
