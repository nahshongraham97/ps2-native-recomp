#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mka::content {

// Mount the extracted retail content directory as the runner's native asset store.
bool mount_root(const std::filesystem::path &root, std::string *error = nullptr);
void unmount();

// Resolve a retail resource name to a host file. Device prefixes from captured
// resource names are treated only as path syntax; this never opens a disc image.
std::optional<std::filesystem::path> resolve(const std::string &resource_name);
bool read_file(const std::string &resource_name, std::vector<std::uint8_t> &out);
std::filesystem::path mounted_root();

} // namespace mka::content
