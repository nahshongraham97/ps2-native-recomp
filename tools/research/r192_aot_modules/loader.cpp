// Startup linking of sealed, already compiled version-8 EE module libraries.
// This translation unit contains no translator, compiler, worker or miss hook.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include "loader.hpp"
#include "ps2/ee_live_abi.hpp"
#include "ps2/ee_live.hpp"
#include "ps2/generated.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef R192_AOT_EE_STAMP
#error "Define R192_AOT_EE_STAMP from the authenticated version-8 package recipe"
#endif
#ifndef R192_AOT_MANIFEST_SHA256
#error "Define R192_AOT_MANIFEST_SHA256 as the sealed manifest's lowercase SHA-256"
#endif

namespace mka::aot {
namespace {
[[noreturn]] void fail(const std::string& why) {
    throw std::runtime_error("[sealed-ee-aot] " + why);
}

struct File {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit File(const std::filesystem::path& path) {
        // Deny writes/deletes throughout hash, LoadLibrary and execution.
        value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (value == INVALID_HANDLE_VALUE) fail("cannot open package file: " + path.filename().string());
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(value, &info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            CloseHandle(value); value = INVALID_HANDLE_VALUE;
            fail("package file is not a regular sealed file: " + path.filename().string());
        }
    }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    ~File() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

void nt_check(NTSTATUS status, const char* operation) {
    if (status < 0) fail(std::string("SHA-256 operation failed: ") + operation);
}

std::string hash_file(File& file, std::string* text = nullptr) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    nt_check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0), "open");
    struct Cleanup {
        BCRYPT_ALG_HANDLE& algorithm; BCRYPT_HASH_HANDLE& hash;
        ~Cleanup() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } cleanup{algorithm, hash};
    DWORD object_bytes = 0, received = 0;
    nt_check(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_bytes), sizeof(object_bytes), &received, 0), "object length");
    std::vector<UCHAR> object(object_bytes);
    nt_check(BCryptCreateHash(algorithm, &hash, object.data(), object_bytes, nullptr, 0, 0), "create");
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(file.value, zero, nullptr, FILE_BEGIN)) fail("cannot rewind sealed file");
    std::array<UCHAR, 65536> buffer{};
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) fail("cannot read sealed file");
        if (!count) break;
        nt_check(BCryptHashData(hash, buffer.data(), count, 0), "update");
        if (text) {
            if (text->size() + count > 16u * 1024u * 1024u) fail("manifest exceeds size bound");
            text->append(reinterpret_cast<const char*>(buffer.data()), count);
        }
    }
    std::array<UCHAR, 32> digest{};
    nt_check(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0), "finish");
    std::string result;
    for (const auto byte : digest) { result += "0123456789abcdef"[byte >> 4]; result += "0123456789abcdef"[byte & 15]; }
    return result;
}

bool hex_digest(const std::string& value) {
    return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

bool safe_name(const std::string& name) {
    return name.size() > 4 && name.size() < 200 && name.ends_with(".dll") &&
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == std::string::npos &&
        name.find("..") == std::string::npos;
}

void mapped_span(HMODULE library, const void* pointer, std::size_t bytes, bool executable = false) {
    if (!pointer || !bytes) fail("null or empty module metadata span");
    auto cursor = reinterpret_cast<std::uintptr_t>(pointer);
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - cursor) fail("module metadata pointer overflow");
    const auto end = cursor + bytes;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) ||
            info.AllocationBase != library || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) fail("module metadata lies outside its sealed DLL");
        if (executable && !(info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            fail("module function is not executable DLL code");
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= cursor) fail("invalid mapped module region");
        cursor = std::min(end, next);
    }
}

template<class T> void mapped_array(HMODULE library, const T* pointer, std::uint32_t count) {
    if (!count || count > 8u * 1024u * 1024u || count > std::numeric_limits<std::size_t>::max() / sizeof(T))
        fail("module array count is invalid");
    mapped_span(library, pointer, sizeof(T) * count);
}

void validate_modules(HMODULE library, const ps2::ModuleInfo* modules, std::uint32_t count) {
    mapped_array(library, modules, count);
    for (std::uint32_t k = 0; k < count; ++k) {
        const auto& module = modules[k];
        if (!module.size || (module.size & 3u) || module.size > ps2::kRamSize ||
            (module.fixed_base && (module.fixed_base >= ps2::kRamSize || module.size > ps2::kRamSize - module.fixed_base)))
            fail("module address/size bounds are invalid");
        bool terminated = false;
        for (std::size_t i = 0; i < 256; ++i) {
            mapped_span(library, module.name ? module.name + i : nullptr, 1);
            if (!module.name[i]) { terminated = i != 0; break; }
        }
        if (!terminated) fail("module name is empty or unterminated");
        if (module.check_count > ps2::kRamSize / 4) fail("module check count exceeds memory bounds");
        mapped_array(library, module.checks, module.check_count * 2);
        for (std::uint32_t c = 0; c < module.check_count; ++c) {
            const auto offset = module.checks[2 * c];
            if ((offset & 3u) || offset > module.size - 4) fail("module check word lies outside image");
        }
        mapped_array(library, module.units, module.count);
        for (std::uint32_t u = 0; u < module.count; ++u) {
            const auto& unit = module.units[u];
            // Original Generator::Layout rounds the origin down and emits a
            // full aligned unit, including inactive padding. codegen writes
            // r.base-fixed_base as uint32: leading padding can therefore have
            // a wrapped negative relative base. Match dispatch's unsigned
            // coordinates, but never admit an active entry in that padding.
            const std::uint32_t absolute_base = module.fixed_base + unit.base;
            if (unit.size < 4 || (unit.size & (unit.size - 1)) || unit.size > ps2::kRamSize ||
                (absolute_base & (unit.size - 1)) || absolute_base >= ps2::kRamSize ||
                unit.size > ps2::kRamSize - absolute_base)
                fail("module aligned unit span is invalid: " + std::string(module.name));
            mapped_span(library, reinterpret_cast<const void*>(unit.function), 1, true);
            mapped_array(library, unit.entries, unit.size / 4);
            for (std::uint32_t entry = 0; entry < unit.size / 4; ++entry) {
                if (!unit.entries[entry]) continue;
                const std::uint32_t relative = unit.base + entry * 4;
                if (relative >= module.size)
                    fail("nonzero module entry lies in unit padding outside image: " + std::string(module.name));
            }
        }
    }
}

ps2::EeLiveHost& host() {
    static ps2::EeLiveHost value = [] {
        ps2::EeLiveHost h{};
        h.layout = static_cast<std::uint64_t>(sizeof(ps2::Cpu)) | (static_cast<std::uint64_t>(sizeof(ps2::EeLiveHost)) << 32);
        h.ram = ps2::g_ram; h.spr = ps2::g_spr;
        h.watch_on = ps2::g_watch_on; h.watch_base = ps2::g_watch_base; h.watch_end = ps2::g_watch_end;
        h.dispatch = &ps2::dispatch;
#define X(R, name, params, args) h.name = &ps2::name;
        PS2_EE_LIVE_FUNCTIONS(X)
#undef X
        return h;
    }();
    return value;
}

struct Loaded { HMODULE library; File* locked_file; const ps2::ModuleInfo* modules; std::uint32_t count; int rank; };
// Intentionally retained until process exit: dispatch stores DLL pointers.
std::vector<Loaded>& resident() { static auto* value = new std::vector<Loaded>; return *value; }
bool initialized = false;
}

void initialize_precompiled_aot_modules(const std::filesystem::path& package_directory) {
    if (initialized) fail("startup initializer called twice");
    const auto directory = std::filesystem::canonical(package_directory);
    File manifest(directory / "manifest.txt");
    std::string text;
    const std::string manifest_hash = hash_file(manifest, &text);
    if (!hex_digest(R192_AOT_MANIFEST_SHA256) || manifest_hash != R192_AOT_MANIFEST_SHA256)
        fail("manifest SHA-256 does not match compiled package seal");
    std::istringstream input(text);
    std::string first, schema, stamp_text, extra;
    std::uint64_t cpu_size = 0, host_size = 0, dll_count = 0;
    if (!std::getline(input, first)) fail("missing manifest header");
    std::istringstream header(first);
    if (!(header >> schema >> stamp_text >> cpu_size >> host_size >> dll_count) || header >> extra || schema != "R192_AOT_EE_V1")
        fail("invalid manifest header");
    if (stamp_text.empty() || stamp_text.size() > 16 || stamp_text.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
        fail("invalid manifest stamp");
    const auto stamp = std::stoull(stamp_text, nullptr, 16);
    if (!stamp || stamp != static_cast<std::uint64_t>(R192_AOT_EE_STAMP)) fail("manifest stamp differs from compiled source seal");
    if (cpu_size != sizeof(ps2::Cpu) || host_size != sizeof(ps2::EeLiveHost)) fail("exact CPU/EE-host ABI layout mismatch");
    if (!dll_count || dll_count > 65536) fail("invalid sealed DLL denominator");
    struct Row { std::string hash, name; int rank; std::uint32_t modules; };
    std::vector<Row> rows;
    std::set<std::string> names;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) fail("empty manifest row");
        Row row{}; std::istringstream fields(line);
        if (!(fields >> row.hash >> row.rank >> row.modules >> row.name) || fields >> extra ||
            !hex_digest(row.hash) || !safe_name(row.name) || (row.rank != 0 && row.rank != 1) || !row.modules || row.modules > 65536)
            fail("invalid sealed DLL row");
        std::string folded = row.name;
        std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!names.insert(folded).second) fail("duplicate packaged DLL path");
        const int expected_rank = row.name.find('q', 9) == std::string::npos ? 1 : 0;
        if (row.rank != expected_rank) fail("quick/normal library rank differs from original cache semantics");
        if (!rows.empty() && rows.back().name >= row.name) fail("manifest DLL order is not original sorted startup order");
        rows.push_back(std::move(row));
    }
    if (rows.size() != dll_count) fail("manifest DLL count mismatch");
    std::vector<Loaded> pending;
    try {
        for (const auto& row : rows) {
            auto* file = new File(directory / row.name);
            HMODULE library = nullptr;
            try {
                if (hash_file(*file) != row.hash) fail("DLL SHA-256 mismatch: " + row.name);
                library = LoadLibraryExW((directory / row.name).c_str(), nullptr,
                                         LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
                if (!library) fail("cannot link sealed DLL: " + row.name + " (Win32 " + std::to_string(GetLastError()) + ")");
                using Stamp = std::uint64_t (*)();
                using Init = const ps2::ModuleInfo* (*)(const ps2::EeLiveHost*, std::uint32_t*);
                const auto get_stamp = reinterpret_cast<Stamp>(GetProcAddress(library, "ps2_ee_live_stamp"));
                const auto init = reinterpret_cast<Init>(GetProcAddress(library, "ps2_ee_live_init"));
                if (!get_stamp || !init) fail("missing native EE module exports: " + row.name);
                mapped_span(library, reinterpret_cast<const void*>(get_stamp), 1, true);
                mapped_span(library, reinterpret_cast<const void*>(init), 1, true);
                if (get_stamp() != stamp) fail("DLL generator/header/flags/ELF stamp mismatch: " + row.name);
                std::uint32_t count = 0;
                const auto* modules = init(&host(), &count);
                if (!modules || count != row.modules) fail("DLL exact ABI/module-count refusal: " + row.name);
                validate_modules(library, modules, count);
                pending.push_back({library, file, modules, count, row.rank});
            } catch (...) { if (library) FreeLibrary(library); delete file; throw; }
        }
    } catch (...) {
        for (const auto& item : pending) { FreeLibrary(item.library); delete item.locked_file; }
        throw;
    }
    // No registration occurs until every sealed DLL passes identity and bounds.
    resident() = std::move(pending);
    std::size_t modules = 0;
    for (const auto& item : resident()) { ps2::add_modules_ranked(item.modules, item.count, item.rank); modules += item.count; }
    initialized = true;
    std::fprintf(stderr, "[sealed-ee-aot] registered %zu modules from %zu sealed precompiled DLLs; no runtime compiler\n", modules, resident().size());
}
}
