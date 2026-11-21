#include "mka/native_save_store.hpp"

#include <array>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

using mka::save::NativeSaveStore;
using mka::save::SaveError;
using mka::save::SaveFailure;

namespace {
void require(bool ok, const char* what) {
    if (!ok) { std::cerr << what << '\n'; std::abort(); }
}
}

int main() {
#ifndef _WIN32
    SaveFailure f;
    require(!NativeSaveStore::open_or_create("/tmp/native-save-unsupported", f) &&
        f.kind == SaveError::unsupported_host, "unsupported host must be named");
#else
    const auto temp = std::filesystem::temp_directory_path();
    std::filesystem::path directory;
    for (int i = 0; i < 100; ++i) {
        directory = temp / ("mka-save-owned-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code ec;
        if (std::filesystem::create_directory(directory, ec)) break;
        require(i != 99, "disposable directory creation");
    }
    // The fixture deletes individual files and its own leaf directory only.
    // It never removes a user-selected tree or the temp parent.
    {
        SaveFailure f;
        auto opened = NativeSaveStore::open_or_create(directory, f);
        require(opened && f.kind == SaveError::none, "open disposable save store");
        NativeSaveStore store = std::move(*opened);
        require(store.validate(f), "retained directory identity");
        std::vector<std::string> names;
        require(store.list(names, f) && names.empty(), "empty enumeration");

        const std::array<std::uint8_t, 6> first{0, 1, 2, 0xff, 0, 7};
        require(store.replace("Profile_A.bin", first, f), "create profile bytes");
        std::vector<std::uint8_t> got;
        require(store.read("Profile_A.bin", got, f) &&
            got == std::vector<std::uint8_t>(first.begin(), first.end()),
            "exact profile bytes roundtrip");
        require(store.list(names, f) && names == std::vector<std::string>{"Profile_A.bin"},
            "enumerate exact profile spelling");

        const std::array<std::uint8_t, 3> replacement{9, 0, 8};
        require(store.replace("Profile_A.bin", replacement, f), "atomic replace");
        require(store.read("Profile_A.bin", got, f) &&
            got == std::vector<std::uint8_t>(replacement.begin(), replacement.end()),
            "replacement bytes visible");
        const auto held = CreateFileW((directory / "Profile_A.bin").c_str(),
            GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        require(held != INVALID_HANDLE_VALUE, "retain original file without delete sharing");
        require(!store.replace("Profile_A.bin", first, f) &&
            f.kind == SaveError::replace_failed,
            "failed publication preserves the original open file");
        require(!store.erase("Profile_A.bin", f) && f.kind == SaveError::delete_failed,
            "delete refusal preserves the original open file");
        require(store.read("Profile_A.bin", got, f) &&
            got == std::vector<std::uint8_t>(replacement.begin(), replacement.end()),
            "failed write/delete leave original bytes intact");
        require(CloseHandle(held), "release retained original file");
        std::size_t entry_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            require(entry.path().filename() == "Profile_A.bin",
                "failed publication retires only its owned temporary file");
            ++entry_count;
        }
        require(entry_count == 1, "no abandoned temporary file after failed publication");
        require(!store.read("profile_a.bin", got, f) && f.kind == SaveError::name_alias,
            "case alias does not impersonate exact profile identity");
        require(!store.replace("profile_a.bin", first, f) && f.kind == SaveError::name_alias,
            "case alias cannot replace different spelling");
        require(!store.read("../Profile_A.bin", got, f) &&
            f.kind == SaveError::unsafe_name, "traversal refused");
        require(!store.replace("CON.txt", first, f) &&
            f.kind == SaveError::unsafe_name, "device name refused");
        require(!store.read("missing.bin", got, f) &&
            f.kind == SaveError::missing, "missing read named");

        const std::array<std::uint8_t, 0> empty{};
        require(store.replace("Empty", empty, f), "empty bytes are valid payload");
        require(store.read("Empty", got, f) && got.empty(), "empty roundtrip");
        require(store.erase("Empty", f), "delete empty profile");
        require(store.erase("Profile_A.bin", f), "delete profile");
        require(!store.erase("Profile_A.bin", f) && f.kind == SaveError::missing,
            "missing delete named");
        require(store.list(names, f) && names.empty(), "empty after deletion");

        NativeSaveStore moved = std::move(store);
        require(!store.validate(f) && f.kind == SaveError::moved_store,
            "moved-from store refuses use");
        require(moved.validate(f), "moved store retains directory identity");
        require(moved.replace("Persistent", first, f),
            "write profile across store lifetime");
    }
    {
        SaveFailure f;
        auto reopened = NativeSaveStore::open_or_create(directory, f);
        require(bool(reopened), "reopen same disposable directory");
        const std::array<std::uint8_t, 6> expected{0, 1, 2, 0xff, 0, 7};
        std::vector<std::uint8_t> got;
        require(reopened->read("Persistent", got, f) &&
            got == std::vector<std::uint8_t>(expected.begin(), expected.end()),
            "profile bytes survive store retirement");
        require(reopened->erase("Persistent", f), "retire persisted profile");
    }
    std::error_code ec;
    require(std::filesystem::remove(directory, ec) && !ec,
        "remove exclusively created empty leaf directory");
#endif
    std::cout << "native_save_store: passed\n";
}
