#pragma once

#include "mka/content_volume.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mka::retail {

enum class RetailArchiveError {
    none, invalid_source, source_length, source_seek, source_read,
    unsupported_format, invalid_header, invalid_table, invalid_name,
    duplicate_name, invalid_member_range, overlapping_ranges,
    missing_member, invalid_member_index, invalid_read_range, wrong_thread,
    allocation, host_capacity, native_exception
};
struct RetailArchiveFailure {
    RetailArchiveError kind{RetailArchiveError::none};
    const char* operation{""};
    content::ContentVolumeFailure content{};
};
struct RetailArchiveMember {
    std::size_t index{};
    std::string name;
    std::uint64_t offset{};
    std::uint64_t size{};
};

// The observed retail PAK serialization: 20-byte LE header, 12-byte records,
// then NUL-terminated names. Original2193E0/219160 prove table/read semantics;
// malformed-input checks below are explicit native admission rules. This
// does not interpret SEC entries, script payloads, or RenderWare chunks.
// Owns the supplied already-open file and its volume identity; never reopens
// a member pathname. Construction publishes nothing before full validation.
class NativeRetailArchive final {
public:
    static std::unique_ptr<NativeRetailArchive> open(
        content::NativeContentFile&&, RetailArchiveFailure&) noexcept;
    NativeRetailArchive(const NativeRetailArchive&) = delete;
    NativeRetailArchive& operator=(const NativeRetailArchive&) = delete;
    NativeRetailArchive(NativeRetailArchive&&) = delete;
    NativeRetailArchive& operator=(NativeRetailArchive&&) = delete;

    std::size_t member_count() const noexcept { return members_.size(); }
    std::uint64_t source_size() const noexcept { return source_size_; }
    std::string_view source_key() const noexcept { return source_.logical_key(); }
    bool same_volume(const content::NativeContentFile& other) const noexcept {
        return source_.same_volume(other);
    }
    // Full logical names only: ASCII case folding and slash comparison.
    // No basename search or implicit retail prefix insertion. Colliding
    // serialized names are rejected, rather than silently made aliases.
    std::optional<RetailArchiveMember> find(std::string_view, RetailArchiveFailure&) const noexcept;
    std::optional<RetailArchiveMember> at(std::size_t, RetailArchiveFailure&) const noexcept;
    // Member-relative reads clip at EOF; short underlying reads fail by name.
    std::optional<std::size_t> read(std::size_t, std::uint64_t,
        std::span<std::uint8_t>, RetailArchiveFailure&) noexcept;
    std::optional<std::size_t> read(std::string_view, std::uint64_t,
        std::span<std::uint8_t>, RetailArchiveFailure&) noexcept;
    std::optional<std::vector<std::uint8_t>> extract(std::size_t, RetailArchiveFailure&) noexcept;
    std::optional<std::vector<std::uint8_t>> extract(std::string_view, RetailArchiveFailure&) noexcept;

private:
    explicit NativeRetailArchive(content::NativeContentFile&&);
    bool validate(RetailArchiveFailure&);
    bool owner_thread(RetailArchiveFailure&) const noexcept;
    bool exact_read(std::uint64_t, std::span<std::uint8_t>, RetailArchiveFailure&);
    static std::optional<std::string> key(std::string_view, RetailArchiveFailure&);
    content::NativeContentFile source_;
    const std::thread::id thread_{std::this_thread::get_id()};
    std::uint64_t source_size_{};
    std::vector<RetailArchiveMember> members_;
    std::unordered_map<std::string, std::size_t> by_name_;
};
} // namespace mka::retail
