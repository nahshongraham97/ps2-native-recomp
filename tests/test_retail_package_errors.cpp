#include "mka/retail_package.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
std::atomic<int> fail_after{-1};
void check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }

void put_word(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    check(at <= bytes.size() && bytes.size() - at >= 4, "synthetic word bounds");
    for (unsigned i = 0; i != 4; ++i) bytes[at+i] = static_cast<std::uint8_t>(value >> (8*i));
}
struct Row { std::string name; std::vector<std::uint8_t> payload; };
std::vector<std::uint8_t> make_pak(const std::vector<Row>& rows) {
    // The real 20-byte PAK header, twelve-byte row and trailing string-table
    // shape are used. These tiny payloads are only malformed-input probes.
    std::vector<std::uint8_t> bytes(20, 0);
    std::vector<std::uint32_t> offsets;
    offsets.reserve(rows.size());
    for (const auto& row : rows) {
        offsets.push_back(static_cast<std::uint32_t>(bytes.size()));
        bytes.insert(bytes.end(), row.payload.begin(), row.payload.end());
    }
    const auto table_offset = static_cast<std::uint32_t>(bytes.size());
    bytes.resize(bytes.size() + rows.size()*12, 0);
    std::vector<std::uint8_t> names;
    for (std::size_t i = 0; i != rows.size(); ++i) {
        const auto start = static_cast<std::uint32_t>(names.size());
        names.insert(names.end(), rows[i].name.begin(), rows[i].name.end());
        names.push_back(0);
        put_word(bytes, table_offset+i*12, start);
        put_word(bytes, table_offset+i*12+4, offsets[i]);
        put_word(bytes, table_offset+i*12+8, static_cast<std::uint32_t>(rows[i].payload.size()));
    }
    put_word(bytes, 0, 0x50414b20u);
    put_word(bytes, 4, 256);
    put_word(bytes, 8, static_cast<std::uint32_t>(rows.size()));
    put_word(bytes, 12, table_offset);
    put_word(bytes, 16, static_cast<std::uint32_t>(names.size()));
    bytes.insert(bytes.end(), names.begin(), names.end());
    return bytes;
}

class ExclusiveFile final {
public:
    explicit ExclusiveFile(const std::filesystem::path& path) {
        handle_ = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        check(handle_ != INVALID_HANDLE_VALUE, "exclusive synthetic PAK create");
    }
    ~ExclusiveFile() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    ExclusiveFile(const ExclusiveFile&)=delete;
    ExclusiveFile& operator=(const ExclusiveFile&)=delete;
    void write(const std::vector<std::uint8_t>& bytes) {
        std::size_t at = 0;
        while (at != bytes.size()) {
            DWORD written = 0;
            const auto request = static_cast<DWORD>(std::min<std::size_t>(bytes.size()-at, 1u<<20));
            check(WriteFile(handle_, bytes.data()+at, request, &written, nullptr) && written != 0,
                "synthetic PAK write");
            at += written;
        }
        check(FlushFileBuffers(handle_) != 0, "synthetic PAK flush");
    }
private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

std::filesystem::path create_case(const std::filesystem::path& root,
    const char* name, const std::vector<std::uint8_t>& bytes) {
    const auto folder = root / name;
    check(std::filesystem::create_directory(folder), "case folder must be new and exclusive");
    ExclusiveFile file(folder / L"mkda.pak");
    file.write(bytes);
    return folder;
}
std::optional<mka::content::NativeContentVolume> volume_at(const std::filesystem::path& folder) {
    mka::content::ContentVolumeFailure failure;
    auto volume = mka::content::NativeContentVolume::open(folder, failure);
    check(volume.has_value(), "native content volume for synthetic case");
    return volume;
}
void rejects(const std::filesystem::path& folder, RetailPackageError expected,
    const char* reason) {
    auto volume = volume_at(folder);
    RetailFilePathFailure path_failure;
    auto paths = NativeRetailFilePaths::create(path_failure);
    check(bool(paths), "real native alias owner for malformed package");
    RetailPackageFailure failure;
    auto package = NativeRetailPackage::open(*volume, *paths, failure);
    check(!package && failure.kind == expected, reason);
}
}

void* operator new(std::size_t n) {
    int left = fail_after.load(std::memory_order_relaxed);
    while (left >= 0) {
        if (left == 0) throw std::bad_alloc();
        if (fail_after.compare_exchange_weak(left, left-1, std::memory_order_relaxed)) break;
    }
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
    try {
        check(argc == 2 || argc == 3, "expected disposable root and optional externally prepared escape fixture");
        const std::filesystem::path supplied_root(argv[1]);
        check(supplied_root.is_absolute(), "disposable root argument must be absolute");
        check(std::filesystem::is_directory(supplied_root), "disposable root must already exist");
        const auto root = std::filesystem::canonical(supplied_root);
        const auto leaf = root.filename().string();
        check(leaf.find("disposable") != std::string::npos,
            "disposable root name must contain disposable");
        check(std::filesystem::is_empty(root), "disposable root must be empty; no existing case is replaced");

        if (argc == 3) {
            auto escaped_volume = volume_at(std::filesystem::path(argv[2]));
            mka::content::ContentVolumeFailure cf;
            check(!escaped_volume->open_directory_files("art", cf) &&
                cf.kind == mka::content::ContentVolumeError::root_escape,
                "actual escaping art junction is rejected before enumeration");
        }
        const std::string first = "\\ps2dvd\\art\\One.ssf";
        const auto archive = make_pak({{first, {0xa1, 0xa2}}});
        const auto archive_only = create_case(root, "archive-only", archive);
        rejects(archive_only, RetailPackageError::source_open,
            "valid PAK without art directory is refused; no archive fallback");
        const auto not_directory = create_case(root, "not-directory", archive);
        { ExclusiveFile file(not_directory / "art"); file.write({1}); }
        rejects(not_directory, RetailPackageError::source_open,
            "art regular file is not a directory");
        const auto empty = create_case(root, "empty-art", archive);
        check(std::filesystem::create_directory(empty / "art"), "new empty art directory");
        {
            auto volume = volume_at(empty);
            RetailFilePathFailure pf; auto paths = NativeRetailFilePaths::create(pf);
            RetailPackageFailure failure;
            auto package = NativeRetailPackage::open(*volume, *paths, failure);
            check(package && package->member_count() == 0, "empty native catalog owns no assets");
            check(!package->find(first, failure) && failure.kind == RetailPackageError::missing_member,
                "missing loose file stays missing even with matching valid PAK data present");
        }
        const auto nonregular = create_case(root, "nonregular", archive);
        check(std::filesystem::create_directories(nonregular / "art" / "One.ssf"), "asset directory fixture");
        rejects(nonregular, RetailPackageError::source_open, "nonregular asset cannot become empty successful data");
        const auto valid_folder = create_case(root, "loose-wins", archive);
        check(std::filesystem::create_directory(valid_folder / "art"), "new loose art directory");
        { ExclusiveFile file(valid_folder / "art" / "One.ssf"); file.write({0x21, 0x22}); }
        { ExclusiveFile file(valid_folder / "art" / "zero.ssf"); file.write({}); }
        { ExclusiveFile file(valid_folder / "art" / "short.ssf"); file.write({1, 2, 3, 4}); }
        auto volume = volume_at(valid_folder);
        {
            using C = mka::content::ContentVolumeError;
            mka::content::ContentVolumeFailure cf;
            check(!volume->open_directory_files("../art", cf) && cf.kind == C::traversal,
                "directory traversal rejected");
            check(!volume->open_directory_files("", cf) && cf.kind == C::invalid_key,
                "empty directory key rejected");
            auto retained = std::move(*volume);
            check(!volume->open_directory_files("art", cf) && cf.kind == C::moved_volume,
                "moved directory volume rejected");
            *volume = std::move(retained);
        }
        RetailFilePathFailure path_failure;
        auto paths = NativeRetailFilePaths::create(path_failure);
        check(bool(paths), "native alias owner before catalog fault");
        check(!paths->open_directory_files(*volume, "/unknown/art", path_failure) &&
            path_failure.kind == RetailFilePathError::unknown_alias,
            "directory listing cannot bypass the real alias service");
        RetailPackageFailure failure;
        std::unique_ptr<NativeRetailPackage> package;
        int cuts = 0;
        for (; cuts < 512; ++cuts) {
            fail_after.store(cuts, std::memory_order_relaxed);
            package = NativeRetailPackage::open(*volume, *paths, failure);
            fail_after.store(-1, std::memory_order_relaxed);
            if (package) break;
            check(failure.kind != RetailPackageError::none, "allocation cut cannot report success");
            // Failed publication must retire all opened child handles. An
            // outstanding retained directory handle would refuse this rename.
            const auto old = valid_folder / "art", renamed = valid_folder / "art-renamed";
            check(MoveFileExW(old.c_str(), renamed.c_str(), 0) != 0, "failure retires directory identity");
            check(MoveFileExW(renamed.c_str(), old.c_str(), 0) != 0, "restore disposable directory");
        }
        check(package && package->member_count() == 3 && cuts > 10, "allocation cuts eventually publish full native catalog");
        const auto member = package->find(first, failure);
        check(member && member->name == "One.ssf" && member->size == 2,
            "original name decodes to actual loose basename");
        check(package->find("oNE.SSF", failure).has_value(), "native basename casefold");
        std::array<std::uint8_t, 3> bytes{};
        const auto n = package->read(first, 0, bytes, failure);
        check(n && *n == 2 && bytes[0] == 0x21 && bytes[1] == 0x22,
            "actual loose bytes override different archive bytes and clip at file end");
        check(package->read("zero.ssf", 0, bytes, failure) == 0, "real empty file is zero-byte read");
        check(!package->find("absent.ssf", failure) && failure.kind == RetailPackageError::missing_member,
            "missing loose filename named");
        for (const auto* bad : {"", ".", "..", "../One.ssf", "other/One.ssf", "C:One.ssf", "\\foreign\\One.ssf"})
            check(!package->find(bad, failure) && failure.kind == RetailPackageError::invalid_name,
                "unverified/path traversal identifier refused");
        check(!package->read(first, member->size+1, bytes, failure) &&
            failure.kind == RetailPackageError::invalid_read_range, "past-end read named");
        bool foreign_thread_rejected = false;
        std::thread other([&] {
            RetailPackageFailure f; std::array<std::uint8_t, 1> one{};
            foreign_thread_rejected = !package->read(first, 0, one, f) && f.kind == RetailPackageError::wrong_thread;
        }); other.join();
        check(foreign_thread_rejected, "foreign thread rejected before cursor use");
        {
            const auto file = valid_folder / "art" / "short.ssf";
            const auto writer = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            check(writer != INVALID_HANDLE_VALUE, "open disposable truncation writer");
            const bool shortened = SetEndOfFile(writer) != 0;
            CloseHandle(writer);
            check(shortened, "truncate actual opened asset");
            check(!package->read("short.ssf", 0, bytes, failure) &&
                failure.kind == RetailPackageError::source_read,
                "shortened actual loose file returns named read failure");
        }
        volume.reset(); paths.reset();
        check(package->read("one.ssf", 0, bytes, failure) == 2 && bytes[0] == 0x21,
            "actual file and child directory survive volume and alias owners");
        const auto old = valid_folder / "art", renamed = valid_folder / "retired-art";
        check(MoveFileExW(old.c_str(), renamed.c_str(), 0) == 0,
            "live catalog retains directory identity against rename");
        package.reset();
        check(MoveFileExW(old.c_str(), renamed.c_str(), 0) != 0,
            "retirement releases directory identity");
        std::printf("PASS native loose assets, no PAK fallback, range/thread/lifetime and %d allocation cuts; no game or endpoint\n", cuts);
        return 0;
    } catch (const std::exception& e) {
        fail_after.store(-1, std::memory_order_relaxed);
        std::fprintf(stderr, "FAIL native package errors: %s\n", e.what());
        return 1;
    }
}
