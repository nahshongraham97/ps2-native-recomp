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
    RetailPackageFailure failure;
    auto package = NativeRetailPackage::open(*volume, failure);
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
        check(argc == 2, "expected explicitly disposable root argument");
        const std::filesystem::path supplied_root(argv[1]);
        check(supplied_root.is_absolute(), "disposable root argument must be absolute");
        check(std::filesystem::is_directory(supplied_root), "disposable root must already exist");
        const auto root = std::filesystem::canonical(supplied_root);
        const auto leaf = root.filename().string();
        check(leaf.find("disposable") != std::string::npos,
            "disposable root name must contain disposable");
        check(std::filesystem::is_empty(root), "disposable root must be empty; no existing case is replaced");

        const std::string first = "\\ps2dvd\\art\\One.ssf";
        const std::string second = "\\PS2DVD\\ART\\oNE.SSF";
        auto good = make_pak({{first, {0x11, 0x12}}, {second, {0x21, 0x22}}});
        auto short_header = std::vector<std::uint8_t>(good.begin(), good.begin()+19);
        rejects(create_case(root, "truncated-header", short_header),
            RetailPackageError::invalid_header, "truncated header rejected");
        auto bad_magic = good; put_word(bad_magic, 0, 0x12345678u);
        rejects(create_case(root, "bad-magic", bad_magic),
            RetailPackageError::invalid_header, "native format guard rejects bad magic");
        auto bad_version = good; put_word(bad_version, 4, 0x101u);
        rejects(create_case(root, "bad-version", bad_version),
            RetailPackageError::invalid_header, "native format guard rejects bad version");
        auto bad_index = good; put_word(bad_index, 12, static_cast<std::uint32_t>(bad_index.size()+1));
        rejects(create_case(root, "bad-index", bad_index),
            RetailPackageError::invalid_index, "out-of-file table rejected");
        auto bad_name_offset = good; put_word(bad_name_offset, 24, 0xffffffffu);
        rejects(create_case(root, "bad-name-offset", bad_name_offset),
            RetailPackageError::invalid_name, "out-of-string-table name offset rejected");
        auto no_terminator = make_pak({{first, {0x11}}});
        no_terminator.back() = 'X';
        rejects(create_case(root, "unterminated-name", no_terminator),
            RetailPackageError::invalid_name, "unterminated member name rejected");
        auto bad_member = good; put_word(bad_member, 28, 0xffffffffu);
        rejects(create_case(root, "bad-member-range", bad_member),
            RetailPackageError::invalid_entry_range, "out-of-file member range rejected");

        const auto valid_folder = create_case(root, "duplicate-casefold", good);
        auto volume = volume_at(valid_folder);
        RetailPackageFailure failure;
        fail_after.store(0, std::memory_order_relaxed);
        auto failed = NativeRetailPackage::open(*volume, failure);
        fail_after.store(-1, std::memory_order_relaxed);
        check(!failed && failure.kind == RetailPackageError::allocation,
            "allocation fault before publication reports failure");
        auto package = NativeRetailPackage::open(*volume, failure);
        check(bool(package) && package->member_count() == 2, "retry opens actual bounded index");
        const auto member = package->find(first, failure);
        check(member && member->source_index == 1 && member->name == second,
            "casefold duplicate replaces lookup value with last source row");
        std::array<std::uint8_t, 3> bytes{};
        const auto n = package->read(first, 0, bytes, failure);
        check(n && *n == 2 && bytes[0] == 0x21 && bytes[1] == 0x22,
            "last duplicate row owns returned member bytes and clips at its end");
        check(!package->find("\\ps2dvd\\art\\absent.ssf", failure) &&
            failure.kind == RetailPackageError::missing_member, "missing member named");
        check(!package->read(first, member->size+1, bytes, failure) &&
            failure.kind == RetailPackageError::invalid_read_range, "past-end read named");
        bool foreign_thread_rejected = false;
        std::thread other([&] {
            RetailPackageFailure f;
            std::array<std::uint8_t, 1> one{};
            foreign_thread_rejected = !package->read(first, 0, one, f) &&
                f.kind == RetailPackageError::wrong_thread;
        });
        other.join();
        check(foreign_thread_rejected, "foreign thread rejected before cursor use");
        std::puts("PASS native package malformed/disposable file cases; no game or endpoint");
        return 0;
    } catch (const std::exception& e) {
        fail_after.store(-1, std::memory_order_relaxed);
        std::fprintf(stderr, "FAIL native package errors: %s\n", e.what());
        return 1;
    }
}
