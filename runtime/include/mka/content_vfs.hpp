#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mka::content {

inline constexpr std::uint32_t kSectorSize = 2048;

// One entry of the disc's ISO 9660 index, as recorded by .disc_layout. sector
// and size are the file's extent, exactly what CDVD search and read report.
struct Entry {
    std::uint32_t sector = 0;
    std::uint32_t size = 0;
    bool directory = false;
};

// Mount the extracted retail content directory as the runner's native asset store.
bool mount_root(const std::filesystem::path &root, std::string *error = nullptr);
void unmount();

// Resolve a retail resource name to a host file. Device prefixes from captured
// resource names are treated only as path syntax; this never opens a disc image.
std::optional<std::filesystem::path> resolve(const std::string &resource_name);
bool read_file(const std::string &resource_name, std::vector<std::uint8_t> &out);
std::filesystem::path mounted_root();

// Transitional diagnostic sector view, forbidden on the accepted native route.
// Direct file reads above use NativeFile; these functions still preserve
// CDVD-style contracts and are not evidence that the CDVD API was removed.
// Indexing reads
// .disc_layout (the file extents) and .disc_meta (sectors that hold disc
// metadata no file covers) from the root. Legacy CDVD reads, searches and layer
// queries are answered from it until those callers are replaced.
bool index_volume(std::string *error = nullptr);
bool volume_ready();
std::uint64_t sector_count();
std::uint32_t layer1_start();

// Look up a resource name in the volume index. A device prefix ("cdrom0:"), a
// version suffix (";1"), '\' separators, a leading '/' and any letter case are
// accepted; the matched name is always the uppercase '/'-separated spelling.
// Directories are returned with directory set, so a search can reject them.
std::optional<Entry> find(const std::string &resource_name);

// Read sectors from the volume into out. Sectors no file and no metadata
// record covers read as zeros, as they do on the disc. Returns false only when
// the first requested sector lies outside the volume; a request that runs past
// the end is zero-filled and returns false.
bool read_sectors(std::uint32_t sector, std::uint32_t count, void *out);

// Read a whole indexed file, honouring its exact byte size.
bool read_entry(const Entry &entry, std::vector<std::uint8_t> &out);

} // namespace mka::content
