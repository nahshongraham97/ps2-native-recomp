#include "iop/io.hpp"
#include "ps2/state.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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

bool check(bool condition, const char *message) {
    if (!condition) std::cerr << "[iop-io-overlay-test] FAIL: " << message << '\n';
    return condition;
}

bool write_bytes(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

} // namespace

int main() {
    const auto temp = std::filesystem::temp_directory_path();
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto work = temp / ("ps2-native-iop-io-overlay-" + std::to_string(unique));
    const auto root = work / "overlay";
    std::filesystem::create_directories(root);
    const bool setup = write_bytes(root / "CUSTOM.BIN", {'A', 'B', 'C', 'D', 'E'}) &&
                       set_env("PS2_NATIVE_VFS_ROOT", root.string().c_str()) &&
                       set_env("PS2_DISC_FILES_STRICT", "1") &&
                       set_env("PS2_DISC_FILES", nullptr);
    bool ok = check(setup, "could not create/configure the native overlay fixture");

    const int fd = ps2::iop::io::open("cdrom0:\\CUSTOM.BIN", 0);
    ok = ok && check(fd >= 3, "native overlay open failed without a disc index");
    std::array<std::uint8_t, 2> prefix{};
    ok = ok && check(ps2::iop::io::read(fd, prefix.data(), static_cast<std::uint32_t>(prefix.size())) == 2 &&
                     prefix == std::array<std::uint8_t, 2>{'A', 'B'}, "native overlay read returned wrong prefix");
    std::uint32_t mode = 0, size = 0;
    ok = ok && check(ps2::iop::io::stat("cdrom0:/CUSTOM.BIN", mode, size) && mode == 0x0017u && size == 5,
                     "native overlay stat did not report the host file size");

    ps2::state::Writer writer;
    writer.set_version(9);
    ps2::iop::io::state(writer);
    ps2::iop::io::reset();
    ps2::state::Reader reader(writer.out.data(), writer.out.size());
    reader.set_version(9);
    ps2::iop::io::state(reader);
    std::array<std::uint8_t, 3> suffix{};
    ok = ok && check(ps2::iop::io::read(fd, suffix.data(), static_cast<std::uint32_t>(suffix.size())) == 3 &&
                     suffix == std::array<std::uint8_t, 3>{'C', 'D', 'E'},
                     "version 9 state did not restore the overlay request and file position");
    ok = ok && check(ps2::iop::io::close(fd) == 0, "native overlay close failed");

    // A version 8 state has no overlay-path field. Verify its old ioman payload
    // shape still loads and leaves the new member empty.
    ps2::state::Writer legacy_writer;
    legacy_writer.set_version(8);
    legacy_writer.tag("iopio");
    std::uint32_t entries = 1;
    legacy_writer.pod(entries);
    int legacy_fd = 7;
    legacy_writer.pod(legacy_fd);
    std::uint32_t sector = 0, legacy_size = 12;
    std::uint64_t position = 4;
    std::vector<std::uint8_t> memory;
    bool in_memory = false;
    legacy_writer.pod(sector);
    legacy_writer.pod(legacy_size);
    legacy_writer.pod(position);
    legacy_writer.vec(memory);
    legacy_writer.pod(in_memory);
    ps2::iop::io::reset();
    ps2::state::Reader legacy_reader(legacy_writer.out.data(), legacy_writer.out.size());
    legacy_reader.set_version(8);
    ps2::iop::io::state(legacy_reader);
    ok = ok && check(ps2::iop::io::seek(legacy_fd, 0, 1) == 4,
                     "version 8 ioman state did not preserve the legacy file position");

    ps2::iop::io::reset();
    set_env("PS2_NATIVE_VFS_ROOT", nullptr);
    set_env("PS2_DISC_FILES_STRICT", nullptr);
    std::error_code ec;
    const auto canonical_temp = std::filesystem::weakly_canonical(temp, ec);
    const auto canonical_parent = std::filesystem::weakly_canonical(work.parent_path(), ec);
    if (!ec && canonical_parent == canonical_temp) std::filesystem::remove_all(work, ec);
    if (ec) {
        std::cerr << "[iop-io-overlay-test] cleanup failed: " << ec.message() << '\n';
        return 1;
    }
    if (!ok) return 1;
    std::cout << "[iop-io-overlay-test] native overlay and state compatibility passed\n";
    return 0;
}
