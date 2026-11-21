#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace mka::retail {
class NativeRetailGameInfoResource;
class NativeRetailHumanBoneTable;
class NativeRetailFlippedHumanBoneTable;
class NativeRetailNpcGroundCollisions;
class NativeRetailLoadedResource;

// A source global name resolves to one of these real native owners. Their
// producers belong to their own source callpoints; no guest address or void
// pointer can be registered here.
using NativeRetailGameResourceValue = std::variant<
    std::shared_ptr<NativeRetailGameInfoResource>,
    std::shared_ptr<NativeRetailHumanBoneTable>,
    std::shared_ptr<NativeRetailFlippedHumanBoneTable>,
    std::shared_ptr<NativeRetailNpcGroundCollisions>>;

enum class RetailGameResourceRegistryError {
    none, wrong_thread, missing_value, allocation, native_exception,
    foreign_record, stale_record, identity_exhausted
};
struct RetailGameResourceRegistryFailure {
    RetailGameResourceRegistryError kind{RetailGameResourceRegistryError::none};
    const char* operation{""};
};
struct RetailGameResourceGlobalObservation {
    std::string original_name;
    std::uint32_t source_hash{};
    std::uint32_t source_bucket{};
    NativeRetailGameResourceValue value;
};
struct RetailGameResourceRecordHandle {
    std::uint64_t registry_identity{}, generation{};
    std::size_t slot{};
};
struct RetailGameResourceRegistryObservation {
    std::size_t global_names{}, record_slots{};
    std::vector<std::size_t> free_order, active_order;
};

// Source 2173D0's global name index and separate resource-record free list.
// The source's 23 buckets remain the hash modulus. Its 20 initial record
// slots are a birth configuration, not a host allocation ceiling.
class NativeRetailGameResourceRegistry final {
public:
    static std::unique_ptr<NativeRetailGameResourceRegistry> create(
        RetailGameResourceRegistryFailure&) noexcept;
    NativeRetailGameResourceRegistry(const NativeRetailGameResourceRegistry&) = delete;
    NativeRetailGameResourceRegistry& operator=(const NativeRetailGameResourceRegistry&) = delete;

    static std::uint32_t source_hash(std::string_view) noexcept;
    static std::uint32_t source_bucket(std::string_view name) noexcept {
        return source_hash(name) % 23u;
    }
    // Null name is the original 2404A0 no-effect case. A source C string
    // ends at its first NUL; the original spelling is copied on insertion.
    bool insert_global(std::optional<std::string_view> name,
        NativeRetailGameResourceValue, RetailGameResourceRegistryFailure&) noexcept;
    std::optional<RetailGameResourceGlobalObservation> lookup_global(
        std::optional<std::string_view> name,
        RetailGameResourceRegistryFailure&) const noexcept;

    // Actual loaded-resource admission is separate from global names. A
    // parsed native resource must exist before taking a record from the free
    // list. These methods do not implement 214120 parsing or 215ED0 lookup.
    std::optional<RetailGameResourceRecordHandle> activate_loaded_record(
        std::shared_ptr<const NativeRetailLoadedResource>,
        RetailGameResourceRegistryFailure&) noexcept;
    bool retire_loaded_record(RetailGameResourceRecordHandle,
        RetailGameResourceRegistryFailure&) noexcept;
    std::shared_ptr<const NativeRetailLoadedResource> retained_loaded_record(
        RetailGameResourceRecordHandle,
        RetailGameResourceRegistryFailure&) const noexcept;
    std::optional<RetailGameResourceRegistryObservation> observe(
        RetailGameResourceRegistryFailure&) const noexcept;

private:
    struct GlobalEntry {
        std::string original_name;
        NativeRetailGameResourceValue value;
        std::uint32_t hash{};
    };
    struct RecordSlot {
        std::shared_ptr<const NativeRetailLoadedResource> resource;
        std::size_t next{};
        std::uint64_t generation{};
        bool active{};
    };
    static constexpr std::size_t no_slot=static_cast<std::size_t>(-1);
    explicit NativeRetailGameResourceRegistry(std::uint64_t identity) noexcept
        : identity_(identity) {}
    bool access(RetailGameResourceRegistryFailure&) const noexcept;
    bool valid_record(RetailGameResourceRecordHandle,
        RetailGameResourceRegistryFailure&) const noexcept;
    static std::string_view c_prefix(std::string_view) noexcept;

    std::array<std::list<GlobalEntry>,23> buckets_{};
    std::vector<RecordSlot> records_;
    std::size_t free_head_{no_slot}, active_head_{no_slot};
    std::size_t global_count_{};
    std::uint64_t identity_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
