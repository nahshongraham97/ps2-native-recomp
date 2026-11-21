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
constexpr std::array<SourceRecord, 7> source_records{{
    {0, 0x69000, subs_0},
    {3, 0x2cb000, subs_3_4},
    {4, 0x2cb000, subs_3_4},
    {1, 0xf7800, subs_1},
    {12, 0x11f800, subs_12},
    {2, 0x3cf000, subs_2},
    {5, 0x1af000, subs_5},
}};

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

        // 19C550(0) publishes seven records in table order. Keep the partial
        // allocations private until every real Section buffer and descriptor
        // is constructed; a failed native birth cannot expose a torn table.
        for (std::size_t i = 0; i < source_records.size(); ++i) {
            const auto& source = source_records[i];
            auto& destination = result->records_[i];
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

bool NativeRetailSectionWorkspace::reset_for_reentry(
    NativeRetailMemoryOwners& memory,
    RetailSectionWorkspaceFailure& failure) noexcept {
    failure = {};
    if (!same_thread(failure)) return false;
    try {
        const auto owner = memory.observe(RetailMemoryRole::section, failure.memory);
        if (!owner || !owner->heap.active || owner->heap.identity != section_owner_) {
            failure.kind = RetailSectionWorkspaceError::section_owner;
            return false;
        }
        // 19C550(0) sees the already selected table and keeps its seven
        // buffers. 26F1A0 clears both pairs, then 3DE760 rebuilds the free
        // list. No native transient producer is active in this birth scope.
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
    return RetailSectionWorkspaceObservation{section_owner_, 0, records_.size(), 0,
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
