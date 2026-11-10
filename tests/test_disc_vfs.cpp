#include "ps2/disc.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool set_env(const char *name, const char *value) {
#if defined(_WIN32)
    return _putenv_s(name, value ? value : "") == 0;
#else
    return value ? setenv(name, value, 1) == 0 : unsetenv(name) == 0;
#endif
}

bool write_text(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(out);
}

bool write_bytes(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

bool check(bool condition, const char *message) {
    if (!condition) std::cerr << "[disc-vfs-test] FAIL: " << message << '\n';
    return condition;
}

} // namespace

int main() {
    using namespace ps2::disc;
    const auto temp_root = std::filesystem::temp_directory_path();
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto work = temp_root / ("ps2-native-disc-vfs-test-" + std::to_string(unique));
    const auto extracted = work / "valid";
    const auto overlapping = work / "overlap";
    std::filesystem::create_directories(extracted);
    std::filesystem::create_directories(overlapping);

    bool ok = set_env("PS2_DISC_FILES_STRICT", "1") &&
              set_env("PS2_DISC_FILES", nullptr) &&
              set_env("PS2_DISC_CHECK", nullptr) &&
              check(!open((work / "missing.iso").string()), "strict mode accepted an ISO path without an extracted root") &&
              check(!is_open(), "failed strict mount left a source open");

    bool raw_reader_called = false;
    if (!ps2::disc::open("raw-reader", 4ull * kSector,
              [&](std::uint64_t, void *out, std::size_t size) {
                  raw_reader_called = true;
                  std::memset(out, 0, size);
                  return size;
              })) {
        ok = ok && check(!raw_reader_called, "strict raw-reader rejection invoked the reader");
    } else {
        ok = false;
        check(false, "strict mode accepted an injected raw sector reader");
    }
    ok = ok && check(!is_open(), "rejected raw reader left a source open");

    ok = ok && write_text(extracted / ".disc_layout", "sectors 4\nlayer1 0\nfile 1 3 TEST.BIN\n") &&
         write_bytes(extracted / "TEST.BIN", {'A', 'B', 'C'}) &&
         write_bytes(extracted / ".disc_meta", {});
    ok = ok && set_env("PS2_DISC_FILES", extracted.string().c_str());
    ok = ok && check(open((work / "no-iso-needed.iso").string()), "valid extracted layout failed to mount without an ISO") &&
         check(is_open(), "mounted extracted source was reported closed") &&
         check(ready(1, 1), "extracted source was not ready");

    std::array<std::uint8_t, kSector> sector{};
    ok = ok && check(read(1, 1, sector.data()), "file-backed sector read failed") &&
         check(sector[0] == 'A' && sector[1] == 'B' && sector[2] == 'C' && sector[3] == 0,
               "file-backed sector contents or final-sector padding are wrong");
    std::vector<std::uint8_t> file;
    ok = ok && check(read_file("cdrom0:/TEST.BIN", file), "strict ioman-style extracted file read failed") &&
         check(file == std::vector<std::uint8_t>({'A', 'B', 'C'}), "extracted file read returned wrong bytes") &&
         check(!read_file("cdrom0:/MISSING.BIN", file), "strict mode accepted an unlisted extracted file");

    ok = ok && write_text(overlapping / ".disc_layout",
                          "sectors 4\nlayer1 0\nfile 1 3 FIRST.BIN\nfile 1 3 SECOND.BIN\n") &&
         write_bytes(overlapping / "FIRST.BIN", {'A', 'B', 'C'}) &&
         write_bytes(overlapping / "SECOND.BIN", {'X', 'Y', 'Z'}) &&
         write_bytes(overlapping / ".disc_meta", {});
    ok = ok && set_env("PS2_DISC_FILES", overlapping.string().c_str()) &&
         check(!open((work / "overlap.iso").string()), "strict mount accepted overlapping file extents") &&
         check(!is_open(), "failed overlap validation left a source open");

    set_env("PS2_DISC_FILES", nullptr);
    set_env("PS2_DISC_FILES_STRICT", nullptr);
    std::error_code ec;
    const auto canonical_temp = std::filesystem::weakly_canonical(temp_root, ec);
    const auto canonical_parent = std::filesystem::weakly_canonical(work.parent_path(), ec);
    if (!ec && canonical_parent == canonical_temp) std::filesystem::remove_all(work, ec);
    if (ec) {
        std::cerr << "[disc-vfs-test] cleanup failed: " << ec.message() << '\n';
        return 1;
    }
    if (!ok) return 1;
    std::cout << "[disc-vfs-test] strict extraction checks passed\n";
    return 0;
}
