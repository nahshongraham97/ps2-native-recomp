#pragma once

#include "mka/allocation_profile.hpp"
#include "mka/retail_memory_owners.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace mka::retail {

enum class RetailProfileOwnerError {
    none, missing_wave_owner, wrong_wave_group, inactive_wave_owner,
    invalid_profile_plan, wrong_thread, missing_class, heap, class_leaf,
    allocation, native_exception, busy, retired, retirement_partial
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
// Its retirement method covers only the 13 Wave slots of 310D30. The three
// earlier overlay slots, callback routes, object constructors and game-pool
// effects remain unbound.
// Retained direct-payload route. This is not a retail callback/object initializer.
// It keeps the exact requested child alive so an old handle sees retirement.
class NativeRetailProfilePayload final {
public:
    NativeRetailProfilePayload(const NativeRetailProfilePayload&) = default;
    NativeRetailProfilePayload(NativeRetailProfilePayload&&) noexcept = default;
    std::optional<memory::ClassAllocation> allocate(
        std::size_t bytes, memory::AllocationClassFailure&) noexcept;
    memory::NativeHeapIdentity child_identity() const;
private:
    friend class NativeRetailProfileOwners;
    NativeRetailProfilePayload(std::shared_ptr<memory::NativeHeapOwner> child,
        std::shared_ptr<memory::NativeAllocationClass> leaf,
        std::size_t alignment) noexcept
        : child_(std::move(child)), leaf_(std::move(leaf)), alignment_(alignment) {}
    std::shared_ptr<memory::NativeHeapOwner> child_;
    std::shared_ptr<memory::NativeAllocationClass> leaf_;
    std::size_t alignment_;
};
class NativeRetailProfileOwners final {
public:
    static std::unique_ptr<NativeRetailProfileOwners> create_initial_storage(
        const NativeClassProfilePlan&, NativeRetailMemoryOwners&,
        RetailProfileOwnerFailure&) noexcept;
    static bool valid_source_plan(const NativeClassProfilePlan&) noexcept;
    NativeRetailProfileOwners(const NativeRetailProfileOwners&) = delete;
    NativeRetailProfileOwners& operator=(const NativeRetailProfileOwners&) = delete;

    std::optional<RetailProfileClassObservation> observe(
        memory::AllocationClassRole, RetailProfileOwnerFailure&) const noexcept;
    // Direct native payload capability only; source callback dispatch remains
    // unbound. The returned owner can outlive this graph with live payloads.
    std::shared_ptr<memory::NativeAllocationClass> payload_leaf(
        memory::AllocationClassRole, RetailProfileOwnerFailure&) const noexcept;
    std::optional<NativeRetailProfilePayload> retained_direct_payload(
        memory::AllocationClassRole, RetailProfileOwnerFailure&) const noexcept;
    // Direct class leaf storage, not the source callback or object constructor.
    std::optional<memory::ClassAllocation> allocate_direct_payload_leaf(
        memory::AllocationClassRole, std::size_t bytes,
        RetailProfileOwnerFailure&) noexcept;
    // 310D30's 13 Wave-class portion. A preflight refuses known live payloads,
    // requests and children. A concurrent external leaf can still race that
    // preflight; any later failure is reported as irreversible partial retire.
    bool retire_source_classes(RetailProfileOwnerFailure&) noexcept;
    bool retired() const noexcept { return retired_; }
    bool retirement_partial() const noexcept { return retirement_partial_; }
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
    bool retired_{}, retirement_partial_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
