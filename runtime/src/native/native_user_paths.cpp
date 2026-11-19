#include "mka/native_user_paths.hpp"
#include <memory>
#include <cerrno>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif
namespace mka::platform {
std::FILE* open_native_file(const std::filesystem::path& path, NativeFileMode mode, std::error_code& error) {
    error.clear();
    if(path.empty() || path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos ||
       (mode != NativeFileMode::read_binary && mode != NativeFileMode::write_binary)) {
        error=std::make_error_code(std::errc::invalid_argument); return nullptr;
    }
#ifdef _WIN32
    auto* file=_wfopen(path.c_str(), mode==NativeFileMode::read_binary ? L"rb" : L"wb");
    if(!file) error=std::error_code(errno,std::generic_category());
    return file;
#else
    error=std::make_error_code(std::errc::operation_not_supported); return nullptr;
#endif
}

bool NativeUserPaths::from_local_app_data(const std::filesystem::path& root, NativeUserPaths& out, UserPathFailure& failure) {
    out={}; failure={};
    if (root.empty() || !root.is_absolute()) { failure.kind=UserPathError::invalid_root; return false; }
    for (const auto& part:root) if (part==".." || part==".") { failure.kind=UserPathError::invalid_root; return false; }
    std::error_code ec;
    auto canonical=std::filesystem::canonical(root,ec);
    if (ec || !std::filesystem::is_directory(canonical,ec)) { failure={UserPathError::invalid_root,ec}; return false; }
    out.application_=canonical / "MortalKombatArmageddon";
    out.logs_=out.application_ / "logs";
    return true;
}
bool NativeUserPaths::resolve(NativeUserPaths& out, UserPathFailure& failure) {
    out={}; failure={};
#ifdef _WIN32
    PWSTR path=nullptr;
    const HRESULT hr=SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DONT_VERIFY,nullptr,&path);
    if (FAILED(hr)) { if(path) CoTaskMemFree(path); failure={UserPathError::known_folder,std::error_code(static_cast<int>(hr),std::system_category())}; return false; }
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> owned(path, &CoTaskMemFree);
    const std::filesystem::path root(path ? path : L"");
    return from_local_app_data(root,out,failure);
#else
    failure.kind=UserPathError::unsupported_platform; return false;
#endif
}
bool NativeUserPaths::create_logs(UserPathFailure& failure) const {
    failure={};
    if(logs_.empty()) { failure.kind=UserPathError::invalid_root; return false; }
    std::error_code ec;
    std::filesystem::create_directories(logs_,ec);
    if(ec || !std::filesystem::is_directory(logs_,ec)) { failure={UserPathError::create_logs,ec}; return false; }
    return true;
}
}
