// Exhaustive native public-API verification. No reference arithmetic/device.
#include "mka/movie_gain.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::uint32_t end_bits = 0x7f800000u;
struct Row { std::uint32_t first, last; std::array<std::int32_t, 5> values; };
[[noreturn]] void fail(const char* label) { throw std::runtime_error(label); }
std::uint32_t hex_word(std::string_view word) {
    if (word.size() != 8) fail("reference endpoint must have exactly8 hex digits");
    for (char c : word)
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
            fail("reference endpoint must be uppercase hexadecimal");
    std::uint32_t value{};
    const auto result = std::from_chars(word.data(), word.data()+word.size(), value, 16);
    if (result.ec != std::errc{} || result.ptr != word.data()+word.size()) fail("reference hex parse");
    return value;
}
std::int32_t decimal(std::string_view word) {
    if (word.empty()) fail("empty reference result");
    std::int32_t value{};
    const auto result = std::from_chars(word.data(), word.data()+word.size(), value);
    if (result.ec != std::errc{} || result.ptr != word.data()+word.size() || value < -960 || value > 0)
        fail("invalid reference tenth-dB result");
    return value;
}
std::vector<Row> load(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("reference open failed");
    std::vector<Row> rows;
    std::string line;
    std::uint32_t next = 0;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::array<std::string_view, 7> fields{};
        std::string_view remaining(line);
        for (std::size_t i = 0; i < fields.size(); ++i) {
            const auto tab = remaining.find('\t');
            if (i+1 == fields.size()) {
                if (tab != remaining.npos) fail("extra reference field");
                fields[i] = remaining;
            } else {
                if (tab == remaining.npos) fail("missing reference field");
                fields[i] = remaining.substr(0, tab);
                remaining.remove_prefix(tab+1);
            }
        }
        Row row{hex_word(fields[0]), hex_word(fields[1]), {}};
        if (row.first != next || row.first > row.last || row.last >= end_bits)
            fail("reference gap/overlap/range");
        for (std::size_t i = 0; i < 5; ++i) row.values[i] = decimal(fields[i+2]);
        // Raw reference has exactly this known non-silent final overflow band.
        // Do not silently replace the input with a native-policy include.
        if (row.last < 0x7f000000u) {
            if (row.values[4] != -960) fail("unexpected reference final result");
        } else {
            if (row.first < 0x7f000000u || row.values != std::array<std::int32_t,5>{0,0,0,0,0})
                fail("reference exceptional band mismatch");
        }
        if (!rows.empty() && rows.back().values == row.values) fail("reference nonmaximal adjacent tuple");
        next = row.last+1;
        rows.push_back(row);
    }
    if (input.bad() || !input.eof()) fail("reference read failure");
    if (rows.empty() || next != end_bits || rows.back().first != 0x7f000000u)
        fail("reference incomplete domain/exception band");
    return rows;
}
}

int main(int argc, char** argv) try {
    if (argc != 2) fail("usage: test_movie_gain_domain raw-full-reference-intervals.tsv");
    const auto rows = load(argv[1]);
    std::size_t cursor = 0;
    std::uint64_t checks = 0, digest = 1469598103934665603ull;
    const auto began = std::chrono::steady_clock::now();
    auto last_progress = began;
    for (std::uint32_t magnitude = 0; magnitude < end_bits; ++magnitude) {
        if (magnitude > rows[cursor].last) ++cursor;
        const auto& reference = rows[cursor];
        for (std::uint32_t sign : {0u, 0x80000000u}) {
            const auto bits = magnitude | sign;
            const auto base = std::bit_cast<float>(bits);
            for (std::uint64_t stage = 0; stage < 5; ++stage) {
                mka::video::MovieGainFailure failure;
                const auto actual = stage == 0 ? mka::video::movie_gain_initial(base, failure) :
                    mka::video::movie_gain_fade({base, stage, 4}, failure);
                const auto expected = stage == 4 ? -960 : reference.values[stage];
                if (!actual || failure.kind != mka::video::MovieGainError::none || *actual != expected) {
                    std::fprintf(stderr, "FAIL bits=%08X stage=%llu expected=%d actual_present=%d actual=%d error=%s operation=%s\n",
                                 static_cast<unsigned>(bits), static_cast<unsigned long long>(stage), expected,
                                 actual.has_value(), actual.value_or(0), mka::video::movie_gain_error_name(failure.kind), failure.operation);
                    return 1;
                }
                ++checks;
                digest ^= static_cast<std::uint32_t>(*actual);
                digest *= 1099511628211ull;
            }
        }
        if (!(magnitude & 0xfffffu)) {
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now-last_progress).count() >= 5) {
                std::printf("{\"kind\":\"progress\",\"magnitude_patterns\":%llu,\"checks\":%llu,\"seconds\":%.6f}\n",
                            static_cast<unsigned long long>(magnitude)+1, static_cast<unsigned long long>(checks),
                            std::chrono::duration<double>(now-began).count());
                std::fflush(stdout); last_progress = now;
            }
        }
    }
    if (checks != 21390950400ull || cursor+1 != rows.size()) fail("final exhaustive count mismatch");
    std::printf("{\"kind\":\"complete\",\"magnitude_patterns\":2139095040,\"signed_inputs\":4278190080,\"checks\":%llu,\"reference_rows\":%llu,\"digest_fnv_word\":\"%016llX\",\"seconds\":%.9f,\"scope\":\"native gain API/source-derived stages0..3/native final mute; no profile loader/mixer/binary/hardware parity\"}\n",
                static_cast<unsigned long long>(checks), static_cast<unsigned long long>(rows.size()),
                static_cast<unsigned long long>(digest), std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count());
    if (std::fflush(stdout) != 0 || std::ferror(stdout)) fail("result output failure");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL movie gain domain: %s\n", error.what()); return 1;
}
