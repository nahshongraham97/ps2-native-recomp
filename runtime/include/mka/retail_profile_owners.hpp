#pragma once

#include "mka/allocation_profile.hpp"
#include "mka/retail_memory_owners.hpp"
#include <array>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailProfileOwnerError {
    none, missing_wave_owner, wrong_wave_group, inactive_wave_owner,
    invalid_profile_plan, wrong_thread, missing_class, heap, class_leaf,
    allocation, native_exception
};
struct RetailProfileOwnerFailure {
    RetailProfileOwnerError kind{RetailProfileOwnerError::none};
    memory::NativeHeapFailure heap;
    memory::AllocationClassFailure class_leaf;
    RetailMemoryOwnerFailure group;
};
struct RetailProfileClassObservation {
    NativeClassProfile profile;
    memory::NativeHeapObservation child;
    memory::AllocationClassObservation payload_leaf;
};

// The initial 310E90 class identity and direct payload leaf subset. The
// caller must pass the actual native memory group, whose private Wave owner is
// used directly. A heap name alone is never an ownership credential.
// This does not execute 310D30, the six callback routes, object constructors,
// or the remaining numeric profile/game-pool effects of 182670.
class NativeRetailProfileOwners final {
public:
    static std::unique_ptr<NativeRetailProfileOwners> create_initial_storage(
        const NativeClassProfilePlan&, NativeRetailMemoryOwners&,
        RetailProfileOwnerFailure&) noexcept;
    NativeRetailProfileOwners(const NativeRetailProfileOwners&) = delete;
    NativeRetailProfileOwners& operator=(const NativeRetailProfileOwners&) = delete;

    std::optional<RetailProfileClassObservation> observe(
        memory::AllocationClassRole, RetailProfileOwnerFailure&) const noexcept;
    // Direct native payload capability only; source callback dispatch remains
    // unbound. The returned owner can outlive this graph with live payloads.
    std::shared_ptr<memory::NativeAllocationClass> payload_leaf(
        memory::AllocationClassRole, RetailProfileOwnerFailure&) const noexcept;
    // Direct class leaf storage, not the source callback or object constructor.
    std::optional<memory::ClassAllocation> allocate_direct_payload_leaf(
        memory::AllocationClassRole, std::size_t bytes,
        RetailProfileOwnerFailure&) noexcept;
private:
    struct Entry {
        NativeClassProfile profile{};
        std::shared_ptr<memory::NativeHeapOwner> child;
        std::shared_ptr<memory::NativeAllocationClass> leaf;
    };
    NativeRetailProfileOwners() = default;
    bool access(RetailProfileOwnerFailure&) const noexcept;
    const Entry* find(memory::AllocationClassRole) const noexcept;
    std::shared_ptr<memory::NativeHeapOwner> wave_;
    std::array<Entry, 13> entries_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
