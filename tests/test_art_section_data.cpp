#include "mka/art_section_data.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

using namespace mka::content;
namespace {
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
std::uint64_t decimal(const char* text) {
    std::uint64_t value{};
    const std::string_view input(text);
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
    check(result.ec == std::errc{} && result.ptr == input.data() + input.size(), "exact decimal range");
    return value;
}
void put(std::vector<std::byte>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = std::byte(static_cast<unsigned char>(value >> (8 * i)));
}
std::vector<std::byte> section_sample() {
    std::vector<std::byte> bytes(117);
    put(bytes, 0, 0x53454320); put(bytes, 4, 4); put(bytes, 16, 2);
    put(bytes, 28, 0x80000002); put(bytes, 32, 72); put(bytes, 36, 0xffffffff);
    put(bytes, 44, 3); put(bytes, 48, 94); put(bytes, 52, 0);
    const std::string_view name = "BAR_GREEN";
    for (std::size_t i = 0; i < name.size(); ++i)
        bytes[60 + i] = std::byte(static_cast<unsigned char>(name[i]));
    bytes[76] = std::byte{1}; bytes[77] = std::byte{0xA0};
    put(bytes, 78, std::bit_cast<std::uint32_t>(std::int32_t{-16})); put(bytes, 82, 9);
    bytes[86] = std::byte{0x9a}; bytes[93] = std::byte{0x17};
    return bytes;
}
std::optional<NativeArtSectionData> write_read(const std::filesystem::path& root,
    const NativeContentVolume& volume, std::string_view key, std::vector<std::byte> bytes,
    ArtSectionFailure& failure) {
    const auto path = root / std::string(key);
    std::ofstream out(path, std::ios::binary);
    const char prefix[11]{}, suffix[7]{};
    out.write(prefix, sizeof prefix);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.write(suffix, sizeof suffix);
    out.close();
    check(bool(out), "actual synthetic backing fixture file");
    ContentVolumeFailure file_failure;
    auto file = volume.open_resource(key, file_failure);
    check(file.has_value(), "actual synthetic owned file");
    return NativeArtSectionData::read(*file, {3, 8 + bytes.size(), 8, bytes.size()}, failure);
}
void synthetic(const std::filesystem::path& root) {
    check(std::filesystem::create_directory(root), "exclusive synthetic fixture root");
    ContentVolumeFailure file_failure;
    auto volume = NativeContentVolume::open(root, file_failure);
    check(volume.has_value(), "actual synthetic content volume");
    ArtSectionFailure f;
    auto bytes = section_sample();
    auto section = write_read(root, *volume, "valid.sec", bytes, f);
    check(section && section->records().size() == 2 && section->records()[0].raw_word_08 == 0xffffffff,
          "valid offsets with opaque +08 and type flags preserved");
    auto sprite = section->find_sprite("BAR_GREEN", f);
    check(sprite && sprite->record_index() == 0 && sprite->width() == -16 && sprite->height() == 9 &&
          sprite->encoded_offset() == 86 && sprite->encoded_bytes().size() == 8,
          "first exact-name record, signed dimensions, independently bounded encoded bytes");
    check(!section->find_sprite("bar_green", f) && f.kind == ArtSectionError::missing_resource,
          "lookup uses exact byte identity");
    auto moved = std::move(*section);
    check(!section->find_sprite("BAR_GREEN", f) && f.kind == ArtSectionError::missing_resource,
          "moved section cannot supply resources");
    section.reset();
    { auto discarded = std::move(moved); }
    check(sprite->encoded_bytes().front() == std::byte{0x9a} && sprite->encoded_bytes().back() == std::byte{0x17},
          "sprite span survives section and file teardown");

    auto file = volume->open_resource("valid.sec", file_failure);
    check(file.has_value(), "reopen validation fixture");
    check(!NativeArtSectionData::read(*file, {0, 32, 31, 2}, f) && f.kind == ArtSectionError::invalid_range,
          "reject section beyond its member");
    check(!NativeArtSectionData::read(*file, {0, 1000, 11, 117}, f) && f.kind == ArtSectionError::truncated,
          "reject member beyond opened file");
    check(!NativeArtSectionData::read(*file, {0, 32, 0, 20}, f) && f.kind == ArtSectionError::truncated,
          "reject short SEC header");
    auto broken = bytes; put(broken, 0, 0);
    check(!write_read(root, *volume, "magic.sec", broken, f) && f.kind == ArtSectionError::unsupported_format,
          "bad magic rejected");
    broken = bytes; put(broken, 16, 0xffffffff);
    check(!write_read(root, *volume, "directory.sec", broken, f) && f.kind == ArtSectionError::malformed_directory,
          "directory multiplication bounded by actual extent");
    broken = bytes; put(broken, 32, 59);
    check(!write_read(root, *volume, "payload.sec", broken, f) && f.kind == ArtSectionError::malformed_directory,
          "payload cannot point inside directory");
    broken = bytes; std::fill(broken.begin() + 60, broken.begin() + 72, std::byte{0x42});
    check(!write_read(root, *volume, "name.sec", broken, f) && f.kind == ArtSectionError::malformed_name,
          "name must terminate before first payload");
    broken = bytes; put(broken, 28, 4);
    auto unsupported = write_read(root, *volume, "type.sec", broken, f);
    check(unsupported && !unsupported->find_sprite("BAR_GREEN", f) && f.kind == ArtSectionError::unsupported_resource_type,
          "first matching unsupported record cannot select later duplicate");
    broken = bytes; put(broken, 32, 0);
    auto missing = write_read(root, *volume, "missing.sec", broken, f);
    check(missing && !missing->find_sprite("BAR_GREEN", f) && f.kind == ArtSectionError::missing_payload,
          "zero payload is absent, not offset zero texture");
    broken = bytes; broken[76] = std::byte{0xff};
    auto prefix = write_read(root, *volume, "prefix.sec", broken, f);
    check(prefix && !prefix->find_sprite("BAR_GREEN", f) && f.kind == ArtSectionError::malformed_resource,
          "length-prefixed sprite record cannot consume the next payload");
}
std::size_t actual(char** argv) {
    const auto root = std::filesystem::u8path(argv[2]);
    const ArtSectionRange range{decimal(argv[4]), decimal(argv[5]), decimal(argv[6]), decimal(argv[7])};
    ContentVolumeFailure cf;
    auto volume = NativeContentVolume::open(root, cf);
    check(volume.has_value(), "real content volume");
    auto file = volume->open_resource(argv[3], cf);
    check(file.has_value(), "real opened archive file");
    ArtSectionFailure f;
    auto section = NativeArtSectionData::read(*file, range, f);
    check(section.has_value(), "read actual selected section range");
    std::ifstream expected(std::filesystem::u8path(argv[8]));
    std::size_t records{}, resources{};
    check(bool(expected >> records >> resources) && records == section->records().size() && resources > 0,
          "real serialized record count agrees with external evidence");
    // Fixture-only independent file read from the same caller-supplied plain resource path.
    std::ifstream raw(root / std::filesystem::u8path(argv[3]), std::ios::binary);
    check(bool(raw), "direct offline comparison file");
    std::vector<ArtSpriteData> retained;
    std::vector<std::vector<std::byte>> reference;
    for (std::size_t i = 0; i < resources; ++i) {
        std::string name; std::size_t index{}, encoded{}, count{};
        std::uint32_t kind{}, payload{}, opaque{}, name_offset{};
        std::int32_t width{}, height{};
        check(bool(expected >> name >> index >> kind >> payload >> opaque >> name_offset >> width >> height >> encoded >> count),
              "complete source-derived expected resource row");
        auto sprite = section->find_sprite(name, f);
        check(sprite && index < section->records().size() && sprite->record_index() == index &&
              sprite->width() == width && sprite->height() == height &&
              sprite->encoded_offset() == encoded && sprite->encoded_bytes().size() == count,
              "actual serialized type dimensions and envelope agree");
        const auto& record = section->records()[index];
        check(record.name == name && record.raw_type == kind && record.payload_offset == payload &&
              record.raw_word_08 == opaque && record.name_offset == name_offset, "exact actual directory words and name");
        const auto absolute = range.member_offset + range.section_offset + encoded;
        check(absolute <= static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) &&
              count <= static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()),
              "independent bounded file offset");
        raw.seekg(static_cast<std::streamoff>(absolute));
        reference.emplace_back(count);
        raw.read(reinterpret_cast<char*>(reference.back().data()), static_cast<std::streamsize>(count));
        check(bool(raw), "read complete independent encoded bytes");
        retained.push_back(std::move(*sprite));
    }
    std::string extra;
    check(!(expected >> extra) && expected.eof(), "source expectation rows consumed exactly");
    section.reset(); file.reset(); volume.reset();
    for (std::size_t i = 0; i < retained.size(); ++i)
        check(std::ranges::equal(retained[i].encoded_bytes(), reference[i]),
              "retained actual encoded bytes equal independent bounded file bytes");
    return resources;
}
} // namespace
int main(int argc, char** argv) {
    check(argc == 2 || argc == 9, "synthetic root, optionally actual root/key/range and expected table");
    synthetic(std::filesystem::u8path(argv[1]));
    const auto resources = argc == 9 ? actual(argv) : 0;
    std::printf("PASS_NATIVE_ART_SECTION_DATA: bounded owned SEC ingress and malformed cases; actual encoded sprites=%zu; no texture readiness or game claim\n", resources);
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
