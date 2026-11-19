#pragma once
#include <filesystem>
#include <cstdio>
#include <system_error>
namespace mka::platform {
enum class NativeFileMode { read_binary, write_binary };
// Returns a caller-owned FILE (fclose required), opening the original native
// wide path on Windows. No encoding or CWD fallback; failures set error.
std::FILE* open_native_file(const std::filesystem::path&, NativeFileMode, std::error_code&);
enum class UserPathError { none, unsupported_platform, known_folder, invalid_root, create_logs };
struct UserPathFailure { UserPathError kind=UserPathError::none; std::error_code host; };
// Native platform policy only. Resolving paths never creates directories.
class NativeUserPaths {
public:
    static bool resolve(NativeUserPaths&, UserPathFailure&);
    // Explicit host root for isolated callers/tests; no environment overrides.
    static bool from_local_app_data(const std::filesystem::path&, NativeUserPaths&, UserPathFailure&);
    const std::filesystem::path& application() const noexcept { return application_; }
    const std::filesystem::path& logs() const noexcept { return logs_; }
    bool create_logs(UserPathFailure&) const;
private:
    std::filesystem::path application_, logs_;
};
}
