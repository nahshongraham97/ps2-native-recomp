#include "mka/retail_task_domain.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailTaskDomainFailure& f, RetailTaskDomainError kind,
          const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return false;
}
}

std::unique_ptr<NativeRetailTaskDomain> NativeRetailTaskDomain::create(
    std::shared_ptr<NativeRetailGameControls> controls,
    RetailTaskDomainFailure& f) noexcept {
    f = {};
    if (!controls) {
        fail(f, RetailTaskDomainError::controls, "task domain game controls absent");
        return {};
    }
    const auto selected = controls->selection_flags(f.controls);
    if (!selected) {
        fail(f, RetailTaskDomainError::controls, "task domain foreign game controls");
        return {};
    }
    try {
        // Source ELF supplies both live words as 5. Counter/override/reset
        // fields are zero-born; no registry root is published until 12F0D0.
        return std::unique_ptr<NativeRetailTaskDomain>(
            new NativeRetailTaskDomain(std::move(controls)));
    } catch (const std::bad_alloc&) {
        fail(f, RetailTaskDomainError::allocation, "task domain allocation");
    } catch (...) {
        fail(f, RetailTaskDomainError::native_exception, "task domain birth");
    }
    return {};
}

bool NativeRetailTaskDomain::access(RetailTaskDomainFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != owner_thread_)
        return fail(f, RetailTaskDomainError::wrong_thread, "task domain thread");
    if (pending_followup_ != RetailTaskLiveFollowup::none)
        return fail(f, RetailTaskDomainError::pending_source_followup,
                    "28E2B0 native followup not yet bound");
    return true;
}

bool NativeRetailTaskDomain::seed_counter_from_185ee0(
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return false;
    // 185EF0..F04 compares unsigned, preserving any already-high counter.
    if (identity_counter_ < 0x80000000u) identity_counter_ = 0xf0000000u;
    return true;
}

bool NativeRetailTaskDomain::reset_task_roots_from_12f0d0(
    std::shared_ptr<NativeRetailGamePool> pool,
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return false;
    if (registry_) {
        const auto state = registry_->observe(f.registry);
        if (!state) return fail(f, RetailTaskDomainError::registry,
                                "12F0D0 prior registry observation");
        if (!state->ordered.empty() || state->retained_links ||
            state->pending_links || state->after_effect_unknown)
            return fail(f, RetailTaskDomainError::active_registry_reset,
                        "12F0D0 live task retirement unbound");
    }
    // Prepare the native linked roots before clearing the source gate bits.
    // The second source root is BSS zero on first birth; a later live second
    // list cannot be silently dropped by this first-root implementation.
    auto candidate = NativeRetailTaskRegistry::create(std::move(pool),f.registry);
    if (!candidate)
        return fail(f, RetailTaskDomainError::registry,
                    "12F0D0 native task root allocation");
    if (!controls_->clear_task_gates_from_12f0d0(f.controls))
        return fail(f, RetailTaskDomainError::controls,
                    "12F0D0 byte 660081 gate clear");
    registry_ = std::move(candidate);
    root_wait_inhibited_ = false;
    return true;
}

bool NativeRetailTaskDomain::cancel_first_root_from_12f030(
    RetailTaskDomainFailure& f) {
    if (!access(f)) return false;
    if (!registry_)
        return fail(f, RetailTaskDomainError::registry_unborn,
                    "12F030 task first root absent");
    // Real body cleanup may transfer to Main; never flatten that exception.
    if (!registry_->cancel_first_root_from_12f030(f.registry))
        return fail(f, RetailTaskDomainError::registry,
                    "12F030 task first-root cleanup");
    return true;
}

bool NativeRetailTaskDomain::dispatch_first_root_zero_flags_from_12e4e0(
    RetailTaskDomainFailure& f) {
    if (!access(f)) return false;
    if (!registry_)
        return fail(f, RetailTaskDomainError::registry_unborn,
                    "12E4E0 task roots have not been born");
    const auto inhibited=controls_->task_dispatch_inhibited_bit5(f.controls);
    if (!inhibited)
        return fail(f, RetailTaskDomainError::controls,
                    "12E670 live byte 660081 bit5");
    // This is the source initial first-list/no-script/zero-task-flags path.
    // The second source root is BSS-empty until a real producer attaches it.
    if (!registry_->dispatch_first_root_zero_flags_from_12e670(
            *inhibited, root_wait_inhibited_, f.registry))
        return fail(f, RetailTaskDomainError::registry,
                    "12E4E0 first task-list dispatch");
    return true;
}

bool NativeRetailTaskDomain::override_cookie_from_130ab0(
    std::uint32_t value, RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return false;
    // Store value first, then publish one-shot pending flag.
    override_word_ = value;
    override_pending_ = true;
    return true;
}

std::optional<std::uint32_t> NativeRetailTaskDomain::next_cookie_from_130a50(
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return {};
    if (override_pending_) {
        override_pending_ = false;
        return override_word_;
    }
    --identity_counter_;
    if (identity_counter_ < 2u) identity_counter_ = 0xf0000000u;
    return (identity_counter_ & 0x7fffffffu) |
           ((live_current_word_ & 1u) << 31);
}

bool NativeRetailTaskDomain::apply_live_pair_from_28e280(
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return false;
    if (live_current_word_ != 5 && return_guard_64ea98_ == 0) {
        live_current_word_ = 5;
        live_partner_word_ = 5;
    }
    return_guard_64ea98_ = 0;
    return true;
}

std::optional<RetailTaskLivePairUpdate>
NativeRetailTaskDomain::begin_live_pair_from_28e2b0(
    std::int32_t first_status_64e914,
    std::int32_t second_status_64e920,
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return {};
    const auto flags = controls_->selection_flags(f.controls);
    if (!flags) {
        fail(f, RetailTaskDomainError::controls, "28E2B0 selection flags");
        return {};
    }
    RetailTaskLiveFollowup followup = RetailTaskLiveFollowup::none;
    if ((*flags & 0x08u) != 0) {
        live_current_word_ = 5;
        live_partner_word_ = 5;
    } else if ((*flags & 0x02u) != 0) {
        live_current_word_ = 0;
        live_partner_word_ = 1;
        if (first_status_64e914 == 1 && second_status_64e920 != 1)
            followup = RetailTaskLiveFollowup::source_245070;
    } else {
        live_partner_word_ = 0;
        live_current_word_ = 1;
        if (second_status_64e920 == 1 && first_status_64e914 != 1)
            followup = RetailTaskLiveFollowup::source_2451f0;
    }
    pending_followup_ = followup;
    return RetailTaskLivePairUpdate{live_current_word_, live_partner_word_, followup};
}

bool NativeRetailTaskDomain::apply_live_current_from_345f50(
    bool source_service_zero, std::int32_t source_64eed4,
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return false;
    if (source_service_zero)
        live_current_word_ = source_64eed4 == 1 ? 0u : 1u;
    return true;
}

NativeRetailTaskRegistry* NativeRetailTaskDomain::registry(
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return nullptr;
    if (!registry_) {
        fail(f, RetailTaskDomainError::registry_unborn,
             "12F0D0 task roots have not been born");
        return nullptr;
    }
    return registry_.get();
}

NativeRetailTaskRegistry* NativeRetailTaskDomain::prior_registry_if_born(
    RetailTaskDomainFailure& f) noexcept {
    if (!access(f)) return nullptr;
    return registry_.get();
}

std::optional<RetailTaskDomainObservation> NativeRetailTaskDomain::observe(
    RetailTaskDomainFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != owner_thread_) {
        fail(f, RetailTaskDomainError::wrong_thread, "task domain observation thread");
        return {};
    }
    return RetailTaskDomainObservation{identity_counter_, override_pending_,
        live_current_word_, live_partner_word_, bool(registry_), pending_followup_};
}

} // namespace mka::retail
