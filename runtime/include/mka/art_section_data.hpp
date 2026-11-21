#pragma once

#include "mka/content_volume.hpp"
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mka::content {
enum class ArtSectionError {
    none, invalid_range, truncated, file_operation, unsupported_format,
    malformed_directory, malformed_name, missing_resource, missing_payload,
    unsupported_resource_type, malformed_resource, host_capacity, allocation_failed, native_exception
};

struct ArtSectionFailure {
    ArtSectionError kind{ArtSectionError::none};
    ContentVolumeFailure file;
};

struct ArtSectionRange {
    std::uint64_t member_offset, member_length;
    std::uint64_t section_offset, section_length;
};

struct ArtSectionRecord {
    std::uint32_t raw_type, payload_offset, raw_word_08, name_offset;
    std::string name;
};

class NativeArtSectionData;

// A retained serialized view. The encoded span is bounded by the next distinct
// payload or section end. It can include padding and is not a decoded texture.
class ArtSpriteData final {
public:
    ArtSpriteData(const ArtSpriteData&) = default;
    ArtSpriteData& operator=(const ArtSpriteData&) = default;
    ArtSpriteData(ArtSpriteData&&) noexcept = default;
    ArtSpriteData& operator=(ArtSpriteData&&) noexcept = default;
    std::size_t record_index() const noexcept { return record_index_; }
    std::int32_t width() const noexcept { return width_; }
    std::int32_t height() const noexcept { return height_; }
    std::size_t encoded_offset() const noexcept { return encoded_offset_; }
    std::span<const std::byte> encoded_bytes() const & noexcept;
    std::span<const std::byte> encoded_bytes() const && = delete;
private:
    friend class NativeArtSectionData;
    ArtSpriteData(std::shared_ptr<const std::vector<std::byte>> bytes, std::size_t index,
        std::int32_t width, std::int32_t height, std::size_t offset, std::size_t length) noexcept;
    std::shared_ptr<const std::vector<std::byte>> bytes_;
    std::size_t record_index_, encoded_offset_, encoded_length_;
    std::int32_t width_, height_;
};

class NativeArtSectionData final {
public:
    // Uses the supplied exclusive file cursor. The returned section owns bytes
    // independently of the file or volume. Range selection belongs to the caller.
    static std::optional<NativeArtSectionData> read(NativeContentFile&, ArtSectionRange,
        ArtSectionFailure&) noexcept;
    std::span<const ArtSectionRecord> records() const & noexcept { return records_; }
    std::span<const ArtSectionRecord> records() const && = delete;
    std::array<std::uint32_t, 7> header_words() const noexcept { return header_; }
    std::optional<ArtSpriteData> find_sprite(std::string_view, ArtSectionFailure&) const noexcept;
private:
    NativeArtSectionData() = default;
    std::shared_ptr<const std::vector<std::byte>> bytes_;
    std::array<std::uint32_t, 7> header_{};
    std::vector<ArtSectionRecord> records_;
};
} // namespace mka::content
