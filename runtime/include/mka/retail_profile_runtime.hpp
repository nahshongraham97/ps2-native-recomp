#pragma once

#include "mka/retail_profile_owners.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailProfileRuntimeError {
    none, allocation, native_exception, wrong_thread, foreign_group,
    invalid_profile_plan, missing_wave_owner,
    busy, profile_owner, generation_exhausted, partial_irreversible
};
struct RetailProfileRuntimeFailure {
    RetailProfileRuntimeError kind{RetailProfileRuntimeError::none};
    RetailMemoryOwnerFailure group;
    RetailProfileOwnerFailure profile;
};
enum class RetailProfileGenerationState { empty, ready, partial_irreversible };
struct RetailProfileRuntimeObservation {
    RetailProfileGenerationState state;
    std::uint64_t generation;
    std::optional<NativeClassProfilePlan> numeric;
    // Unknown after an irreversible mid-retirement failure.
    std::optional<std::size_t> wave_classes;
    memory::NativeHeapIdentity wave_identity;
};

// Program-lifetime numeric profile and native Wave-class generations. This is
// the storage/reset prerequisite, not the full 182670 or 130350 main binding.
// The later 311260 overlay classes and full 16-slot 310D30 retirement are
// outside this owner. Their source effects remain unbound.
// The source memory group must outlive this nonowning reference.
class NativeRetailProfileRuntime final {
public:
    static std::unique_ptr<NativeRetailProfileRuntime> create(
        NativeRetailMemoryOwners&, RetailProfileRuntimeFailure&) noexcept;
    NativeRetailProfileRuntime(const NativeRetailProfileRuntime&) = delete;
    NativeRetailProfileRuntime& operator=(const NativeRetailProfileRuntime&) = delete;

    // Models numeric publication followed by retirement of old Wave classes
    // and 13 new Wave-child constructions. The preceding three overlay slots,
    // parent's 42A0B0 reset, callbacks and pool/object effects remain unbound.
    bool apply_numeric_and_wave_classes(const NativeClassProfilePlan&,
        NativeRetailMemoryOwners&, RetailProfileRuntimeFailure&) noexcept;
    std::optional<RetailProfileRuntimeObservation> observe(
        RetailProfileRuntimeFailure&) const noexcept;
    std::optional<NativeRetailProfilePayload> direct_payload(
        memory::AllocationClassRole, RetailProfileRuntimeFailure&) const noexcept;

private:
    NativeRetailProfileRuntime(NativeRetailMemoryOwners&,
        memory::NativeHeapIdentity) noexcept;
    bool access(NativeRetailMemoryOwners&, RetailProfileRuntimeFailure&) const noexcept;
    bool same_thread(RetailProfileRuntimeFailure&) const noexcept;

    NativeRetailMemoryOwners* group_;
    memory::NativeHeapIdentity wave_identity_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::optional<NativeClassProfilePlan> numeric_;
    std::unique_ptr<NativeRetailProfileOwners> wave_classes_;
    std::uint64_t generation_{};
    RetailProfileGenerationState state_{RetailProfileGenerationState::empty};
};
} // namespace mka::retail
