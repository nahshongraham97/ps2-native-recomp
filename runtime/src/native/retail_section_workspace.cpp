#include "mka/retail_section_workspace.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {

struct SourceSubrange { std::uint32_t id; std::size_t bytes; };
struct SourceRecord {
    std::uint32_t id;
    std::size_t initial_bytes;
    std::span<const SourceSubrange> subranges;
};

constexpr std::array<SourceSubrange, 2> subs_0{{{0x0a, 0x64000}, {1, 0x5000}}};
constexpr std::array<SourceSubrange, 3> subs_3_4{{{0x0d, 0x190000},
    {0x0f, 0x2a800}, {0x10, 0x110800}}};
constexpr std::array<SourceSubrange, 2> subs_1{{{0x0b, 0xdc000}, {0x77, 0x1b800}}};
constexpr std::array<SourceSubrange, 1> subs_12{{{0x0c, 0x11f800}}};
constexpr std::array<SourceSubrange, 2> subs_2{{{0x28, 0x39d000}, {0x29, 0x32000}}};
constexpr std::array<SourceSubrange, 3> subs_5{{{0x11, 0x118000},
    {0x12, 0x92000}, {0x13, 0x5000}}};
// Original 5EC86C -> 5EC460, terminated after three 12-byte records.
// The first triple shares the exact 5EBF10 descriptor with table zero.
constexpr std::array<SourceSubrange, 2> subs_8{{{0x2a, 0x795000},
    {3, 0x4000}}};
constexpr std::array<SourceSubrange, 1> subs_23{{{2, 0x19000}}};
constexpr std::array<SourceRecord, 7> source_records{{
    {0, 0x69000, subs_0},
    {3, 0x2cb000, subs_3_4},
    {4, 0x2cb000, subs_3_4},
    {1, 0xf7800, subs_1},
    {12, 0x11f800, subs_12},
    {2, 0x3cf000, subs_2},
    {5, 0x1af000, subs_5},
}};
constexpr std::array<SourceRecord, 3> source_records_3{{
    {0, 0x69000, subs_0},
    {8, 0x799000, subs_8},
    {23, 0x19000, subs_23},
}};

std::span<const SourceRecord> source_table(std::uint32_t index) noexcept {
    if (index == 0) return source_records;
    if (index == 3) return source_records_3;
    return {};
}

bool identical_source_triple(const SourceRecord& a,
                             const SourceRecord& b) noexcept {
    // The original comparison is ID, serialized subtable pointer, size.
    // Shared native static subtable identity represents that pointer alias.
    return a.id == b.id && a.initial_bytes == b.initial_bytes &&
           a.subranges.data() == b.subranges.data();
}

} // namespace

bool NativeRetailSectionWorkspace::same_thread(
    RetailSectionWorkspaceFailure& failure) const noexcept {
    if (std::this_thread::get_id() == owner_thread_) return true;
    failure.kind = RetailSectionWorkspaceError::wrong_thread;
    return false;
}

std::unique_ptr<NativeRetailSectionWorkspace>
NativeRetailSectionWorkspace::initialize(
    NativeRetailMemoryOwners& memory,
    RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    try {
        const auto owner = memory.observe(RetailMemoryRole::section, failure.memory);
        if (!owner || !owner->heap.active) {
            failure.kind = RetailSectionWorkspaceError::section_owner;
            return {};
        }
        // 19C4C0 first clears 69F100..10, then publishes the Section heap's
        // free-byte report at 69F100. Its only direct scalar reader is the
        // 19C550 fixed-arena admission comparison. Dynamic native allocation
        // replaces that backend budget; actual allocation failure is named.
        // The source-visible selected table/count/head are represented below.
        auto result = std::unique_ptr<NativeRetailSectionWorkspace>(
            new NativeRetailSectionWorkspace);
        result->section_owner_ = owner->heap.identity;
        result->records_.reserve(source_records.size());

        // 19C550(0) publishes seven records in table order. Keep the partial
        // allocations private until every real Section buffer and descriptor
        // is constructed; a failed native birth cannot expose a torn table.
        for (std::size_t i = 0; i < source_records.size(); ++i) {
            const auto& source = source_records[i];
            result->records_.emplace_back();
            auto& destination = result->records_.back();
            destination.logical_id = source.id;
            destination.initial_bytes = source.initial_bytes;
            auto allocation = memory.allocate_ordinary(
                RetailMemoryRole::section, source.initial_bytes, failure.memory);
            if (!allocation ||
                allocation->requested_owner() != result->section_owner_) {
                failure.kind = RetailSectionWorkspaceError::section_owner;
                return {};
            }
            destination.storage = std::move(*allocation);
            destination.subranges.reserve(source.subranges.size());
            std::size_t offset = 0;
            for (const auto& subrange : source.subranges) {
                destination.subranges.push_back(
                    {subrange.id, offset, subrange.bytes});
                offset += subrange.bytes;
            }
            if (offset != source.initial_bytes) {
                failure.kind = RetailSectionWorkspaceError::invalid_subrange;
                return {};
            }
        }
        // 26F1A0's two pairs are zero-born. 3DE760 has zero active records;
        // its fixed free-node backing has no native host capacity analogue.
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailSectionWorkspaceError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailSectionWorkspaceError::host_capacity;
    } catch (...) {
        failure.kind = RetailSectionWorkspaceError::native_exception;
    }
    return {};
}

bool NativeRetailSectionWorkspace::select_from_19c550(
    std::uint32_t table_index, NativeRetailMemoryOwners& memory,
    RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return false;
    // Failure after table publication can leave a partial descriptor chain.
    // There is no proved source retry route from that state.
    if (selection_incomplete_) {
        failure.kind = RetailSectionWorkspaceError::incomplete_selection;
        return false;
    }
    const auto desired = source_table(table_index);
    if (desired.empty()) {
        failure.kind = RetailSectionWorkspaceError::unsupported_selection;
        return false;
    }
    try {
        const auto owner = memory.observe(RetailMemoryRole::section, failure.memory);
        if (!owner || !owner->heap.active || owner->heap.identity != section_owner_) {
            failure.kind = RetailSectionWorkspaceError::section_owner;
            return false;
        }
        // 19C580 is BEQL: an identical table pointer returns before any
        // record comparison, retirement, count store or allocation.
        if (selected_table_index_ == table_index) return true;

        const auto prior = source_table(selected_table_index_);
        std::size_t shared = 0;
        while (shared < prior.size() && shared < desired.size() &&
               identical_source_triple(prior[shared], desired[shared]))
            ++shared;

        // The original 19C8B0 -> 19C940 invokes resource/list cleanup on
        // populated retiring subrecords. Their native producer is not yet
        // attached; refuse before discarding even one real child owner.
        for (std::size_t i = shared; i < records_.size(); ++i) {
            if (records_[i].loaded_subrecords_unbound) {
                failure.kind =
                    RetailSectionWorkspaceError::loaded_resource_retirement_unbound;
                return false;
            }
        }

        // Host vector capacity is private allocation bookkeeping, not the
        // original SECTION_TABLE pool. Reserve before the source-visible
        // selected-table publication so metadata allocation cannot strand
        // a partially selected table.
        records_.reserve(desired.size());
        // 19C620 retires the divergent suffix in linked-head order (last
        // ordinal first); 19C630 then publishes the selected table pointer.
        // This native empty-child branch preserves the same surviving record
        // and actual heap allocation identity. Pop destroys each native
        // buffer and descriptor only after the source prefix comparison.
        while (records_.size() > shared) {
            auto& retiring = records_.back();
            retiring.storage = memory::NativeAllocation{}; // 19C8F4 buffer free
            retiring.subranges.clear();                  // 19C904 descriptor free
            records_.pop_back();
        }
        selected_table_index_ = table_index;
        selection_incomplete_ = true;

        // Source 19C684 publishes count, then 19C6A0..19C740 allocates
        // records in descriptor order. Dynamic native allocations replace
        // the PS2 free-byte gate and fixed descriptor arena. A failure after
        // this point leaves the already retired/published partial transition.
        for (std::size_t i = shared; i < desired.size(); ++i) {
            const auto& source = desired[i];
            Record next;
            next.logical_id = source.id;
            next.initial_bytes = source.initial_bytes;
            // The source allocates its descriptor first, then the Section
            // buffer, then builds subrecords. Native metadata reservation is
            // private but keeps the same failure-before-buffer ordering.
            next.subranges.reserve(source.subranges.size());
            auto allocation = memory.allocate_ordinary(
                RetailMemoryRole::section, source.initial_bytes, failure.memory);
            if (!allocation || allocation->requested_owner() != section_owner_) {
                failure.kind = RetailSectionWorkspaceError::section_owner;
                return false;
            }
            next.storage = std::move(*allocation);
            std::size_t offset = 0;
            for (const auto& subrange : source.subranges) {
                next.subranges.push_back({subrange.id, offset, subrange.bytes});
                offset += subrange.bytes;
            }
            if (offset != source.initial_bytes) {
                failure.kind = RetailSectionWorkspaceError::invalid_subrange;
                return false;
            }
            records_.push_back(std::move(next));
        }
        selection_incomplete_ = false;
        return true;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailSectionWorkspaceError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailSectionWorkspaceError::host_capacity;
    } catch (...) {
        failure.kind = RetailSectionWorkspaceError::native_exception;
    }
    return false;
}

bool NativeRetailSectionWorkspace::mark_loaded_subrecords_unbound_from_source(
    std::uint32_t logical_id, RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return false;
    if (selection_incomplete_) {
        failure.kind = RetailSectionWorkspaceError::incomplete_selection;
        return false;
    }
    for (auto& record : records_) {
        if (record.logical_id != logical_id) continue;
        record.loaded_subrecords_unbound = true;
        return true;
    }
    failure.kind = RetailSectionWorkspaceError::invalid_record;
    return false;
}

bool NativeRetailSectionWorkspace::reset_for_reentry(
    NativeRetailMemoryOwners& memory,
    RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return false;
    if (!select_from_19c550(0, memory, failure)) return false;
    try {
        const auto owner = memory.observe(RetailMemoryRole::section, failure.memory);
        if (!owner || !owner->heap.active || owner->heap.identity != section_owner_) {
            failure.kind = RetailSectionWorkspaceError::section_owner;
            return false;
        }
        // 19C550(0) either returns for unchanged table zero or first retires
        // the divergent table-three suffix. 26F1A0 then clears both pairs,
        // and 3DE760 rebuilds the free list.
        first_auxiliary_words_ = {};
        second_auxiliary_words_ = {};
        free_transient_head_.reset();
        for (std::size_t i = transient_records_.size(); i-- > 0;) {
            transient_records_[i].next_free = free_transient_head_;
            free_transient_head_ = i;
        }
        return true;
    } catch (...) {
        failure.kind = RetailSectionWorkspaceError::native_exception;
        return false;
    }
}

std::optional<RetailSectionWorkspaceObservation>
NativeRetailSectionWorkspace::observe(
    RetailSectionWorkspaceFailure& failure) const noexcept {
    failure = {};
    if (!same_thread(failure)) return {};
    return RetailSectionWorkspaceObservation{section_owner_, selected_table_index_, records_.size(), 0,
        transient_records_.size(), first_auxiliary_words_, second_auxiliary_words_};
}

std::optional<RetailSectionRecordView>
NativeRetailSectionWorkspace::record(
    std::size_t ordinal, RetailSectionWorkspaceFailure& failure) const noexcept {
    failure = {};
    if (!same_thread(failure)) return {};
    if (ordinal >= records_.size()) {
        failure.kind = RetailSectionWorkspaceError::invalid_record;
        return {};
    }
    const auto& value = records_[ordinal];
    return RetailSectionRecordView{value.logical_id, value.initial_bytes,
        value.storage.requested_size(), value.subranges, section_owner_};
}

std::optional<RetailSectionRecordView>
NativeRetailSectionWorkspace::find_record(
    std::uint32_t logical_id, RetailSectionWorkspaceFailure& failure) const noexcept {
    failure = {};
    if (!same_thread(failure)) return {};
    for (std::size_t i = 0; i < records_.size(); ++i)
        if (records_[i].logical_id == logical_id) return record(i, failure);
    failure.kind = RetailSectionWorkspaceError::invalid_record;
    return {};
}

std::optional<std::span<std::byte>>
NativeRetailSectionWorkspace::section_storage(
    std::uint32_t logical_id, RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return {};
    if (selection_incomplete_) {
        failure.kind = RetailSectionWorkspaceError::incomplete_selection;
        return {};
    }
    for (auto& value : records_)
        if (value.logical_id == logical_id)
            return std::span(static_cast<std::byte*>(value.storage.data()),
                             value.storage.requested_size());
    failure.kind = RetailSectionWorkspaceError::invalid_record;
    return {};
}

bool NativeRetailSectionWorkspace::ensure_section_storage(
    std::uint32_t logical_id, std::size_t required_bytes,
    NativeRetailMemoryOwners& memory,
    RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return false;
    if (selection_incomplete_) {
        failure.kind = RetailSectionWorkspaceError::incomplete_selection;
        return false;
    }
    try {
        const auto owner = memory.observe(RetailMemoryRole::section, failure.memory);
        if (!owner || !owner->heap.active || owner->heap.identity != section_owner_) {
            failure.kind = RetailSectionWorkspaceError::section_owner;
            return false;
        }
        for (auto& value : records_) {
            if (value.logical_id != logical_id) continue;
            if (required_bytes <= value.storage.requested_size()) return true;
            // Allocate before retiring the old buffer. Existing written bytes
            // survive; earlier spans must be reacquired after this call.
            auto replacement = memory.allocate_ordinary(
                RetailMemoryRole::section, required_bytes, failure.memory);
            if (!replacement || replacement->requested_owner() != section_owner_) {
                failure.kind = RetailSectionWorkspaceError::section_owner;
                return false;
            }
            std::memcpy(replacement->data(), value.storage.data(),
                        value.storage.requested_size());
            value.storage = std::move(*replacement);
            return true;
        }
        failure.kind = RetailSectionWorkspaceError::invalid_record;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailSectionWorkspaceError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailSectionWorkspaceError::host_capacity;
    } catch (...) {
        failure.kind = RetailSectionWorkspaceError::native_exception;
    }
    return false;
}

bool NativeRetailSectionWorkspace::reserve_transient_records(
    std::size_t count, RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return false;
    if (selection_incomplete_) {
        failure.kind = RetailSectionWorkspaceError::incomplete_selection;
        return false;
    }
    try {
        if (count <= transient_records_.size()) return true;
        const auto prior = transient_records_.size();
        transient_records_.resize(count);
        // Every new node is free. Index links, unlike pointers into vector
        // storage, remain valid through future native growth.
        for (std::size_t i = count; i-- > prior;) {
            transient_records_[i].next_free = free_transient_head_;
            free_transient_head_ = i;
        }
        return true;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailSectionWorkspaceError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailSectionWorkspaceError::host_capacity;
    } catch (...) {
        failure.kind = RetailSectionWorkspaceError::native_exception;
    }
    return false;
}

} // namespace mka::retail
