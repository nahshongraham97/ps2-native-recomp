#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ps2::disc {

inline constexpr std::uint32_t kSector = 2048;

struct Entry {
    std::uint32_t sector;
    std::uint32_t size;
    bool directory;
};

using Reader = std::function<std::size_t(std::uint64_t offset, void *out, std::size_t size)>;
using Probe = std::function<bool(std::uint64_t offset, std::size_t size)>;

bool open(const std::string &path);
bool open(const std::string &name, std::uint64_t size, Reader reader, Probe probe = {});
bool is_open();
bool read(std::uint32_t sector, std::uint32_t count, void *out);
bool ready(std::uint32_t sector, std::uint32_t count);
std::optional<Entry> find(const std::string &path);
// Optional native filesystem source for disc files. The path is accepted only when it
// resolves to an indexed file in the mounted disc image and its extracted size matches.
bool extracted_files_enabled();
bool extracted_files_strict();
std::optional<std::string> extracted_file_path(const std::string &path);
std::optional<std::string> extracted_file_path(std::uint32_t sector, std::uint32_t size);
bool read_file(const std::string &path, std::vector<std::uint8_t> &out);
std::uint32_t layer1_start();
std::uint64_t sector_count();

}
