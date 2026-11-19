#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

namespace mka::content {
enum class SeekOrigin { begin, current, end };
struct FileRead { std::size_t bytes = 0; bool eof = false; std::error_code error; };
// An opened host file owns its handle. No guest addresses or disc-sector units.
class NativeFile {
public:
    NativeFile() = default;
    ~NativeFile();
    NativeFile(const NativeFile&) = delete;
    NativeFile& operator=(const NativeFile&) = delete;
    NativeFile(NativeFile&& other) noexcept;
    NativeFile& operator=(NativeFile&& other) noexcept;
    bool open(const std::filesystem::path&, std::error_code&);
    void close() noexcept;
    bool is_open() const noexcept { return file_ != nullptr; }
    // Queries this already opened handle, without reopening or moving its
    // cursor. Windows returns the normalized extended DOS/UNC path, retaining
    // its prefix (do not strip it and reopen under different path semantics).
    // Other hosts explicitly report operation_not_supported. Failure clears out.
    bool opened_path(std::filesystem::path& out, std::error_code&) const;
    // Opens a directory for attributes only, observes its normalized actual
    // handle path and closes that temporary handle. No guessed case folding.
    // Same Windows-only path/UTF16/error contract as opened_path.
    static bool directory_path(const std::filesystem::path&,std::filesystem::path& out,std::error_code&);
    bool position(std::uint64_t&, std::error_code&);
    bool length(std::uint64_t&, std::error_code&);
    bool seek(std::int64_t offset, SeekOrigin, std::error_code&);
    FileRead read(void*, std::size_t);
    bool read_all(std::vector<std::uint8_t>&, std::error_code&);
private:
    std::FILE* file_ = nullptr;
};
} // namespace mka::content
