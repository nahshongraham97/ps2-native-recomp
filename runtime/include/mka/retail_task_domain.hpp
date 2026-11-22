#pragma once

#include "mka/retail_game_controls.hpp"
#include "mka/retail_task_registry.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace mka::retail {

enum class RetailTaskDomainError {
    none, wrong_thread, controls, registry, registry_unborn,
    active_registry_reset, pending_source_followup,
    invalid_source_followup, allocation, native_exception
};
struct RetailTaskDomainFailure {
    RetailTaskDomainError kind{RetailTaskDomainError::none};
    const char* operation{ "" };
    RetailGameControlsFailure controls{};
    RetailTaskRegistryFailure registry{};
};

enum class RetailTaskLiveFollowup {
    none, source_245070, source_2451f0
};
struct RetailTaskLivePairUpdate {
    std::uint32_t current_word{};
    std::uint32_t partner_word{};
    RetailTaskLiveFollowup required_followup{RetailTaskLiveFollowup::none};
};
struct RetailTaskDomainObservation {
    std::uint32_t identity_counter{};
    bool override_pending{};
    std::uint32_t live_current_word{};
    std::uint32_t live_partner_word{};
    bool task_roots_born{};
    RetailTaskLiveFollowup pending_followup{RetailTaskLiveFollowup::none};
};

// Persistent source identity/control state plus ONE real ordered task registry.
// Registry birth is source 12F0D0; lookup and execution reuse that registry.
class NativeRetailTaskDomain final {
public:
    NativeRetailTaskDomain(const NativeRetailTaskDomain&) = delete;
    NativeRetailTaskDomain& operator=(const NativeRetailTaskDomain&) = delete;

    static std::unique_ptr<NativeRetailTaskDomain> create(
        std::shared_ptr<NativeRetailGameControls>,
        RetailTaskDomainFailure&) noexcept;
    bool seed_counter_from_185ee0(RetailTaskDomainFailure&) noexcept;
    bool reset_task_roots_from_12f0d0(
        std::shared_ptr<NativeRetailGamePool>, RetailTaskDomainFailure&) noexcept;
    bool cancel_first_root_from_12f030(RetailTaskDomainFailure&);
    bool dispatch_first_root_zero_flags_from_12e4e0(RetailTaskDomainFailure&);
    NativeRetailTaskScriptContextState* script_context(RetailTaskDomainFailure&) noexcept;
    bool override_cookie_from_130ab0(std::uint32_t,
        RetailTaskDomainFailure&) noexcept;
    std::optional<std::uint32_t> next_cookie_from_130a50(
        RetailTaskDomainFailure&) noexcept;

    // Real source producers of the 64C7C0/C4 pair. 28E2B0 reports the
    // exact additional call that must finish before another source effect.
    bool apply_live_pair_from_28e280(RetailTaskDomainFailure&) noexcept;
    std::optional<RetailTaskLivePairUpdate> begin_live_pair_from_28e2b0(
        std::int32_t first_status_64e914,
        std::int32_t second_status_64e920,
        RetailTaskDomainFailure&) noexcept;
    bool apply_live_current_from_345f50(bool source_service_zero,
        std::int32_t source_64eed4,
        RetailTaskDomainFailure&) noexcept;

    // Same live64DF64 read by Main1229E4 and task-list dispatch.
    std::optional<bool> root_wait_inhibited(RetailTaskDomainFailure&) const noexcept;
    NativeRetailTaskRegistry* registry(RetailTaskDomainFailure&) noexcept;
    NativeRetailTaskRegistry* prior_registry_if_born(
        RetailTaskDomainFailure&) noexcept;
    std::optional<RetailTaskDomainObservation> observe(
        RetailTaskDomainFailure&) const noexcept;

private:
    explicit NativeRetailTaskDomain(
        std::shared_ptr<NativeRetailGameControls> controls) noexcept
        : controls_(std::move(controls)) {}
    bool access(RetailTaskDomainFailure&) const noexcept;

    std::shared_ptr<NativeRetailGameControls> controls_;
    std::unique_ptr<NativeRetailTaskRegistry> registry_;
    NativeRetailTaskScriptContextState script_context_;
    std::uint32_t identity_counter_{}; // 64DF7C original BSS zero.
    bool override_pending_{};          // 64DF74 original BSS zero.
    std::uint32_t override_word_{};     // 64DF78 original BSS zero.
    std::uint32_t live_current_word_{5}; // 64C7C0 original ELF word.
    std::uint32_t live_partner_word_{5}; // 64C7C4 original ELF word.
    std::int32_t return_guard_64ea98_{}; // Original BSS zero.
    bool root_wait_inhibited_{}; // 64DF64, cleared by 12F0D0.
    RetailTaskLiveFollowup pending_followup_{RetailTaskLiveFollowup::none};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
