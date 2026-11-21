#include "mka/native_save_store.hpp"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstddef>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace mka::save {
namespace {
#ifdef _WIN32
constexpr std::wstring_view temporary_prefix = L".__mka_tmp_";

std::error_code last_error() noexcept {
    const auto code = GetLastError();
    return {static_cast<int>(code ? code : ERROR_GEN_FAILURE), std::system_category()};
}

struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle() = default;
    explicit Handle(HANDLE h) noexcept : value(h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& o) noexcept : value(std::exchange(o.value, INVALID_HANDLE_VALUE)) {}
    Handle& operator=(Handle&& o) noexcept {
        if (this != &o) {
            if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
            value = std::exchange(o.value, INVALID_HANDLE_VALUE);
        }
        return *this;
    }
    explicit operator bool() const noexcept { return value != INVALID_HANDLE_VALUE; }
};

std::filesystem::path io_path(const std::filesystem::path& path) {
    auto p = path;
    p.make_preferred();
    const auto& s = p.native();
    if (s.size() >= 4 && (s.compare(0, 4, L"\\\\?\\") == 0 ||
                           s.compare(0, 4, L"\\\\.\\") == 0)) return p;
    if (s.size() >= 3 && ((s[0] >= L'A' && s[0] <= L'Z') ||
                          (s[0] >= L'a' && s[0] <= L'z')) &&
        s[1] == L':' && s[2] == L'\\')
        return std::filesystem::path(L"\\\\?\\" + s);
    if (s.size() >= 2 && s[0] == L'\\' && s[1] == L'\\')
        return std::filesystem::path(L"\\\\?\\UNC\\" + s.substr(2));
    return p;
}

bool same_identity(const BY_HANDLE_FILE_INFORMATION& a,
                   const BY_HANDLE_FILE_INFORMATION& b) noexcept {
    return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
           a.nFileIndexHigh == b.nFileIndexHigh &&
           a.nFileIndexLow == b.nFileIndexLow;
}

bool handle_path(HANDLE h, std::filesystem::path& out, SaveFailure& f) {
    DWORD n = GetFinalPathNameByHandleW(h, nullptr, 0,
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!n) { f = {SaveError::open_failed, last_error()}; return false; }
    std::vector<wchar_t> buffer;
    for (;;) {
        if (n > buffer.max_size()) { f.kind = SaveError::host_capacity; return false; }
        buffer.resize(n);
        const DWORD got = GetFinalPathNameByHandleW(h, buffer.data(), n,
            FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!got) { f = {SaveError::open_failed, last_error()}; return false; }
        if (got >= n) { n = got + 1; continue; }
        out = std::filesystem::path(std::wstring(buffer.data(), got));
        return true;
    }
}

bool utf8_name(std::string_view text, std::wstring& out, SaveFailure& f) {
    out.clear();
    if (text.empty() || text.size() > static_cast<std::size_t>(INT_MAX) ||
        text.find('\0') != text.npos) { f.kind = SaveError::invalid_name; return false; }
    const auto n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n) { f = {SaveError::invalid_name, last_error()}; return false; }
    out.resize(static_cast<std::size_t>(n));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), out.data(), n) != n) {
        f = {SaveError::invalid_name, last_error()}; return false;
    }
    const bool temp_name = out.size() >= temporary_prefix.size() &&
        CompareStringOrdinal(out.data(), static_cast<int>(temporary_prefix.size()),
            temporary_prefix.data(), static_cast<int>(temporary_prefix.size()),
            TRUE) == CSTR_EQUAL;
    if (out == L"." || out == L".." || out.back() == L' ' ||
        out.back() == L'.' || temp_name) {
        f.kind = SaveError::unsafe_name; return false;
    }
    for (const auto c : out) {
        if (c < 32 || c == 127 || c == L'/' || c == L'\\' || c == L':' ||
            c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' ||
            c == L'|') { f.kind = SaveError::unsafe_name; return false; }
    }
    const auto dot = out.find(L'.');
    auto base = out.substr(0, dot);
    std::transform(base.begin(), base.end(), base.begin(), [](wchar_t c) {
        return c >= L'a' && c <= L'z' ? wchar_t(c - L'a' + L'A') : c;
    });
    if (base == L"CON" || base == L"PRN" || base == L"AUX" ||
        base == L"NUL" || base == L"CONIN$" || base == L"CONOUT$") {
        f.kind = SaveError::unsafe_name; return false;
    }
    if (base.size() == 4 &&
        (base.starts_with(L"COM") || base.starts_with(L"LPT")) &&
        ((base[3] >= L'1' && base[3] <= L'9') || base[3] == 0x00B9 ||
         base[3] == 0x00B2 || base[3] == 0x00B3)) {
        f.kind = SaveError::unsafe_name; return false;
    }
    return true;
}

bool to_utf8(const std::wstring& name, std::string& out, SaveFailure& f) {
    if (name.size() > static_cast<std::size_t>(INT_MAX)) {
        f.kind = SaveError::host_capacity; return false;
    }
    const auto n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        name.data(), static_cast<int>(name.size()), nullptr, 0, nullptr, nullptr);
    if (!n) { f = {SaveError::invalid_name, last_error()}; return false; }
    out.resize(static_cast<std::size_t>(n));
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name.data(),
        static_cast<int>(name.size()), out.data(), n, nullptr, nullptr) != n) {
        f = {SaveError::invalid_name, last_error()}; return false;
    }
    return true;
}

enum class Existing { absent, exact, alias, unsafe, error };
Existing existing_file(const std::filesystem::path& path,
                       std::wstring_view exact, SaveFailure& f) {
    WIN32_FIND_DATAW found{};
    const auto h = FindFirstFileW(path.c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) {
        const auto code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND)
            return Existing::absent;
        f = {SaveError::open_failed, last_error()}; return Existing::error;
    }
    FindClose(h);
    if (std::wstring_view(found.cFileName) != exact) {
        f.kind = SaveError::name_alias; return Existing::alias;
    }
    if ((found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        f.kind = SaveError::not_regular_file; return Existing::unsafe;
    }
    return Existing::exact;
}

bool observed_in_root(HANDLE file, const std::filesystem::path& root,
                      SaveFailure& f, std::wstring_view exact = {}) {
    std::filesystem::path observed;
    if (!handle_path(file, observed, f)) return false;
    const auto parent = observed.parent_path().native();
    const auto expected = root.native();
    if (parent.size() > static_cast<std::size_t>(INT_MAX) ||
        expected.size() > static_cast<std::size_t>(INT_MAX) ||
        CompareStringOrdinal(parent.data(), static_cast<int>(parent.size()),
            expected.data(), static_cast<int>(expected.size()), TRUE) != CSTR_EQUAL) {
        f.kind = SaveError::root_changed; return false;
    }
    if (!exact.empty() && observed.filename().native() != exact) {
        f.kind = SaveError::name_alias; return false;
    }
    return true;
}

struct FindHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit FindHandle(HANDLE h) noexcept : value(h) {}
    ~FindHandle() { if (value != INVALID_HANDLE_VALUE) FindClose(value); }
};

struct TempFile {
    Handle file;
    bool committed = false;
    ~TempFile() {
        // No name is armed for deletion before exclusive creation succeeds.
        // Cleanup refers to that opened file even if another name is created.
        if (file && !committed) {
            FILE_DISPOSITION_INFO disposition{TRUE};
            SetFileInformationByHandle(file.value, FileDispositionInfo,
                &disposition, sizeof(disposition));
        }
    }
};
#endif
} // namespace

struct NativeSaveStore::State {
#ifdef _WIN32
    std::filesystem::path root, observed;
    Handle directory;
    BY_HANDLE_FILE_INFORMATION identity{};
    std::mutex mutex;
    bool unchanged(SaveFailure& f) const {
        Handle now(CreateFileW(root.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!now) { f = {SaveError::root_changed, last_error()}; return false; }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(now.value, &info) ||
            !same_identity(identity, info) ||
            !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            f.kind = SaveError::root_changed; return false;
        }
        return true;
    }
#endif
};

NativeSaveStore::NativeSaveStore(std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}
NativeSaveStore::NativeSaveStore(NativeSaveStore&&) noexcept = default;
NativeSaveStore& NativeSaveStore::operator=(NativeSaveStore&&) noexcept = default;
NativeSaveStore::~NativeSaveStore() noexcept = default;

std::optional<NativeSaveStore> NativeSaveStore::open_or_create(
    const std::filesystem::path& directory, SaveFailure& f) noexcept {
    f = {};
#ifndef _WIN32
    (void)directory; f.kind = SaveError::unsupported_host; return {};
#else
    try {
        if (directory.empty() || !directory.is_absolute() ||
            directory.native().find(L'\0') != std::wstring::npos) {
            f.kind = SaveError::invalid_root; return {};
        }
        for (const auto& part : directory)
            if (part == "." || part == "..") {
                f.kind = SaveError::invalid_root; return {};
            }
        // Refuse existing junctions/symlinks before creating a missing leaf.
        // In particular, no user-selected alias may redirect save writes.
        auto prefix = directory.root_path();
        for (const auto& part : directory.relative_path()) {
            prefix /= part;
            const auto attributes = GetFileAttributesW(io_path(prefix).c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES) {
                const auto code = GetLastError();
                if (code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND) {
                    f = {SaveError::invalid_root,
                        {static_cast<int>(code), std::system_category()}};
                    return {};
                }
                break;
            }
            if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                f.kind = SaveError::invalid_root; return {};
            }
        }
        std::error_code ec;
        const auto original_status = std::filesystem::symlink_status(directory, ec);
        if (!ec && std::filesystem::is_symlink(original_status)) {
            f.kind = SaveError::invalid_root; return {};
        }
        ec.clear();
        std::filesystem::create_directories(directory, ec);
        if (ec) { f = {SaveError::invalid_root, ec}; return {}; }
        const auto raw_attributes = GetFileAttributesW(directory.c_str());
        if (raw_attributes == INVALID_FILE_ATTRIBUTES ||
            !(raw_attributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (raw_attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            f.kind = SaveError::invalid_root; return {};
        }
        auto state = std::make_unique<State>();
        state->root = io_path(std::filesystem::canonical(directory, ec));
        if (ec) { f = {SaveError::invalid_root, ec}; return {}; }
        state->directory = Handle(CreateFileW(state->root.c_str(),
            FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!state->directory) { f = {SaveError::invalid_root, last_error()}; return {}; }
        if (!GetFileInformationByHandle(state->directory.value, &state->identity) ||
            !(state->identity.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (state->identity.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            f.kind = SaveError::invalid_root; return {};
        }
        if (!handle_path(state->directory.value, state->observed, f)) return {};
        if (!state->unchanged(f)) return {};
        return NativeSaveStore(std::move(state));
    } catch (const std::bad_alloc&) { f.kind = SaveError::allocation_failed; }
      catch (const std::length_error&) { f.kind = SaveError::host_capacity; }
      catch (const std::filesystem::filesystem_error& e) {
          f = {SaveError::invalid_root, e.code()};
      } catch (...) { f.kind = SaveError::native_exception; }
    return {};
#endif
}

bool NativeSaveStore::validate(SaveFailure& f) const noexcept {
    f = {};
    if (!state_) { f.kind = SaveError::moved_store; return false; }
#ifndef _WIN32
    f.kind = SaveError::unsupported_host; return false;
#else
    try { std::scoped_lock lock(state_->mutex); return state_->unchanged(f); }
    catch (...) { f.kind = SaveError::native_exception; return false; }
#endif
}

bool NativeSaveStore::list(std::vector<std::string>& names,
                           SaveFailure& f) const noexcept {
    f = {}; names.clear();
    if (!state_) { f.kind = SaveError::moved_store; return false; }
#ifndef _WIN32
    f.kind = SaveError::unsupported_host; return false;
#else
    try {
        std::scoped_lock lock(state_->mutex);
        if (!state_->unchanged(f)) return false;
        std::vector<std::string> found_names;
        WIN32_FIND_DATAW entry{};
        const auto pattern = state_->root / L"*";
        FindHandle find(FindFirstFileW(pattern.c_str(), &entry));
        if (find.value == INVALID_HANDLE_VALUE) {
            if (GetLastError() != ERROR_FILE_NOT_FOUND) {
                f = {SaveError::enumerate_failed, last_error()}; return false;
            }
            return state_->unchanged(f);
        }
        do {
            const std::wstring name(entry.cFileName);
            if (name == L"." || name == L".." ||
                name.starts_with(temporary_prefix)) continue;
            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                f.kind = SaveError::not_regular_file; return false;
            }
            std::string utf8;
            if (!to_utf8(name, utf8, f)) return false;
            std::wstring checked;
            if (!utf8_name(utf8, checked, f)) return false;
            found_names.push_back(std::move(utf8));
        } while (FindNextFileW(find.value, &entry));
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            f = {SaveError::enumerate_failed, last_error()}; return false;
        }
        if (!state_->unchanged(f)) return false;
        std::sort(found_names.begin(), found_names.end());
        names = std::move(found_names);
        return true;
    } catch (const std::bad_alloc&) { f.kind = SaveError::allocation_failed; }
      catch (const std::length_error&) { f.kind = SaveError::host_capacity; }
      catch (const std::filesystem::filesystem_error& e) {
          f = {SaveError::enumerate_failed, e.code()};
      } catch (...) { f.kind = SaveError::native_exception; }
    return false;
#endif
}

bool NativeSaveStore::read(std::string_view name,
                           std::vector<std::uint8_t>& bytes,
                           SaveFailure& f) const noexcept {
    f = {}; bytes.clear();
    if (!state_) { f.kind = SaveError::moved_store; return false; }
#ifndef _WIN32
    (void)name; f.kind = SaveError::unsupported_host; return false;
#else
    try {
        std::scoped_lock lock(state_->mutex);
        std::wstring component;
        if (!utf8_name(name, component, f) || !state_->unchanged(f)) return false;
        const auto path = state_->root / component;
        const auto present = existing_file(path, component, f);
        if (present == Existing::absent) { f.kind = SaveError::missing; return false; }
        if (present != Existing::exact) return false;
        Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!file) { f = {SaveError::open_failed, last_error()}; return false; }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file.value, &info)) {
            f = {SaveError::open_failed, last_error()}; return false;
        }
        if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
            f.kind = SaveError::not_regular_file; return false;
        }
        if (!observed_in_root(file.value, state_->observed, f, component)) return false;
        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file.value, &length) || length.QuadPart < 0) {
            f = {SaveError::read_failed, last_error()}; return false;
        }
        std::vector<std::uint8_t> data;
        if (static_cast<std::uint64_t>(length.QuadPart) > data.max_size()) {
            f.kind = SaveError::host_capacity; return false;
        }
        data.resize(static_cast<std::size_t>(length.QuadPart));
        std::size_t at = 0;
        while (at < data.size()) {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
                data.size() - at, std::numeric_limits<DWORD>::max()));
            DWORD got = 0;
            if (!ReadFile(file.value, data.data() + at, chunk, &got, nullptr) || got == 0) {
                f = {SaveError::read_failed, last_error()}; return false;
            }
            at += got;
        }
        if (!state_->unchanged(f)) return false;
        bytes = std::move(data);
        return true;
    } catch (const std::bad_alloc&) { f.kind = SaveError::allocation_failed; }
      catch (const std::length_error&) { f.kind = SaveError::host_capacity; }
      catch (const std::filesystem::filesystem_error& e) {
          f = {SaveError::open_failed, e.code()};
      } catch (...) { f.kind = SaveError::native_exception; }
    return false;
#endif
}

bool NativeSaveStore::replace(std::string_view name,
                              std::span<const std::uint8_t> bytes,
                              SaveFailure& f) const noexcept {
    f = {};
    if (!state_) { f.kind = SaveError::moved_store; return false; }
#ifndef _WIN32
    (void)name; (void)bytes; f.kind = SaveError::unsupported_host; return false;
#else
    try {
        std::scoped_lock lock(state_->mutex);
        std::wstring component;
        if (!utf8_name(name, component, f) || !state_->unchanged(f)) return false;
        const auto destination = state_->root / component;
        const auto present = existing_file(destination, component, f);
        if (present != Existing::absent && present != Existing::exact) return false;
        static std::atomic<std::uint64_t> sequence{0};
        TempFile temp;
        for (;;) {
            const auto id = sequence.fetch_add(1, std::memory_order_relaxed);
            const auto candidate = state_->root / (std::wstring(temporary_prefix) +
                std::to_wstring(GetCurrentProcessId()) + L"_" +
                std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(id));
            temp.file = Handle(CreateFileW(candidate.c_str(), GENERIC_WRITE | DELETE,
                FILE_SHARE_READ, nullptr, CREATE_NEW,
                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (temp.file) break;
            if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) {
                f = {SaveError::open_failed, last_error()}; return false;
            }
        }
        if (!observed_in_root(temp.file.value, state_->observed, f)) return false;
        std::size_t at = 0;
        while (at < bytes.size()) {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
                bytes.size() - at, std::numeric_limits<DWORD>::max()));
            DWORD wrote = 0;
            if (!WriteFile(temp.file.value, bytes.data() + at, chunk, &wrote, nullptr) || wrote == 0) {
                f = {SaveError::write_failed, last_error()}; return false;
            }
            at += wrote;
        }
        if (!FlushFileBuffers(temp.file.value)) {
            f = {SaveError::flush_failed, last_error()}; return false;
        }
        if (!state_->unchanged(f)) return false;
        const auto before_move = existing_file(destination, component, f);
        if (before_move != Existing::absent && before_move != Existing::exact) return false;
        const auto& target = destination.native();
        const auto bytes_needed = offsetof(FILE_RENAME_INFO, FileName) +
            (target.size() + 1) * sizeof(wchar_t);
        if (bytes_needed > std::numeric_limits<DWORD>::max()) {
            f.kind = SaveError::host_capacity; return false;
        }
        // The written file remains open without delete/write sharing until
        // publication. Rename that handle, never a reopened temporary path.
        std::vector<std::uint8_t> rename_storage(bytes_needed);
        auto* rename = reinterpret_cast<FILE_RENAME_INFO*>(rename_storage.data());
        rename->ReplaceIfExists = TRUE;
        rename->RootDirectory = nullptr;
        rename->FileNameLength = static_cast<DWORD>(target.size() * sizeof(wchar_t));
        std::copy(target.begin(), target.end(), rename->FileName);
        rename->FileName[target.size()] = L'\0';
        if (!SetFileInformationByHandle(temp.file.value, FileRenameInfo,
                rename, static_cast<DWORD>(bytes_needed))) {
            f = {SaveError::replace_failed, last_error()}; return false;
        }
        temp.committed = true;
        if (!state_->unchanged(f)) { f.kind = SaveError::after_effect; return false; }
        if (!observed_in_root(temp.file.value, state_->observed, f, component)) {
            f.kind = SaveError::after_effect; return false;
        }
        return true;
    } catch (const std::bad_alloc&) { f.kind = SaveError::allocation_failed; }
      catch (const std::length_error&) { f.kind = SaveError::host_capacity; }
      catch (const std::filesystem::filesystem_error& e) {
          f = {SaveError::replace_failed, e.code()};
      } catch (...) { f.kind = SaveError::native_exception; }
    return false;
#endif
}

bool NativeSaveStore::erase(std::string_view name, SaveFailure& f) const noexcept {
    f = {};
    if (!state_) { f.kind = SaveError::moved_store; return false; }
#ifndef _WIN32
    (void)name; f.kind = SaveError::unsupported_host; return false;
#else
    try {
        std::scoped_lock lock(state_->mutex);
        std::wstring component;
        if (!utf8_name(name, component, f) || !state_->unchanged(f)) return false;
        const auto path = state_->root / component;
        const auto present = existing_file(path, component, f);
        if (present == Existing::absent) { f.kind = SaveError::missing; return false; }
        if (present != Existing::exact) return false;
        Handle file(CreateFileW(path.c_str(), DELETE | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (!file) { f = {SaveError::delete_failed, last_error()}; return false; }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file.value, &info)) {
            f = {SaveError::delete_failed, last_error()}; return false;
        }
        if (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
            f.kind = SaveError::not_regular_file; return false;
        }
        if (!observed_in_root(file.value, state_->observed, f, component)) return false;
        FILE_DISPOSITION_INFO disposition{TRUE};
        if (!SetFileInformationByHandle(file.value, FileDispositionInfo,
                &disposition, sizeof(disposition))) {
            f = {SaveError::delete_failed, last_error()}; return false;
        }
        file = Handle{};
        if (!state_->unchanged(f) || existing_file(path, component, f) != Existing::absent) {
            f.kind = SaveError::after_effect; return false;
        }
        return true;
    } catch (const std::bad_alloc&) { f.kind = SaveError::allocation_failed; }
      catch (const std::length_error&) { f.kind = SaveError::host_capacity; }
      catch (const std::filesystem::filesystem_error& e) {
          f = {SaveError::delete_failed, e.code()};
      } catch (...) { f.kind = SaveError::native_exception; }
    return false;
#endif
}

} // namespace mka::save
