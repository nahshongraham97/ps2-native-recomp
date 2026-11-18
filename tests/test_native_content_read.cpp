#include "mka/content_vfs.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void write(const std::filesystem::path& path, const char* text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    check(bool(file), "write host fixture");
}
}
int main() {
    using namespace mka::content;
    const auto parent = std::filesystem::temp_directory_path();
    const auto root = parent / ("mka-native-content-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(root), "exclusive native content directory");
    std::filesystem::create_directory(root / "NEW");
    write(root / "NEW" / "EXTRA.BIN", "new host asset");
    write(root / "EMPTY.BIN", "");
    check(!std::filesystem::exists(root / ".disc_layout") && !std::filesystem::exists(root / ".disc_meta"),
          "fixture has no disc index or sector metadata");
    std::string error;
    check(mount_root(root, &error), "mount host content without ISO/index");
    std::vector<std::uint8_t> out{0xff};
    check(read_file("NEW/EXTRA.BIN", out) && std::string(out.begin(), out.end()) == "new host asset",
          "direct native read accepts unindexed host asset");
    write(root / "NEW" / "EXTRA.BIN", "replacement");
    check(read_file("NEW/EXTRA.BIN", out) && std::string(out.begin(), out.end()) == "replacement",
          "subsequent read observes changed host content, not a stale cached buffer");
    check(read_file("EMPTY.BIN", out) && out.empty(), "empty native file succeeds");
    for (const auto* path : {"MISSING.BIN", "../OUTSIDE.BIN", "NEW/../EMPTY.BIN", "NEW"}) {
        out = {9, 8, 7};
        check(!read_file(path, out) && out.empty(), "failed read clears prior bytes");
    }
    check(std::filesystem::remove(root / "NEW" / "EXTRA.BIN"), "remove changed host fixture");
    out = {1};
    check(!read_file("NEW/EXTRA.BIN", out) && out.empty(), "deleted file does not replay old content");
    unmount();
    // Only the exclusive temporary fixture root may be recursively removed.
    check(root.parent_path() == parent && root.filename().string().find("mka-native-content-") == 0,
          "cleanup stays within exclusive test directory");
    check(std::filesystem::remove_all(root) == 3 && !std::filesystem::exists(root), "remove fixture root only");
    std::puts("native content read tests passed");
}
