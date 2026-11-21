#include "mka/indexed_sprite_data.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <utility>

using namespace mka::content;
namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
std::uint64_t decimal(const char* text) {
    const std::string_view input(text);
    std::uint64_t value{};
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), value);
    check(parsed.ec == std::errc{} && parsed.ptr == input.data() + input.size(), "exact byte range");
    return value;
}
void put(std::vector<std::byte>& bytes, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = std::byte(static_cast<unsigned char>(value >> (8 * i)));
}
ArtSpriteData make_fixture(const std::filesystem::path& root, const char* key,
    std::span<const std::byte> encoded, std::size_t record_offset, std::size_t file_offset) {
    constexpr std::size_t sprite_prefix = 13;
    std::vector<std::byte> bytes(record_offset + sprite_prefix + encoded.size());
    put(bytes, 0, 0x53454320); put(bytes, 16, 1);
    put(bytes, 28, 2); put(bytes, 32, static_cast<std::uint32_t>(record_offset));
    bytes[44] = std::byte{0x58};
    put(bytes, record_offset + 5, 16); put(bytes, record_offset + 9, 16);
    std::copy(encoded.begin(), encoded.end(), bytes.begin() + record_offset + sprite_prefix);
    std::ofstream out(root / key, std::ios::binary);
    std::vector<char> prefix(file_offset);
    if (!prefix.empty()) out.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();
    check(bool(out), "write bounded fixture SEC");
    ContentVolumeFailure c;
    auto volume = NativeContentVolume::open(root, c);
    check(volume.has_value(), "fixture content owner");
    auto file = volume->open_resource(key, c);
    check(file.has_value(), "fixture opened actual file");
    ArtSectionFailure f;
    auto section = NativeArtSectionData::read(*file, {file_offset, bytes.size(), 0, bytes.size()}, f);
    check(section.has_value(), "fixture through actual SEC reader");
    auto sprite = section->find_sprite("X", f);
    check(sprite.has_value(), "fixture retains actual encoded sprite descriptor");
    return std::move(*sprite);
}
void synthetic(const std::filesystem::path& root, std::span<const std::byte> source,
               std::span<const StoredSpritePixel> expected) {
    constexpr std::size_t first_mode2 = 243;
    check(source.size() > first_mode2, "actual source fixture extent");
    IndexedSpriteFailure f;
    auto sprite = make_fixture(root, "relocated.sec", source, 128, 19);
    auto decoded = decode_indexed_sprite(sprite, f);
    check(decoded && std::ranges::equal(decoded->pixels(), expected),
          "separate host backing and absolute file origin preserve source alignment");
    // This relocation changes the serialized cursor. Regenerate its padding
    // instead of assuming the same bytes work at any section-relative offset.
    constexpr std::size_t shifted_mode2 = 227;
    std::vector<std::byte> shifted(source.begin(), source.begin() + 164);
    shifted.resize(shifted_mode2);
    shifted.insert(shifted.end(), source.begin() + first_mode2, source.end());
    sprite = make_fixture(root, "shifted.sec", shifted, 144, 7);
    decoded = decode_indexed_sprite(sprite, f);
    check(decoded && std::ranges::equal(decoded->pixels(), expected),
          "serialized padding derives from section cursor, not constant or pointer");

    sprite = make_fixture(root, "short-header.sec", source.first(150), 128, 0);
    check(!decode_indexed_sprite(sprite, f) && f.kind == IndexedSpriteError::truncated, "truncated header");
    sprite = make_fixture(root, "short-transfer.sec", source.first(first_mode2 + 0x6bf), 128, 0);
    check(!decode_indexed_sprite(sprite, f) && f.kind == IndexedSpriteError::truncated, "truncated mode-2 range");
    auto broken = std::vector<std::byte>(source.begin(), source.end());
    broken[12] = std::byte{0x41};
    sprite = make_fixture(root, "platform.sec", broken, 128, 0);
    check(!decode_indexed_sprite(sprite, f) && f.kind == IndexedSpriteError::unsupported_layout, "unknown platform");
    broken.assign(source.begin(), source.end());
    broken[88] ^= std::byte{1};
    sprite = make_fixture(root, "layout.sec", broken, 128, 0);
    check(!decode_indexed_sprite(sprite, f) && f.kind == IndexedSpriteError::unsupported_layout, "unknown raster layout");
    broken.assign(source.begin(), source.end());
    broken[first_mode2 + 0x100 + 32] ^= std::byte{1};
    sprite = make_fixture(root, "transfer.sec", broken, 128, 0);
    check(!decode_indexed_sprite(sprite, f) && f.kind == IndexedSpriteError::malformed_transfer, "wrong transfer dimensions");
}
} // namespace
int main(int argc, char** argv) {
    check(argc == 9, "output root, actual root/key/range and expected table");
    const auto output = std::filesystem::u8path(argv[1]);
    check(std::filesystem::create_directory(output), "exclusive fixture output root");
    ContentVolumeFailure cf;
    auto volume = NativeContentVolume::open(std::filesystem::u8path(argv[2]), cf);
    check(volume.has_value(), "actual content volume");
    auto file = volume->open_resource(argv[3], cf);
    check(file.has_value(), "actual opened archive");
    ArtSectionFailure sf;
    auto section = NativeArtSectionData::read(*file,
        {decimal(argv[4]), decimal(argv[5]), decimal(argv[6]), decimal(argv[7])}, sf);
    check(section.has_value(), "actual SEC range ingested");
    std::ifstream input(std::filesystem::u8path(argv[8]));
    std::size_t count{};
    check(bool(input >> count) && count == 2, "two external actual references");
    std::vector<std::pair<std::string, ArtSpriteData>> selected;
    for (std::size_t i = 0; i < count; ++i) {
        std::string name;
        std::int32_t width{}, height{};
        std::size_t offset{}, extent{};
        check(bool(input >> name >> width >> height >> offset >> extent), "complete external row");
        check((name == "BAR_GREEN" || name == "BAR_BOLT") &&
              std::none_of(selected.begin(), selected.end(), [&](const auto& old) { return old.first == name; }),
              "known distinct fixture names");
        auto sprite = section->find_sprite(name, sf);
        check(sprite && sprite->width() == width && sprite->height() == height &&
              sprite->encoded_offset() == offset && sprite->encoded_bytes().size() == extent,
              "actual encoded sprite matches source range");
        selected.emplace_back(std::move(name), std::move(*sprite));
    }
    std::string trailing;
    check(!(input >> trailing) && input.eof(), "exact source table extent");
    section.reset(); file.reset(); volume.reset();

    for (const auto& [name, sprite] : selected) {
        IndexedSpriteFailure f;
        auto image = decode_indexed_sprite(sprite, f);
        check(image && f.kind == IndexedSpriteError::none && image->width() == 16 &&
              image->height() == 16 && image->pixels().size() == 256, "actual owned decoded image");
        if (name == "BAR_GREEN") synthetic(output, sprite.encoded_bytes(), image->pixels());
        auto path = output / (name + ".rgba");
        check(!std::filesystem::exists(path), "unique decoded output");
        std::ofstream out(path, std::ios::binary);
        for (const auto& px : image->pixels())
            out.write(reinterpret_cast<const char*>(px.data()), static_cast<std::streamsize>(px.size()));
        out.close();
        check(bool(out), "write exact stored channel bytes for source reference comparison");
    }
    std::puts("PASS_NATIVE_INDEXED_SPRITE_DATA: actual decoded sprites=2 with malformed and serialized-alignment coverage; stored pixels only, no renderer or game claim");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
