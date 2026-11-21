#include "mka/retail_file_paths.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace mka::retail;

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    std::filesystem::path root;
    std::array<std::filesystem::path, 8> files;
    std::array<std::filesystem::path, 7> directories;
    bool owns_root = false;

    void cleanup() noexcept {
        if (!owns_root) return;
        std::error_code ignored;
        for (const auto& file : files) if (!file.empty()) std::filesystem::remove(file, ignored);
        for (auto it = directories.rbegin(); it != directories.rend(); ++it)
            if (!it->empty()) std::filesystem::remove(*it, ignored);
        std::filesystem::remove(root, ignored);
    }

    Fixture() {
        try {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() /
                ("mka_retail_file_paths_" + std::to_string(nonce));
            check(std::filesystem::create_directory(root), "new private fixture root");
            owns_root = true;
            directories = {root / "art", root / "sndsps2", root / "logs",
                           root / "movieps2", root / "movieps2" / "krypt",
                           root / "anims", root / "mko"};
            for (const auto& directory : directories)
                check(std::filesystem::create_directory(directory), "fixture directory created");
            files = {root / "mkda.pak", root / "art" / "a.bin",
                     root / "sndsps2" / "a.bin", root / "logs" / "a.bin",
                     root / "movieps2" / "a.bin", root / "movieps2" / "krypt" / "a.bin",
                     root / "anims" / "a.bin", root / "mko" / "a.bin"};
            for (std::size_t i = 0; i < files.size(); ++i) {
                std::ofstream out(files[i], std::ios::binary);
                check(bool(out), "fixture file created");
                out.put(static_cast<char>('A' + i));
                check(bool(out), "fixture byte written");
            }
        } catch (...) {
            cleanup();
            throw;
        }
    }
    ~Fixture() { cleanup(); }
};
} // namespace

int main() {
    try {
        Fixture fixture;
        mka::content::ContentVolumeFailure volume_failure;
        auto volume = mka::content::NativeContentVolume::open(fixture.root, volume_failure);
        check(volume.has_value(), "native fixture content volume opened");

        RetailFilePathFailure failure;
        auto paths = NativeRetailFilePaths::create(failure);
        check(bool(paths) && failure.kind == RetailFilePathError::none,
              "eight source aliases constructed in order");
        constexpr std::array<std::string_view, 8> requests{
            "/game/mkda.pak", "/art/a.bin", "/sounds/a.bin", "/logs/a.bin",
            "/movies/a.bin", "/kryptmovies/a.bin", "/anims/a.bin", "/mko/a.bin"};
        constexpr std::array<std::string_view, 8> expected_keys{
            "mkda.pak", "art/a.bin", "sndsps2/a.bin", "logs/a.bin",
            "movieps2/a.bin", "movieps2/krypt/a.bin", "anims/a.bin", "mko/a.bin"};
        for (std::size_t i = 0; i < requests.size(); ++i) {
            auto file = paths->open(*volume, requests[i], failure);
            check(file.has_value() && file->logical_key() == expected_keys[i],
                  "source alias reaches exact host volume key");
            char byte = 0;
            const auto read = file->read(&byte, 1, failure.content);
            check(read.bytes == 1 && !read.error && byte == 'A' + static_cast<int>(i),
                  "source alias opens actual native file owner");
        }
        auto upper = paths->open(*volume, "/GAME/mkda.pak", failure);
        check(upper && upper->logical_key() == "mkda.pak", "source alias case fold");
        auto builtin = paths->open(*volume, "/cdrom/mkda.pak", failure);
        check(builtin && builtin->logical_key() == "mkda.pak", "source cdrom provider root");
        check(!paths->open(*volume, "/game", failure) &&
                  failure.kind == RetailFilePathError::empty_file_key,
              "provider root is not a file");
        check(!paths->open(*volume, "game/mkda.pak", failure) &&
                  failure.kind == RetailFilePathError::invalid_source_path,
              "source alias path requires initial slash");
        check(!paths->open(*volume, "/missing/a.bin", failure) &&
                  failure.kind == RetailFilePathError::unknown_alias,
              "unknown alias fails by name");
        check(!paths->open(*volume, "/abcdefghijklmnopqrstuvwxyzabcdefg/a.bin", failure) &&
                  failure.kind == RetailFilePathError::unknown_alias,
              "long unknown alias is looked up without a console buffer cap");
        check(!paths->open(*volume, "/game/../a.bin", failure) &&
                  failure.kind == RetailFilePathError::content_file &&
                  failure.content.kind == mka::content::ContentVolumeError::traversal,
              "host volume rejects traversal after alias resolution");
        // The volume grammar allows one optional leading separator. Test a
        // genuinely empty interior component after a non-root alias prefix.
        check(!paths->open(*volume, "/art//a.bin", failure) &&
                  failure.kind == RetailFilePathError::content_file &&
                  failure.content.kind == mka::content::ContentVolumeError::invalid_key,
              "host volume rejects empty interior path component");
        auto optional_root_separator = paths->open(*volume, "/game//mkda.pak", failure);
        check(optional_root_separator && optional_root_separator->logical_key() == "mkda.pak",
              "existing native volume grammar consumes its optional leading separator");
        check(!paths->open(*volume, "/game/CON", failure) &&
                  failure.kind == RetailFilePathError::content_file &&
                  failure.content.kind == mka::content::ContentVolumeError::unsafe_component,
              "host volume rejects reserved host component");
        auto retained = paths->open(*volume, "/game/mkda.pak", failure);
        check(retained && failure.kind == RetailFilePathError::none,
              "successful open clears prior path failure");
        paths.reset();
        char retained_byte = 0;
        const auto retained_read = retained->read(&retained_byte, 1, failure.content);
        check(retained_read.bytes == 1 && !retained_read.error && retained_byte == 'A',
              "opened file survives alias owner retirement");
        std::puts("PASS retail native source aliases and owned file route");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL retail native file paths: %s\n", error.what());
        return 1;
    }
}
