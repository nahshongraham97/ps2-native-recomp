// Contract test for mka::content's sector-addressed volume: the native
// replacement for the disc image that CDVD reads, searches and layer queries
// are answered from. No game data: the layout, metadata and files are synthetic.
#include "mka/content_vfs.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, const char *what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_failures;
}

void write_text(const std::filesystem::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void write_bytes(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void write_metadata(const std::filesystem::path &path,
                    const std::vector<std::pair<std::uint32_t, std::uint8_t>> &sectors) {
    std::ofstream out(path, std::ios::binary);
    for (const auto &[sector, fill] : sectors) {
        const std::array<std::uint8_t, 4> word{
            static_cast<std::uint8_t>(sector & 0xFFu), static_cast<std::uint8_t>((sector >> 8u) & 0xFFu),
            static_cast<std::uint8_t>((sector >> 16u) & 0xFFu), static_cast<std::uint8_t>((sector >> 24u) & 0xFFu)};
        out.write(reinterpret_cast<const char *>(word.data()), static_cast<std::streamsize>(word.size()));
        std::array<std::uint8_t, mka::content::kSectorSize> data{};
        data.fill(fill);
        out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    }
}

std::vector<std::uint8_t> sector_bytes(std::uint32_t count) {
    return std::vector<std::uint8_t>(static_cast<std::size_t>(count) * mka::content::kSectorSize);
}

bool all_of(const std::vector<std::uint8_t> &data, std::size_t offset, std::size_t length, std::uint8_t value) {
    if (offset + length > data.size()) return false;
    for (std::size_t i = 0; i < length; ++i)
        if (data[offset + i] != value) return false;
    return true;
}

std::filesystem::path make_fixture(const std::filesystem::path &root) {
    std::filesystem::create_directories(root / "A");
    write_text(root / "A" / "B.BIN", "hello");
    std::vector<std::uint8_t> c(mka::content::kSectorSize);
    for (std::size_t i = 0; i < c.size(); ++i) c[i] = static_cast<std::uint8_t>(i & 0xFFu);
    write_bytes(root / "C.BIN", c);
    write_text(root / "D.BIN", "0123456789");
    write_text(root / ".disc_layout", "sectors 8\nlayer1 0\nfile 2 5 A/B.BIN\nfile 4 2048 C.BIN\n");
    write_metadata(root / ".disc_meta", {{0, 0xAB}, {3, 0xCD}});
    return root;
}

} // namespace

int main() {
    const std::filesystem::path base =
        std::filesystem::temp_directory_path() / "mka-content-volume-test";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    const std::filesystem::path root = make_fixture(base / "disc-files");

    std::string error;
    check(mka::content::mount_root(root, &error), "mount_root accepts a complete extracted layout");
    if (!error.empty()) std::fprintf(stderr, "mount error: %s\n", error.c_str());
    check(mka::content::volume_ready(), "index_volume makes the volume ready");
    check(mka::content::sector_count() == 8, "sector count comes from .disc_layout");
    check(mka::content::layer1_start() == 0, "layer 1 start comes from .disc_layout");

    // Name normalization: device prefix, version suffix, separators, case.
    const auto file = mka::content::find("cdrom0:\\a\\b.bin;1");
    check(file.has_value(), "find accepts a device prefix, backslashes, case and a version suffix");
    check(file && file->sector == 2 && file->size == 5 && !file->directory, "find returns the recorded extent");
    const auto directory = mka::content::find("A");
    check(directory.has_value() && directory->directory, "find reports a parent directory");
    check(!mka::content::find("../A/B.BIN").has_value(), "find rejects a path that climbs out of the root");
    check(!mka::content::find("rom0:/A/B.BIN").has_value(), "find rejects a non-CDROM device");

    // Metadata sectors, file extents and the zero gaps between them.
    auto data = sector_bytes(1);
    check(mka::content::read_sectors(0, 1, data.data()), "a metadata sector is readable");
    check(all_of(data, 0, data.size(), 0xAB), "the recorded metadata sector bytes are served");
    check(mka::content::read_sectors(1, 1, data.data()) && all_of(data, 0, data.size(), 0x00),
          "a sector no file covers reads as zeros");
    check(mka::content::read_sectors(2, 1, data.data()), "a file sector is readable");
    check(std::memcmp(data.data(), "hello", 5) == 0 && all_of(data, 5, data.size() - 5, 0x00),
          "a short file leaves the rest of its last sector zero");
    check(mka::content::read_sectors(3, 1, data.data()) && all_of(data, 0, data.size(), 0xCD),
          "metadata between two files is served in place");

    data = sector_bytes(3);
    check(mka::content::read_sectors(1, 3, data.data()), "a spanning read is served");
    check(all_of(data, 0, mka::content::kSectorSize, 0x00), "the span starts in the zero gap");
    check(std::memcmp(data.data() + mka::content::kSectorSize, "hello", 5) == 0, "the span reaches the file");
    check(all_of(data, 2 * mka::content::kSectorSize, mka::content::kSectorSize, 0xCD), "the span reaches metadata");

    data = sector_bytes(2);
    check(mka::content::read_sectors(4, 2, data.data()), "a file's last sector and the gap after it are readable");
    bool pattern = true;
    for (std::size_t i = 0; i < mka::content::kSectorSize; ++i)
        if (data[i] != static_cast<std::uint8_t>(i & 0xFFu)) pattern = false;
    check(pattern, "the full sector file is served byte for byte");
    check(all_of(data, mka::content::kSectorSize, mka::content::kSectorSize, 0x00), "the sector after it is zero");

    data = sector_bytes(1);
    check(!mka::content::read_sectors(8, 1, data.data()), "a read past the volume reports failure");
    check(all_of(data, 0, data.size(), 0x00), "a read past the volume is zero-filled");

    const auto c_file = mka::content::find("C.BIN");
    check(c_file.has_value(), "the whole-file entry is indexed");
    std::vector<std::uint8_t> whole;
    check(c_file && mka::content::read_entry(*c_file, whole), "read_entry serves an indexed file");
    check(whole.size() == mka::content::kSectorSize && whole[17] == 17, "read_entry honours the exact byte size");

    // An overlapping layout, a missing file and a wrong size are mount errors,
    // not silently zero-filled reads.
    write_text(root / ".disc_layout", "sectors 8\nlayer1 0\nfile 2 5 A/B.BIN\nfile 2 10 D.BIN\n");
    error.clear();
    check(!mka::content::mount_root(root, &error), "an overlapping layout is rejected");
    check(!mka::content::volume_ready(), "a rejected layout leaves no stale volume");

    write_text(root / ".disc_layout", "sectors 8\nlayer1 0\nfile 2 5 A/B.BIN\nfile 4 10 MISSING.BIN\n");
    error.clear();
    check(!mka::content::mount_root(root, &error), "a missing extracted file is rejected");

    write_text(root / ".disc_layout", "sectors 8\nlayer1 0\nfile 2 5 A/B.BIN\nfile 4 9 C.BIN\n");
    error.clear();
    check(!mka::content::mount_root(root, &error), "a wrong extracted size is rejected");

    write_text(root / ".disc_layout", "sectors 8\nlayer1 0\nfile 6 4096 A/B.BIN\n");
    error.clear();
    check(!mka::content::mount_root(root, &error), "an extent past the volume is rejected");

    // A root without a layout stays mountable for path-based content reads but
    // cannot pretend to be a disc.
    make_fixture(base / "no-layout");
    std::filesystem::remove(base / "no-layout" / ".disc_layout");
    error.clear();
    check(mka::content::mount_root(base / "no-layout", &error), "a root without a layout still mounts");
    check(!mka::content::volume_ready(), "a root without a layout has no volume");

    mka::content::unmount();
    std::filesystem::remove_all(base, ec);
    if (g_failures) {
        std::fprintf(stderr, "%d content-volume check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("content volume: all checks passed\n");
    return 0;
}
