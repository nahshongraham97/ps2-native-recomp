#pragma once
#include "mka/retail_archive.hpp"
#include "mka/art_section_data.hpp"
#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace mka::retail {
enum class RetailSsfBundleError {
    none, archive, unsupported_format, invalid_header, invalid_directory,
    invalid_section_index, invalid_payload_range, overlapping_payloads,
    unsupported_section_type, unsupported_section_format, art_section,
    invalid_resource_index, missing_resource, ambiguous_resource,
    unsupported_resource_type, missing_payload, invalid_resource_length,
    unsupported_clump, malformed_clump, invalid_texture_range,
    host_capacity, allocation, native_exception
};
struct RetailSsfBundleFailure {
    RetailSsfBundleError kind{RetailSsfBundleError::none};
    const char* operation{""};
    RetailArchiveFailure archive{};
    content::ArtSectionFailure section{};
};
struct RetailSsfSectionRecord {
    std::size_t index{};
    std::uint32_t raw_type{};
    std::uint64_t offset{}, size{};
    std::uint32_t opaque_word_0c{};
};
class NativeRetailSsfArtSection;
class RetailSsfResourceView final {
public:
    const content::ArtSectionRecord& record() const noexcept;
    std::size_t index() const noexcept { return index_; }
    std::span<const std::byte> bytes() const & noexcept { return bytes_; }
    std::span<const std::byte> bytes() const && = delete;
private:
    friend class NativeRetailSsfArtSection;
    RetailSsfResourceView(std::shared_ptr<const NativeRetailSsfArtSection>,std::size_t,
        std::span<const std::byte>) noexcept;
    std::shared_ptr<const NativeRetailSsfArtSection> owner_;
    std::size_t index_{};
    std::span<const std::byte> bytes_;
};
class RetailSsfClumpView final {
public:
    const RetailSsfResourceView& resource() const noexcept { return resource_; }
    std::span<const std::byte> rw_bytes() const & noexcept { return rw_; }
    std::span<const std::byte> rw_bytes() const && = delete;
    std::int32_t first_texture() const noexcept { return first_; }
    std::int32_t last_texture() const noexcept { return last_; }
    std::span<const std::size_t> texture_records() const & noexcept { return texture_records_; }
    std::span<const std::size_t> texture_records() const && = delete;
private:
    friend class NativeRetailSsfArtSection;
    RetailSsfClumpView(RetailSsfResourceView,std::span<const std::byte>,std::int32_t,std::int32_t,
        std::vector<std::size_t>) noexcept;
    RetailSsfResourceView resource_;
    std::span<const std::byte> rw_;
    std::int32_t first_{}, last_{};
    std::vector<std::size_t> texture_records_;
};
// Existing NativeArtSectionData owns the named inner SEC representation.
// These retained views add explicit supported resource/length and CLUMP wrapper
// admission. A bounded clump view does not decode geometry, frames or plugins.
class NativeRetailSsfArtSection final : public std::enable_shared_from_this<NativeRetailSsfArtSection> {
public:
    NativeRetailSsfArtSection(const NativeRetailSsfArtSection&)=delete;
    NativeRetailSsfArtSection& operator=(const NativeRetailSsfArtSection&)=delete;
    std::span<const content::ArtSectionRecord> records() const & noexcept { return data_.records(); }
    std::span<const content::ArtSectionRecord> records() const && = delete;
    std::array<std::uint32_t,7> header_words() const noexcept { return data_.header_words(); }
    std::optional<RetailSsfResourceView> resource(std::size_t,RetailSsfBundleFailure&) const noexcept;
    std::optional<RetailSsfResourceView> resource(std::string_view,RetailSsfBundleFailure&) const noexcept;
    std::optional<RetailSsfClumpView> clump(std::size_t,RetailSsfBundleFailure&) const noexcept;
    std::optional<RetailSsfClumpView> clump(std::string_view,RetailSsfBundleFailure&) const noexcept;
private:
    friend class NativeRetailSsfBundle;
    explicit NativeRetailSsfArtSection(content::NativeArtSectionData) noexcept;
    content::NativeArtSectionData data_;
};
// Serialized outer SSF: SEC4/class0, 28-byte header and16-byte section records.
// Retail26F070/26EE40 instead use ELF-baked named descriptors for these ranges;
// outer+C and header+14 are NOT assumed to be a local string-table scheme.
class NativeRetailSsfBundle final {
public:
    static std::unique_ptr<NativeRetailSsfBundle> parse(std::vector<std::byte>,RetailSsfBundleFailure&) noexcept;
    static std::unique_ptr<NativeRetailSsfBundle> open(NativeRetailArchive&,std::size_t,RetailSsfBundleFailure&) noexcept;
    static std::unique_ptr<NativeRetailSsfBundle> open(NativeRetailArchive&,std::string_view,RetailSsfBundleFailure&) noexcept;
    std::array<std::uint32_t,7> header_words() const noexcept { return header_; }
    std::span<const RetailSsfSectionRecord> sections() const & noexcept { return sections_; }
    std::span<const RetailSsfSectionRecord> sections() const && = delete;
    std::shared_ptr<const NativeRetailSsfArtSection> open_art_section(std::size_t,RetailSsfBundleFailure&) const noexcept;
private:
    NativeRetailSsfBundle() = default;
    std::vector<std::byte> bytes_;
    std::array<std::uint32_t,7> header_{};
    std::vector<RetailSsfSectionRecord> sections_;
};
} // namespace mka::retail
