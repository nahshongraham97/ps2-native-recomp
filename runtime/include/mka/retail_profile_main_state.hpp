#pragma once

#include "mka/retail_active_overlay.hpp"
#include "mka/retail_profile_runtime.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailProfileMainError {
    none, wrong_thread, foreign_group, invalid_plan, missing_wave,
    allocation, native_exception, wave_runtime, companion_busy,
    companion_partial, partial_irreversible, missing_overlay,
    foreign_overlay, companions_already_present, callback_route_unbound,
    generation_exhausted
};
struct RetailProfileMainFailure {
    RetailProfileMainError kind{RetailProfileMainError::none};
    const char* operation{""};
    RetailProfileRuntimeFailure wave;
    RetailProfileOwnerFailure profile;
    RetailMemoryOwnerFailure group;
    memory::NativeHeapFailure heap;
    memory::AllocationClassFailure class_leaf;
    RetailActiveOverlayFailure overlay;
};
enum class RetailProfileMainPhase {
    empty, ready, numeric_published, partial_irreversible
};
struct RetailProfileMainObservation {
    RetailProfileMainPhase phase;
    std::optional<NativeClassProfilePlan> numeric;
    std::uint64_t generation{};
    // The three source 311260 overlay slots, in creation order AB90/AB8C/AB94.
    std::array<std::optional<bool>,3> companion_present{};
    std::array<std::optional<std::uint64_t>,3> companion_configured_counts{};
    std::optional<std::size_t> active_wave_classes;
};

// Program-lifetime source 182670 state. The actual memory group outlives it.
// An overlay capability can be supplied only by the later real 3DDA70 owner;
// the three companion slots start absent as original BSS values.
class NativeRetailProfileMainState final {
public:
    static std::unique_ptr<NativeRetailProfileMainState> create(
        NativeRetailMemoryOwners&,RetailProfileMainFailure&) noexcept;
    NativeRetailProfileMainState(const NativeRetailProfileMainState&)=delete;
    NativeRetailProfileMainState& operator=(const NativeRetailProfileMainState&)=delete;

    // Numeric publication, conditional three-companion retirement in source
    // order, then the existing 13-Wave retirement and recreation.
    bool apply_182670(const NativeClassProfilePlan&,NativeRetailMemoryOwners&,
        RetailProfileMainFailure&) noexcept;
    // Separate later source 311260 producer. Requires the exact 3DDA70
    // overlay capability and never manufactures companions at first profile.
    bool install_companions_311260(const NativeRetailActiveOverlay&,
        NativeRetailMemoryOwners&,RetailProfileMainFailure&) noexcept;
    std::optional<RetailProfileMainObservation> observe(
        RetailProfileMainFailure&) const noexcept;

private:
    struct Companion {
        std::shared_ptr<memory::NativeHeapOwner> child;
        std::shared_ptr<memory::NativeAllocationClass> leaf;
        std::uint64_t configured_game_count{};
    };
    NativeRetailProfileMainState(NativeRetailMemoryOwners&,
        std::unique_ptr<NativeRetailProfileRuntime>) noexcept;
    bool access(NativeRetailMemoryOwners&,RetailProfileMainFailure&) const noexcept;
    bool retire_companions(RetailProfileMainFailure&) noexcept;

    NativeRetailMemoryOwners* group_{};
    std::unique_ptr<NativeRetailProfileRuntime> wave_;
    std::array<Companion,3> companions_{};
    std::shared_ptr<memory::NativeHeapOwner> overlay_parent_;
    memory::NativeHeapIdentity overlay_identity_{};
    std::optional<NativeClassProfilePlan> numeric_;
    std::uint64_t generation_{};
    RetailProfileMainPhase phase_{RetailProfileMainPhase::empty};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
