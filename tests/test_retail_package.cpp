#include "mka/retail_package.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::vector<std::uint8_t> unhex(const std::string& value) {
    check(value.size() % 2 == 0, "manifest hex length");
    std::vector<std::uint8_t> result;
    for (std::size_t i = 0; i < value.size(); i += 2)
        result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i, 2), nullptr, 16)));
    return result;
}
}
// The expected manifest is extracted independently from the original archive
// and frozen before execution. No parser-produced offset is used as its oracle.
int main(int argc, char** argv) { try {
    check(argc == 3, "expected read-only original content root and frozen member manifest");
    mka::content::ContentVolumeFailure vf;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), vf);
    check(volume.has_value(), "actual original native content root");
    RetailPackageFailure f;
    auto package = NativeRetailPackage::open(*volume, f);
    check(bool(package), "retained original package opened and indexed");
    std::ifstream expected(argv[2]); check(bool(expected), "frozen independent manifest opened");
    std::string line; std::size_t rows = 0;
    while (std::getline(expected, line)) {
        std::istringstream fields(line);
        std::array<std::string, 6> data;
        for (auto& item : data) check(bool(std::getline(fields, item, '\t')), "complete independent manifest row");
        const auto index = std::stoul(data[0]);
        const auto offset = std::stoull(data[2]), size = std::stoull(data[3]);
        const auto first = unhex(data[4]), tail = unhex(data[5]);
        auto member = package->find(data[1], f);
        check(member && member->source_index == index && member->name == data[1] &&
            member->offset == offset && member->size == size, "native metadata equals frozen original index row");
        std::array<std::uint8_t, 32> actual{};
        auto n = package->read(data[1], 0, actual, f);
        check(n && *n == first.size() && std::equal(first.begin(), first.end(), actual.begin()),
            "native member bytes equal independent original first bytes");
        n = package->read(data[1], size - tail.size(), actual, f);
        check(n && *n == tail.size() && std::equal(tail.begin(), tail.end(), actual.begin()),
            "native member bytes equal independent original tail bytes");
        check(package->read(data[1], size, actual, f) == 0, "exact member end is zero-byte read");
        check(!package->read(data[1], size + 1, actual, f) && f.kind == RetailPackageError::invalid_read_range,
            "past member end is a named range failure");
        ++rows;
    }
    check(expected.eof() && rows == 245 && package->member_count() == rows, "all original245 index rows compared");
    const auto upper = package->find("\\PS2DVD\\ART\\WASTELANDS.SSF", f);
    check(upper && upper->offset == 1405845504 && upper->size == 2983936, "actual ASCII case-insensitive member lookup");
    check(!package->find("/PS2DVD/ART/WASTELANDS.SSF", f) && f.kind == RetailPackageError::missing_member,
        "member separators stay literal; archive path conversion is separate");
    check(!package->find("\\ps2dvd\\art\\absent.ssf", f) && f.kind == RetailPackageError::missing_member,
        "missing member cannot report success");
    bool wrong_thread = false;
    std::thread reader([&] {
        RetailPackageFailure other; std::array<std::uint8_t, 1> byte{};
        wrong_thread = !package->read("\\ps2dvd\\art\\armory.ssf", 0, byte, other) &&
            other.kind == RetailPackageError::wrong_thread;
    }); reader.join(); check(wrong_thread, "foreign thread rejected before archive cursor mutation");
    std::puts("PASS native original package all245 metadata/byte comparisons and range checks; no game or endpoint");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL native package: %s\n", e.what()); return 1;
}}
