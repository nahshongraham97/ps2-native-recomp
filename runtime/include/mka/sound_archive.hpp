#pragma once

#include "mka/sample_payload.hpp"
#include "mka/audio_resource_domain.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mka::sound {

enum class ArchiveError {
    none, open_failed, io_error, cursor_restore_failed, short_read,
    truncated_header, unsupported_type, invalid_count, directory_out_of_bounds,
    entry_out_of_bounds, unterminated_name, duplicate_key, ambiguous_name,
    host_capacity, allocation_failed, absent_bank, closed, resource_access
};
struct ArchiveFailure {
    ArchiveError kind = ArchiveError::none;
    std::error_code host_error;
    AudioResourceFailure resource;
};
struct ArchiveRecord {
    std::uint32_t key = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::string name;
};

// Owns the actual opened host file, not its path alone. Banks retain this
// object, so opening another archive cannot redirect an existing bank's reads.
// NativeFile cursor operations are serialized across all sharing banks.
class SoundArchiveBacking {
public:
    static std::shared_ptr<SoundArchiveBacking> open(const std::filesystem::path&,
                                                      ArchiveFailure&);
    static std::shared_ptr<SoundArchiveBacking> open(const std::filesystem::path&,
        std::shared_ptr<AudioResourceDomain>, ArchiveFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept;
    ~SoundArchiveBacking();
    SoundArchiveBacking(const SoundArchiveBacking&) = delete;
    SoundArchiveBacking& operator=(const SoundArchiveBacking&) = delete;
    std::uint64_t size() const noexcept;
    std::optional<std::vector<std::uint8_t>> read_bytes(
        std::uint64_t offset, std::uint64_t count, ArchiveFailure&) const;
    std::optional<SamplePayload> read_payload(const ResourceLookup&, PayloadFailure&) const;
private:
    struct State;
    explicit SoundArchiveBacking(std::unique_ptr<State>);
    std::unique_ptr<State> state_;
};

struct BankAssetSource {
    ArchiveRecord origin;
    std::vector<std::uint8_t> bytes;
    std::shared_ptr<SoundArchiveBacking> backing;
};

// Serialized MSL type1/type2 directory data. This is not a disc, guest module
// or fixed-size bank table. Count and name-tail boundaries come from the file.
// Duplicate keys/names are explicitly outside the supported host contract;
// the retail asset has neither, so no cached-search-order guess is required.
class SoundArchive {
public:
    SoundArchive(const SoundArchive&) = default;
    SoundArchive(SoundArchive&& other) noexcept { swap(other); }
    // Copying the parameter completes before mutation. Allocation failure
    // cannot publish a new backing paired with the old directory.
    SoundArchive& operator=(SoundArchive other) noexcept { swap(other); return *this; }
    static std::optional<SoundArchive> open(const std::filesystem::path&, ArchiveFailure&);
    static std::optional<SoundArchive> open(const std::filesystem::path&,
        std::shared_ptr<AudioResourceDomain>, ArchiveFailure&);
    std::shared_ptr<AudioResourceDomain> resource_domain() const noexcept {
        return backing_ ? backing_->resource_domain() : nullptr;
    }
    std::optional<ArchiveRecord> find_bank(std::string_view request, ArchiveFailure&) const;
    std::optional<BankAssetSource> read_bank(std::string_view request, ArchiveFailure&) const;
    std::size_t directory_entry_count() const noexcept { return backing_ ? records_.size() : 0; }
    std::size_t named_entry_count() const noexcept;
    // Inspection data includes sentinel0 and numeric entries; find_bank is the
    // normal name route and never returns sentinel0.
    const std::vector<ArchiveRecord>& directory_records() const noexcept { return records_; }
private:
    SoundArchive() = default;
    void swap(SoundArchive&) noexcept;
    std::shared_ptr<SoundArchiveBacking> backing_;
    std::vector<ArchiveRecord> records_;
    // Includes sentinel0 for source indices, but normal lookup starts at1.
    // Type1 can contain numeric slots between names; those consume no string.
    std::vector<std::optional<std::string>> names_;
};

} // namespace mka::sound
