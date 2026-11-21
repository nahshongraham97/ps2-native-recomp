#include "mka/retail_profile_runtime.hpp"

#include <limits>
#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailProfileRuntimeFailure& f, RetailProfileRuntimeError kind) noexcept {
    f.kind = kind;
    return false;
}
}

NativeRetailProfileRuntime::NativeRetailProfileRuntime(
    NativeRetailMemoryOwners& group, memory::NativeHeapIdentity wave) noexcept
    : group_(&group), wave_identity_(wave) {}

std::unique_ptr<NativeRetailProfileRuntime> NativeRetailProfileRuntime::create(
    NativeRetailMemoryOwners& group, RetailProfileRuntimeFailure& f) noexcept {
    f = {};
    const auto wave = group.observe(RetailMemoryRole::wave, f.group);
    if (!wave && f.group.kind == RetailMemoryOwnerError::native_exception) {
        f.kind = RetailProfileRuntimeError::native_exception;
        return {};
    }
    if (!wave || wave->policy != RetailMemoryPolicy::wave_callback || !wave->heap.active) {
        f.kind = RetailProfileRuntimeError::missing_wave_owner;
        return {};
    }
    try {
        return std::unique_ptr<NativeRetailProfileRuntime>(
            new NativeRetailProfileRuntime(group, wave->heap.identity));
    } catch (const std::bad_alloc&) {
        f.kind = RetailProfileRuntimeError::allocation;
    } catch (...) {
        f.kind = RetailProfileRuntimeError::native_exception;
    }
    return {};
}

bool NativeRetailProfileRuntime::same_thread(
    RetailProfileRuntimeFailure& f) const noexcept {
    if (std::this_thread::get_id() == owner_thread_) return true;
    return fail(f, RetailProfileRuntimeError::wrong_thread);
}

bool NativeRetailProfileRuntime::access(NativeRetailMemoryOwners& group,
                                        RetailProfileRuntimeFailure& f) const noexcept {
    if (!same_thread(f)) return false;
    if (&group != group_) return fail(f, RetailProfileRuntimeError::foreign_group);
    const auto wave = group.observe(RetailMemoryRole::wave, f.group);
    if (!wave && f.group.kind == RetailMemoryOwnerError::native_exception)
        return fail(f, RetailProfileRuntimeError::native_exception);
    if (!wave || wave->policy != RetailMemoryPolicy::wave_callback ||
        !wave->heap.active || wave->heap.identity != wave_identity_)
        return fail(f, RetailProfileRuntimeError::missing_wave_owner);
    return true;
}

bool NativeRetailProfileRuntime::apply_numeric_and_wave_classes(
    const NativeClassProfilePlan& plan, NativeRetailMemoryOwners& group,
    RetailProfileRuntimeFailure& f) noexcept {
    f = {};
    if (!access(group, f)) return false;
    if (state_ == RetailProfileGenerationState::partial_irreversible)
        return fail(f, RetailProfileRuntimeError::partial_irreversible);
    if (!NativeRetailProfileOwners::valid_source_plan(plan))
        return fail(f, RetailProfileRuntimeError::invalid_profile_plan);
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        return fail(f, RetailProfileRuntimeError::generation_exhausted);

    // 182670 writes the numeric record before 310D30. There is deliberately
    // no native value for original unwritten word7.
    numeric_ = plan;
    const bool replacing = bool(wave_classes_);
    if (replacing && !wave_classes_->retire_source_classes(f.profile)) {
        if (wave_classes_->retirement_partial()) {
            state_ = RetailProfileGenerationState::partial_irreversible;
            return fail(f, RetailProfileRuntimeError::partial_irreversible);
        }
        return fail(f, f.profile.kind == RetailProfileOwnerError::busy ?
            RetailProfileRuntimeError::busy : RetailProfileRuntimeError::profile_owner);
    }
    auto successor = NativeRetailProfileOwners::create_initial_storage(plan, group, f.profile);
    if (!successor) {
        if (replacing) {
            state_ = RetailProfileGenerationState::partial_irreversible;
            return fail(f, RetailProfileRuntimeError::partial_irreversible);
        }
        return fail(f, RetailProfileRuntimeError::profile_owner);
    }
    wave_classes_ = std::move(successor);
    ++generation_;
    state_ = RetailProfileGenerationState::ready;
    return true;
}

std::optional<RetailProfileRuntimeObservation> NativeRetailProfileRuntime::observe(
    RetailProfileRuntimeFailure& f) const noexcept {
    f = {};
    if (!access(*group_, f)) return {};
    const std::optional<std::size_t> count =
        state_ == RetailProfileGenerationState::partial_irreversible ?
        std::optional<std::size_t>{} :
        std::optional<std::size_t>{wave_classes_ && !wave_classes_->retired() ? 13u : 0u};
    return RetailProfileRuntimeObservation{state_, generation_, numeric_,
        count, wave_identity_};
}

std::optional<NativeRetailProfilePayload> NativeRetailProfileRuntime::direct_payload(
    memory::AllocationClassRole role, RetailProfileRuntimeFailure& f) const noexcept {
    f = {};
    if (!access(*group_, f)) return {};
    if (state_ == RetailProfileGenerationState::partial_irreversible)
        { fail(f, RetailProfileRuntimeError::partial_irreversible); return {}; }
    if (!wave_classes_) { fail(f, RetailProfileRuntimeError::profile_owner); return {}; }
    auto leaf = wave_classes_->retained_direct_payload(role, f.profile);
    if (!leaf) fail(f, RetailProfileRuntimeError::profile_owner);
    return leaf;
}
} // namespace mka::retail
