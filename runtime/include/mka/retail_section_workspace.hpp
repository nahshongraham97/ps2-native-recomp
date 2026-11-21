#pragma once

#include "mka/retail_memory_owners.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace mka::retail {

enum class RetailSectionWorkspaceError {
    none, wrong_thread, section_owner, allocation, host_capacity,
    native_exception, invalid_record, invalid_subrange,
    unsupported_selection, loaded_resource_retirement_unbound,
    incomplete_selection
};
struct RetailSectionWorkspaceFailure {
    RetailSectionWorkspaceError kind{RetailSectionWorkspaceError::none};
    RetailMemoryOwnerFailure memory{};
};

struct RetailSectionSubrange {
    std::uint32_t logical_id{};
    std::size_t offset{};
    std::size_t initial_bytes{};
};
struct RetailSectionRecordView {
    std::uint32_t logical_id{};
    std::size_t initial_bytes{};
    std::size_t storage_bytes{};
    std::span<const RetailSectionSubrange> subranges;
    memory::NativeHeapIdentity storage_owner{};
};
struct RetailSectionWorkspaceObservation {
    memory::NativeHeapIdentity section_owner{};
    std::size_t selected_table_index{};
    std::size_t record_count{};
    std::size_t active_transient_records{};
    std::size_t available_transient_records{};
    std::array<std::uint32_t, 2> first_auxiliary_words{};
    std::array<std::uint32_t, 2> second_auxiliary_words{};
};

// Source 19C4C0's born section buffers and descriptor records. The original
// fixed SECTION_TABLE and transient-node pools are dynamic native storage;
// source byte extents are initial allocations, never growth ceilings.
class NativeRetailSectionWorkspace final {
public:
    NativeRetailSectionWorkspace(const NativeRetailSectionWorkspace&) = delete;
    NativeRetailSectionWorkspace& operator=(const NativeRetailSectionWorkspace&) = delete;

    static std::unique_ptr<NativeRetailSectionWorkspace> initialize(
        NativeRetailMemoryOwners&, RetailSectionWorkspaceFailure&) noexcept;
    bool reset_for_reentry(NativeRetailMemoryOwners&,
        RetailSectionWorkspaceFailure&) noexcept;
    // Source 19C550 selects a serialized record table. The currently proved
    // native routes are table zero and the 18D970 table-three transition.
    // A populated retiring subrecord requires its real loader/cleanup owner.
    bool select_from_19c550(std::uint32_t table_index,
        NativeRetailMemoryOwners&, RetailSectionWorkspaceFailure&) noexcept;
    // A later real section loader must call this when it publishes live
    // subrecord children. Selection then refuses their unported retirement.
    bool mark_loaded_subrecords_unbound_from_source(std::uint32_t logical_id,
        RetailSectionWorkspaceFailure&) noexcept;
    std::optional<RetailSectionWorkspaceObservation> observe(
        RetailSectionWorkspaceFailure&) const noexcept;
    // Views borrow this owner and expire when selection changes or it retires.
    std::optional<RetailSectionRecordView> record(
        std::size_t ordinal, RetailSectionWorkspaceFailure&) const noexcept;
    std::optional<RetailSectionRecordView> find_record(
        std::uint32_t logical_id, RetailSectionWorkspaceFailure&) const noexcept;
    // Writable storage for a later source-proved loader. Bytes have no decoded
    // content meaning until written. Borrowed spans expire on growth, changed
    // selection, or owner retirement; reacquire after any such operation.
    std::optional<std::span<std::byte>> section_storage(
        std::uint32_t logical_id, RetailSectionWorkspaceFailure&) noexcept;
    bool ensure_section_storage(std::uint32_t logical_id, std::size_t required_bytes,
        NativeRetailMemoryOwners&, RetailSectionWorkspaceFailure&) noexcept;
    // Host storage preparation for the later source transient-record producer.
    // The original 39 free nodes do not cap native capacity.
    bool reserve_transient_records(std::size_t count,
        RetailSectionWorkspaceFailure&) noexcept;

private:
    struct Record {
        std::uint32_t logical_id{};
        std::size_t initial_bytes{};
        memory::NativeAllocation storage;
        std::vector<RetailSectionSubrange> subranges;
        bool loaded_subrecords_unbound{};
    };
    NativeRetailSectionWorkspace() = default;
    bool same_thread(RetailSectionWorkspaceFailure&) const noexcept;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    memory::NativeHeapIdentity section_owner_{};
    std::vector<Record> records_;
    std::uint32_t selected_table_index_{};
    bool selection_incomplete_{};
    struct TransientRecord { std::optional<std::size_t> next_free; };
    // Index links stay valid when native vector storage grows.
    std::vector<TransientRecord> transient_records_;
    std::optional<std::size_t> free_transient_head_;
    std::array<std::uint32_t, 2> first_auxiliary_words_{};
    std::array<std::uint32_t, 2> second_auxiliary_words_{};
};

} // namespace mka::retail
