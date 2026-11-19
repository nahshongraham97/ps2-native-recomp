#include "mka/sound_archive.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
#include <thread>

namespace {
thread_local bool fail_next_allocation = false;
}
void* operator new(std::size_t size) {
    if (fail_next_allocation) { fail_next_allocation = false; throw std::bad_alloc(); }
    if (void* data = std::malloc(size ? size : 1)) return data;
    throw std::bad_alloc();
}
void operator delete(void* data) noexcept { std::free(data); }
void operator delete(void* data, std::size_t) noexcept { std::free(data); }

using namespace mka::sound;
namespace {
void check(bool value, const char* text) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
void put(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i != 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
struct Item { std::uint32_t key; std::string name; std::vector<std::uint8_t> data; };
std::vector<std::uint8_t> fixture(unsigned type, const std::vector<Item>& items,
                                  std::uint32_t sentinel_key = 0) {
    std::vector<std::uint8_t> names;
    if (!(sentinel_key & 0x20000000u)) names.push_back(0);
    for (const auto& item : items) if (!(item.key & 0x20000000u)) {
        names.insert(names.end(), item.name.begin(), item.name.end()); names.push_back(0);
    }
    const auto count = items.size() + 1, table = count * 12 + names.size();
    std::vector<std::uint8_t> result(12 + table);
    put(result, 0, type); put(result, 4, static_cast<std::uint32_t>(count));
    put(result, 8, static_cast<std::uint32_t>(table));
    put(result, 12, sentinel_key);
    std::copy(names.begin(), names.end(), result.begin() + 12 + count * 12);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i]; const auto at = 12 + (i + 1) * 12;
        put(result, at, item.key); put(result, at + 4, static_cast<std::uint32_t>(result.size()));
        put(result, at + 8, static_cast<std::uint32_t>(item.data.size()));
        result.insert(result.end(), item.data.begin(), item.data.end());
    }
    put(result, 16, static_cast<std::uint32_t>(result.size())); // sentinel EOF, length0
    return result;
}
void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    check(bool(file), "write isolated host fixture");
}
}
int main(int argc, char** argv) {
    const auto parent = std::filesystem::temp_directory_path();
    const auto dir = parent / ("mka-sound-archive-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir), "exclusive temporary directory");
    const auto path = dir / "archive.bin";
    const auto base = fixture(2, {{17, "alpha.mst", {1,2,3}},
                                  {0x20000002, {}, {4,5}}});
    write(path, base);
    ArchiveFailure error;
    auto archive = SoundArchive::open(path, error);
    check(archive && archive->directory_entry_count() == 3 && archive->named_entry_count() == 1,
          "count excludes name-tail bytes; named count excludes sentinel and numeric entry");
    {
        const auto other_path = dir / "other.bin";
        std::vector<Item> items{{1,"alpha.mst",{9,9,9}}};
        for (unsigned i = 0; i < 32; ++i) items.push_back({i+2,"extra"+std::to_string(i)+".mst",{8}});
        write(other_path, fixture(2, items));
        auto replacement = SoundArchive::open(other_path, error);
        check(bool(replacement), "open larger replacement archive");
        auto target = *archive;
        bool threw = false;
        fail_next_allocation = true;
        try { target = *replacement; } catch (const std::bad_alloc&) { threw = true; }
        fail_next_allocation = false;
        check(threw, "copy allocation failure injected");
        const auto retained = target.read_bank("alpha.msb", error);
        check(retained && retained->origin.key == 17 && retained->bytes == std::vector<std::uint8_t>({1,2,3}),
              "failed copy retains original index and backing together");
        target = *replacement;
        check(target.read_bank("alpha.msb", error)->bytes == std::vector<std::uint8_t>({9,9,9}),
              "successful copy replaces index and backing together");
        target = target;
        target = std::move(target);
        check(target.read_bank("alpha.msb", error)->origin.key == 1, "self copy/move preserves replacement identity");
    }
    check(std::filesystem::remove(dir / "other.bin"), "close/remove replacement fixture only");
    for (const auto request : {"alpha", "ALPHA.MSB", "dir.with.dot/sub/alpha.other",
                               "cdrom0:\\SNDSPS2\\Alpha.MSB;1"}) {
        const auto record = archive->find_bank(request, error);
        check(record && record->key == 17 && record->name == "alpha.mst", "basename/extension/ASCII fold uses actual identity");
    }
    const std::string nul_query("alpha.msb\0ignored", 17);
    check(bool(archive->find_bank(nul_query, error)), "query obeys original NUL termination");
    check(!archive->find_bank("missing.msb", error) && error.kind == ArchiveError::absent_bank,
          "unknown bank never aliases first bank");
    check(!archive->find_bank("", error) && error.kind == ArchiveError::absent_bank,
          "empty request cannot select sentinel");
    auto source = archive->read_bank("alpha.msb", error);
    check(source && source->bytes == std::vector<std::uint8_t>({1,2,3}), "native range returns exact bank extent");
    auto moved = std::move(*archive);
    check(!archive->read_bank("alpha.msb", error) && error.kind == ArchiveError::closed,
          "moved index has no backing and cannot serve stale data");
    archive.reset();
    auto backing = source->backing; const auto record = source->origin;
    source->bytes[0] = 99; source.reset();
    auto bytes = backing->read_bytes(record.offset, record.size, error);
    check(bytes && *bytes == std::vector<std::uint8_t>({1,2,3}), "returned copies do not mutate opened backing");
    check(backing->read_bytes(backing->size(), 0, error)->empty(), "zero-length EOF extent valid");
    check(!backing->read_bytes(UINT64_MAX, UINT64_MAX, error) && error.kind == ArchiveError::entry_out_of_bounds,
          "overflowing extent fails without wrap or allocation");
    check(bool(backing->read_bytes(record.offset, record.size, error)), "failed read leaves backing usable");
    std::atomic<bool> concurrent_ok{true};
    std::vector<std::thread> readers;
    for (unsigned thread = 0; thread < 4; ++thread) readers.emplace_back([&, thread] {
        for (unsigned repeat = 0; repeat < 250; ++repeat) {
            ArchiveFailure local_error;
            const auto at = (thread & 1u) ? record.offset : 0;
            const auto count = (thread & 1u) ? record.size : 12;
            const auto actual = backing->read_bytes(at, count, local_error);
            const std::vector<std::uint8_t> expected(base.begin() + at, base.begin() + at + count);
            if (!actual || *actual != expected || local_error.kind != ArchiveError::none) concurrent_ok = false;
        }
    });
    for (auto& reader : readers) reader.join();
    check(concurrent_ok, "sharing banks cannot race the native file cursor");
    // Close only native fixture owners before rewriting/deleting their files.
    backing.reset();
    { auto release = std::move(moved); }
    auto test_error = [&](std::vector<std::uint8_t> data, ArchiveError expected, const char* message) {
        write(path, data); const auto result = SoundArchive::open(path, error);
        check(!result && error.kind == expected, message);
    };
    test_error({1,2,3}, ArchiveError::truncated_header, "short header named error");
    auto bad = base; put(bad, 0, 3);
    test_error(bad, ArchiveError::unsupported_type, "unsupported archive type never guessed");
    bad = base; put(bad, 4, 0);
    test_error(bad, ArchiveError::invalid_count, "original positive signed count enforced for zero");
    bad = base; put(bad, 4, UINT32_MAX);
    test_error(bad, ArchiveError::invalid_count, "original positive signed count enforced for negative encoding");
    bad = base; put(bad, 4, 0x7fffffffu);
    test_error(bad, ArchiveError::directory_out_of_bounds, "count product checked before allocation");
    bad = base; put(bad, 8, UINT32_MAX);
    test_error(bad, ArchiveError::directory_out_of_bounds, "table extent checked against opened file");
    bad = base; put(bad, 28, UINT32_MAX);
    test_error(bad, ArchiveError::entry_out_of_bounds, "entry absolute offset checked");
    bad = base; put(bad, 32, UINT32_MAX);
    test_error(bad, ArchiveError::entry_out_of_bounds, "entry length checked without addition overflow");
    bad = base; put(bad, 36, 17);
    test_error(bad, ArchiveError::duplicate_key, "duplicate full keys explicitly unsupported");
    bad = fixture(2, {{1,"same.mst",{1}}, {2,"SAME.MST",{2}}});
    test_error(bad, ArchiveError::ambiguous_name, "ambiguous case-fold names explicitly unsupported");
    bad = base;
    const auto table_end = 12 + std::size_t(base[8] | (std::uint32_t(base[9]) << 8));
    bad[table_end - 1] = 'X';
    test_error(bad, ArchiveError::unterminated_name, "name scan cannot escape directory table");
    // Type1 retains numeric slots and continues naming later nonnumeric slots.
    write(path, fixture(1, {{0x121,"one.mst",{1}}, {0x20000002,{}, {2}}, {0x345,"two.mst",{3}}}));
    auto one = SoundArchive::open(path, error);
    check(one && one->named_entry_count() == 2 && one->find_bank("two.msb", error)->key == 0x345,
          "type1 numeric holes consume no name and cannot shift later identities");
    one.reset();
    write(path, fixture(2, {{17,"alpha.mst",{7}}}, 0x20000000u));
    auto numeric_sentinel = SoundArchive::open(path, error);
    check(numeric_sentinel && numeric_sentinel->named_entry_count() == 1 &&
          numeric_sentinel->read_bank("alpha.msb", error)->bytes == std::vector<std::uint8_t>({7}),
          "type2 cutoff scan starts at index1 even when sentinel0 has numeric bits");
    numeric_sentinel.reset();
    // No original 344-bank or 256-callback-slot capacity exists in this index.
    std::vector<Item> many;
    for (unsigned i = 0; i < 600; ++i) many.push_back({i+1,"bank"+std::to_string(i)+".mst",{std::uint8_t(i)}});
    write(path, fixture(2, many)); auto large = SoundArchive::open(path, error);
    check(large && large->named_entry_count() == 600 && large->find_bank("bank599.msb", error)->key == 600,
          "native directory grows beyond original device/callback counts");
    large.reset();
    if (argc == 2) {
        auto real = SoundArchive::open(argv[1], error);
        check(real && real->directory_entry_count() == 8705 && real->named_entry_count() == 344,
              "pinned real header and named prefix");
        const auto announcer = real->read_bank("announcr.msb", error);
        const auto shell = real->find_bank("shell.msb", error);
        check(announcer && announcer->origin.key == 1 && announcer->origin.offset == 0x1b0a8 &&
              announcer->bytes.size() == 28896 && announcer->bytes[0] == 11,
              "announcr is actual entry1, not invented permanent-bank label");
        check(shell && shell->key == 342 && shell->offset == 0x5c8484c,
              "shell resolves actual name instead of ordinal array1");
        check(!real->find_bank("not-a-retail-bank", error), "real archive unknown name absent");
        std::size_t loaded = 0, programs = 0;
        for (std::size_t i = 1; i < real->directory_records().size(); ++i) {
            const auto& entry = real->directory_records()[i];
            if (entry.name.empty()) continue;
            auto raw = real->read_bank(entry.name, error);
            check(raw && raw->origin.key == entry.key && raw->origin.offset == entry.offset,
                  "every real named bank keeps exact directory identity");
            BankError bank_error;
            auto parsed = SoundBank::parse(std::move(raw->bytes), bank_error);
            check(bool(parsed), "every actual named extent parses as its own bank");
            ++loaded; programs += parsed->program_count();
        }
        check(loaded == 344 && programs == 9656, "all real bank names and program totals match pinned source observations");
        std::printf("Real archive: %zu records, %zu names; %zu banks/%zu programs; announcr key1, shell key342\n",
                    real->directory_entry_count(), real->named_entry_count(), loaded, programs);
    }
    check(!SoundArchive::open(dir / "missing.bin", error) && error.kind == ArchiveError::open_failed,
          "missing native file named error");
    check(dir.parent_path() == parent && dir.filename().string().find("mka-sound-archive-") == 0,
          "cleanup confined to exclusive temporary directory");
    check(std::filesystem::remove_all(dir) == 2 && !std::filesystem::exists(dir), "remove fixture only");
    std::puts("sound archive tests passed");
}
