#include "mka/art_section_data.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::content {
namespace {
constexpr std::size_t header_bytes = 28;
constexpr std::size_t record_bytes = 16;
constexpr std::uint32_t sec_magic = 0x53454320;
constexpr std::uint32_t type_mask = 0x3fffffff;

std::uint32_t le32(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    return std::to_integer<std::uint32_t>(bytes[offset]) |
        (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8) |
        (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16) |
        (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24);
}
bool within(std::uint64_t offset, std::uint64_t count, std::uint64_t size) noexcept {
    return offset <= size && count <= size - offset;
}
} // namespace

ArtSpriteData::ArtSpriteData(std::shared_ptr<const std::vector<std::byte>> bytes,
    std::size_t index, std::int32_t width, std::int32_t height,
    std::size_t offset, std::size_t length) noexcept
    : bytes_(std::move(bytes)), record_index_(index), encoded_offset_(offset),
      encoded_length_(length), width_(width), height_(height) {}

std::span<const std::byte> ArtSpriteData::encoded_bytes() const & noexcept {
    if (!bytes_) return {};
    return std::span<const std::byte>(*bytes_).subspan(encoded_offset_, encoded_length_);
}

std::optional<NativeArtSectionData> NativeArtSectionData::read(NativeContentFile& file,
    ArtSectionRange range, ArtSectionFailure& f) noexcept {
    f = {};
    try {
        if (!within(range.section_offset, range.section_length, range.member_length)) {
            f.kind = ArtSectionError::invalid_range;
            return {};
        }
        const auto file_length = file.length(f.file);
        if (!file_length) { f.kind = ArtSectionError::file_operation; return {}; }
        if (!within(range.member_offset, range.member_length, *file_length)) {
            f.kind = ArtSectionError::truncated;
            return {};
        }
        if (range.section_length < header_bytes) { f.kind = ArtSectionError::truncated; return {}; }
        const auto position = range.member_offset + range.section_offset;
        std::vector<std::byte> storage;
        if (position > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
            range.section_length > storage.max_size()) {
            f.kind = ArtSectionError::host_capacity;
            return {};
        }
        storage.resize(static_cast<std::size_t>(range.section_length));
        if (!file.seek(static_cast<std::int64_t>(position), SeekOrigin::begin, f.file)) {
            f.kind = ArtSectionError::file_operation;
            return {};
        }
        const auto got = file.read(storage.data(), storage.size(), f.file);
        if (got.error) { f.kind = ArtSectionError::file_operation; return {}; }
        if (got.bytes != storage.size()) { f.kind = ArtSectionError::truncated; return {}; }

        return parse(std::move(storage), f);
    } catch (const std::length_error&) { f.kind = ArtSectionError::host_capacity; }
      catch (const std::bad_alloc&) { f.kind = ArtSectionError::allocation_failed; }
      catch (...) { f.kind = ArtSectionError::native_exception; }
    return {};
}

std::optional<NativeArtSectionData> NativeArtSectionData::parse(std::vector<std::byte> input,
    ArtSectionFailure& f) noexcept {
    f = {};
    if(input.size()<header_bytes){f.kind=ArtSectionError::truncated;return {};}
    try {
        auto storage=std::make_shared<std::vector<std::byte>>(std::move(input));
        NativeArtSectionData result;
        const auto bytes = std::span<const std::byte>(*storage);
        for (std::size_t i = 0; i < result.header_.size(); ++i) result.header_[i] = le32(bytes, i * 4);
        if (result.header_[0] != sec_magic) { f.kind = ArtSectionError::unsupported_format; return {}; }
        const std::size_t count = result.header_[4];
        if (count > (bytes.size() - header_bytes) / record_bytes) {
            f.kind = ArtSectionError::malformed_directory;
            return {};
        }
        const std::size_t table_end = header_bytes + count * record_bytes;
        std::size_t names_end = bytes.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto payload = le32(bytes, header_bytes + i * record_bytes + 4);
            if (payload == 0) continue;
            if (payload < table_end || payload >= bytes.size()) {
                f.kind = ArtSectionError::malformed_directory;
                return {};
            }
            names_end = std::min(names_end, static_cast<std::size_t>(payload));
        }
        result.records_.reserve(count);
        const auto names = bytes.subspan(table_end, names_end - table_end);
        for (std::size_t i = 0; i < count; ++i) {
            const auto at = header_bytes + i * record_bytes;
            const auto name_offset = le32(bytes, at + 12);
            if (name_offset >= names.size()) { f.kind = ArtSectionError::malformed_name; return {}; }
            const auto remaining = names.subspan(name_offset);
            const auto end = std::find(remaining.begin(), remaining.end(), std::byte{0});
            if (end == remaining.end()) { f.kind = ArtSectionError::malformed_name; return {}; }
            auto name = std::string(reinterpret_cast<const char*>(remaining.data()),
                                    static_cast<std::size_t>(end - remaining.begin()));
            result.records_.push_back({le32(bytes, at), le32(bytes, at + 4),
                le32(bytes, at + 8), name_offset, std::move(name)});
        }
        result.bytes_ = std::move(storage);
        return result;
    } catch (const std::length_error&) { f.kind = ArtSectionError::host_capacity; }
      catch (const std::bad_alloc&) { f.kind = ArtSectionError::allocation_failed; }
      catch (...) { f.kind = ArtSectionError::native_exception; }
    return {};
}

std::optional<std::span<const std::byte>> NativeArtSectionData::resource_bytes(
    std::size_t index,ArtSectionFailure& f) const & noexcept {
    f={};if(!bytes_||index>=records_.size()){f.kind=ArtSectionError::missing_resource;return {};}
    const auto& entry=records_[index];const std::size_t start=entry.payload_offset;
    if(!start){f.kind=ArtSectionError::missing_payload;return {};}
    std::size_t end=bytes_->size();for(const auto& other:records_)if(other.payload_offset>start)end=std::min(end,std::size_t(other.payload_offset));
    return std::span<const std::byte>(*bytes_).subspan(start,end-start);
}

std::optional<ArtSpriteData> NativeArtSectionData::find_sprite(
    std::string_view name, ArtSectionFailure& f) const noexcept {
    f = {};
    if (!bytes_) { f.kind = ArtSectionError::missing_resource; return {}; }
    const auto entry = std::find_if(records_.begin(), records_.end(),
        [name](const ArtSectionRecord& record) { return record.name == name; });
    if (entry == records_.end()) { f.kind = ArtSectionError::missing_resource; return {}; }
    const auto kind = entry->raw_type & type_mask;
    if (kind != 2 && kind != 3) { f.kind = ArtSectionError::unsupported_resource_type; return {}; }
    const std::size_t start = entry->payload_offset;
    if (!start) { f.kind = ArtSectionError::missing_payload; return {}; }
    std::size_t end = bytes_->size();
    for (const auto& other : records_) {
        if (other.payload_offset > start) end = std::min(end, static_cast<std::size_t>(other.payload_offset));
    }
    const auto payload = std::span<const std::byte>(*bytes_).subspan(start, end - start);
    if (payload.size() < 5) { f.kind = ArtSectionError::malformed_resource; return {}; }
    const std::size_t dims = 5 + std::to_integer<unsigned>(payload[4]);
    if (dims >= payload.size() || payload.size() - dims <= 8) {
        f.kind = ArtSectionError::malformed_resource;
        return {};
    }
    const auto width = std::bit_cast<std::int32_t>(le32(payload, dims));
    const auto height = std::bit_cast<std::int32_t>(le32(payload, dims + 4));
    return ArtSpriteData(bytes_, static_cast<std::size_t>(entry - records_.begin()), width, height,
        start + dims + 8, payload.size() - dims - 8);
}
} // namespace mka::content
