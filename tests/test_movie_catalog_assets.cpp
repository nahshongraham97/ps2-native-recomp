// Isolated source-request -> existing native asset-store integration fixture.
// No decoder, player, audio, graphics, retail executable or guest ABI is used.
#include "mka/movie_catalog.hpp"
#include "mka/movie_assets.hpp"
#include <array>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Expected {
    std::uint64_t bytes;
    std::array<unsigned char, 8> first, last;
};
std::array<unsigned char, 8> hex_bytes(std::string_view text) {
    check(text.size() == 16, "fixture byte identity has wrong length");
    std::array<unsigned char, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        unsigned value{};
        const auto parsed = std::from_chars(text.data()+2*i, text.data()+2*i+2, value, 16);
        check(parsed.ec == std::errc{} && parsed.ptr == text.data()+2*i+2,
              "fixture byte identity is not hexadecimal");
        bytes[i] = static_cast<unsigned char>(value);
    }
    return bytes;
}
std::map<std::string, Expected> load(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    check(bool(stream), "cannot open explicit test manifest");
    std::map<std::string, Expected> entries;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::array<std::string_view, 4> fields{};
        std::string_view tail(line);
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto tab = tail.find('\t');
            if (i+1 == fields.size()) {
                check(tab == tail.npos, "extra test manifest field");
                fields[i] = tail;
            } else {
                check(tab != tail.npos, "missing test manifest field");
                fields[i] = tail.substr(0, tab);
                tail.remove_prefix(tab+1);
            }
        }
        check(!fields[0].empty(), "empty test manifest key");
        std::uint64_t bytes{};
        const auto number = std::from_chars(fields[1].data(), fields[1].data()+fields[1].size(), bytes);
        check(number.ec == std::errc{} && number.ptr == fields[1].data()+fields[1].size() && bytes >= 8,
              "invalid test manifest length");
        check(entries.emplace(std::string(fields[0]),
              Expected{bytes, hex_bytes(fields[2]), hex_bytes(fields[3])}).second,
              "duplicate test manifest key");
    }
    check(stream.eof() && !stream.bad(), "test manifest read failed");
    check(entries.size() == 38, "fixture must contain all38 pinned actual assets");
    return entries;
}
}

int main(int argc, char** argv) try {
    check(argc == 3, "usage: test_movie_catalog_assets exact_MOVIEPS2_root explicit_manifest.tsv");
    const auto expected = load(argv[2]);
    std::vector<mka::video::MovieAssetCatalogEntry> bindings;
    for (const auto& [key, value] : expected) bindings.push_back({key, key});
    mka::video::MovieAssetFailure failure;
    auto store = mka::video::NativeMovieAssetStore::create(std::filesystem::u8path(argv[1]), bindings, failure);
    check(store.has_value() && failure.kind == mka::video::MovieAssetError::none,
          "the38 actual bindings must create independently of unavailable source requests");
    const std::set<std::string> unavailable{"BLITZN.SFD", "BLITZW.SFD", "KRYPT/V_FIN.SFD"};
    std::set<std::string> opened, missing;
    std::size_t successes = 0, failures = 0;
    for (std::int64_t id = 0; id < 33; ++id) {
        for (auto view : {mka::video::MovieViewSelection::normal, mka::video::MovieViewSelection::alternate}) {
            mka::video::MovieCatalogFailure request_failure;
            auto request = mka::video::RetailMovieCatalog{}.resolve(id, view, 0, request_failure);
            check(request.has_value() && request_failure.kind == mka::video::MovieCatalogError::none,
                  "source request must resolve before independent native file preflight");
            auto asset = store->open(request->logical_key, failure);
            if (unavailable.contains(request->logical_key)) {
                check(!asset && failure.kind == mka::video::MovieAssetError::unknown_key,
                      "unavailable request must fail named without another file being selected");
                missing.insert(request->logical_key); ++failures;
                continue;
            }
            check(asset.has_value() && failure.kind == mka::video::MovieAssetError::none,
                  "available exact source request must open natively");
            check(asset->logical_key() == request->logical_key, "native asset identity changed");
            const auto wanted = expected.find(request->logical_key);
            check(wanted != expected.end(), "opened request is absent from pinned input manifest");
            check(asset->length(failure) == wanted->second.bytes &&
                  failure.kind == mka::video::MovieAssetError::none, "native asset length mismatch");
            std::array<unsigned char, 8> first{}, last{};
            const auto first_read = asset->read(first.data(), first.size(), failure);
            check(!first_read.error && first_read.bytes == first.size() && first == wanted->second.first,
                  "native asset prefix mismatch");
            check(asset->seek(-8, mka::content::SeekOrigin::end, failure), "native asset tail seek failed");
            const auto last_read = asset->read(last.data(), last.size(), failure);
            check(!last_read.error && last_read.bytes == last.size() && last == wanted->second.last,
                  "native asset suffix mismatch");
            opened.insert(request->logical_key); ++successes;
        }
    }
    check(opened.size() == 38 && successes == 62 && failures == 4 && missing == unavailable,
          "actual request/asset coverage is incomplete or substituted");
    std::printf("{\"status\":\"PASS_NATIVE_CATALOG_ASSET_BINDING\",\"requests\":66,"
                "\"opened_requests\":%zu,\"distinct_opened\":%zu,\"missing_requests\":%zu,"
                "\"distinct_missing\":%zu,\"game_or_device_launched\":false}\n",
                successes, opened.size(), failures, missing.size());
    check(std::fflush(stdout) == 0 && !std::ferror(stdout), "test output write failed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL native movie catalog assets: %s\n", error.what());
    return 1;
}
