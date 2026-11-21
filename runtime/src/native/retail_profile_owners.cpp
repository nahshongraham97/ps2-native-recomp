#include "mka/retail_profile_owners.hpp"
#include <new>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::array<const char*, 13> names{
    "PFX objects", "MKOBJ fixed block heap", "MKBONE fixed block heap",
    "MKSOBJ fixed block heap", "MKPROC fixed block heap",
    "fixed block 16 heap", "fixed block 32 heap", "fixed block 64 heap",
    "fixed block 128 heap", "fixed block 512 heap", "fixed block 1024 heap",
    "TINYSTACK fixed block heap", "BIGSTACK fixed block heap"
};
bool same_source_plan(const NativeClassProfilePlan& plan) noexcept {
    const auto expected = select_native_class_profile(plan.requested_selector);
    if (plan.selected_profile != expected.selected_profile ||
        plan.object_counts != expected.object_counts) return false;
    for (std::size_t i = 0; i < plan.classes.size(); ++i) {
        const auto& a = plan.classes[i];
        const auto& b = expected.classes[i];
        if (a.role != b.role || a.configured_game_count != b.configured_game_count ||
            a.payload_bytes != b.payload_bytes || a.alignment != b.alignment ||
            a.callback != b.callback) return false;
    }
    return true;
}
} // namespace

bool NativeRetailProfileOwners::valid_source_plan(const NativeClassProfilePlan& plan) noexcept {
    return same_source_plan(plan);
}

std::unique_ptr<NativeRetailProfileOwners> NativeRetailProfileOwners::create_initial_storage(
    const NativeClassProfilePlan& plan, NativeRetailMemoryOwners& group,
    RetailProfileOwnerFailure& failure) noexcept {
    failure = {};
    if (!same_source_plan(plan)) {
        failure.kind = RetailProfileOwnerError::invalid_profile_plan; return {};
    }
    auto wave = group.owners_[4];
    if (!wave) { failure.kind = RetailProfileOwnerError::missing_wave_owner; return {}; }
    const auto observed = group.observe(RetailMemoryRole::wave, failure.group);
    if (!observed || observed->policy != RetailMemoryPolicy::wave_callback) {
        failure.kind = RetailProfileOwnerError::wrong_wave_group;
        return {};
    }
    auto admission = wave->retain_request(failure.heap);
    if (!admission) { failure.kind = RetailProfileOwnerError::inactive_wave_owner; return {}; }
    try {
        auto result = std::unique_ptr<NativeRetailProfileOwners>(new NativeRetailProfileOwners);
        result->wave_ = std::move(wave);
        for (std::size_t i = 0; i < result->entries_.size(); ++i) {
            auto& entry = result->entries_[i];
            entry.profile = plan.classes[i];
            entry.child = memory::NativeHeapOwner::create(names[i], result->wave_, failure.heap);
            if (!entry.child) { failure.kind = RetailProfileOwnerError::heap; return {}; }
            auto leaf = memory::NativeAllocationClass::create(
                {entry.child, entry.profile.role, memory::AllocationClassSchema::source_kind2_payload,
                    entry.profile.payload_bytes, entry.profile.alignment}, failure.class_leaf);
            if (!leaf) { failure.kind = RetailProfileOwnerError::class_leaf; return {}; }
            entry.leaf = std::move(leaf);
        }
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailProfileOwnerError::allocation;
    } catch (...) {
        failure.kind = RetailProfileOwnerError::native_exception;
    }
    return {};
}

std::optional<memory::ClassAllocation> NativeRetailProfilePayload::allocate(
    std::size_t bytes, memory::AllocationClassFailure& failure) noexcept {
    return leaf_->allocate(child_, bytes, alignment_, failure);
}
memory::NativeHeapIdentity NativeRetailProfilePayload::child_identity() const {
    return child_->observe().identity;
}
bool NativeRetailProfileOwners::access(RetailProfileOwnerFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_thread_) {
        failure.kind = RetailProfileOwnerError::wrong_thread;
        return false;
    }
    if (retired_ || retirement_partial_) {
        failure.kind = retirement_partial_ ? RetailProfileOwnerError::retirement_partial :
            RetailProfileOwnerError::retired;
        return false;
    }
    return true;
}
const NativeRetailProfileOwners::Entry* NativeRetailProfileOwners::find(
    memory::AllocationClassRole role) const noexcept {
    for (const auto& entry : entries_) if (entry.profile.role == role) return &entry;
    return nullptr;
}
std::optional<RetailProfileClassObservation> NativeRetailProfileOwners::observe(
    memory::AllocationClassRole role, RetailProfileOwnerFailure& failure) const noexcept {
    if (!access(failure)) return {};
    const auto* entry = find(role);
    if (!entry) { failure.kind = RetailProfileOwnerError::missing_class; return {}; }
    const auto leaf = entry->leaf->observe(failure.class_leaf);
    if (!leaf) { failure.kind = RetailProfileOwnerError::class_leaf; return {}; }
    try {
        return RetailProfileClassObservation{entry->profile, entry->child->observe(), *leaf};
    } catch (...) {
        failure.kind = RetailProfileOwnerError::native_exception;
        return {};
    }
}
std::shared_ptr<memory::NativeAllocationClass> NativeRetailProfileOwners::payload_leaf(
    memory::AllocationClassRole role, RetailProfileOwnerFailure& failure) const noexcept {
    if (!access(failure)) return {};
    const auto* entry = find(role);
    if (!entry) { failure.kind = RetailProfileOwnerError::missing_class; return {}; }
    return entry->leaf;
}
std::optional<NativeRetailProfilePayload> NativeRetailProfileOwners::retained_direct_payload(
    memory::AllocationClassRole role, RetailProfileOwnerFailure& failure) const noexcept {
    if (!access(failure)) return {};
    const auto* entry = find(role);
    if (!entry) { failure.kind = RetailProfileOwnerError::missing_class; return {}; }
    return NativeRetailProfilePayload(entry->child, entry->leaf, entry->profile.alignment);
}std::optional<memory::ClassAllocation> NativeRetailProfileOwners::allocate_direct_payload_leaf(
    memory::AllocationClassRole role, std::size_t bytes,
    RetailProfileOwnerFailure& failure) noexcept {
    if (!access(failure)) return {};
    const auto* entry = find(role);
    if (!entry) { failure.kind = RetailProfileOwnerError::missing_class; return {}; }
    auto payload = entry->leaf->allocate(entry->child, bytes, entry->profile.alignment,
        failure.class_leaf);
    if (!payload) failure.kind = RetailProfileOwnerError::class_leaf;
    return payload;
}

bool NativeRetailProfileOwners::retire_source_classes(RetailProfileOwnerFailure& failure) noexcept {
    if (!access(failure)) return false;
    // 310D30 first retires the three later overlay classes. This graph owns
    // only its following 13 Wave classes, in source retirement order.
    constexpr std::array<std::size_t, 13> order{0, 1, 2, 3, 4, 12, 11, 5, 6, 7, 8, 9, 10};
    try {
        for (const auto index : order) {
            const auto state = entries_[index].child->observe();
            if (!state.active || state.live_allocations || state.inflight_allocations ||
                state.child_owners || state.active_requests) {
                failure.kind = RetailProfileOwnerError::busy;
                failure.heap.kind = state.active ? memory::NativeHeapError::owner_busy :
                    memory::NativeHeapError::inactive_owner;
                return false;
            }
        }
        std::size_t completed = 0;
        for (const auto index : order) {
            if (!entries_[index].child->shutdown(failure.heap)) {
                // A native exception can occur after shutdown marks this child
                // inactive but before it unlinks the parent. Only owner_busy is
                // known to refuse before mutation. Treat every other failure as
                // an irreversible partial retirement, including the first slot.
                retirement_partial_ = completed != 0 ||
                    failure.heap.kind != memory::NativeHeapError::owner_busy;
                failure.kind = retirement_partial_ ? RetailProfileOwnerError::retirement_partial :
                    RetailProfileOwnerError::busy;
                return false;
            }
            ++completed;
        }
        retired_ = true;
        return true;
    } catch (...) {
        failure.kind = RetailProfileOwnerError::native_exception;
        return false;
    }
}
} // namespace mka::retail
