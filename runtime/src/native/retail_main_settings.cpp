#include "mka/retail_main_settings.hpp"

#include <new>
#include <utility>

namespace mka::retail {

NativeRetailMainSettings::NativeRetailMainSettings(
    std::shared_ptr<NativeRetailGameControls> controls) noexcept
    : controls_(std::move(controls)) {}

std::shared_ptr<NativeRetailMainSettings> NativeRetailMainSettings::create(
    std::shared_ptr<NativeRetailGameControls> controls,
    RetailMainSettingsFailure& failure) noexcept {
    failure = {};
    if (!controls) {
        failure = {RetailMainSettingsError::invalid_owner, "retail settings game controls missing", {}};
        return {};
    }
    RetailGameControlsFailure controls_failure;
    if (!controls->selection_flags(controls_failure)) {
        failure = {RetailMainSettingsError::wrong_thread,
            "retail settings game controls thread", controls_failure};
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailMainSettings>(
            new NativeRetailMainSettings(std::move(controls)));
    } catch (const std::bad_alloc&) {
        failure = {RetailMainSettingsError::allocation, "retail settings allocation", {}};
    } catch (...) {
        failure = {RetailMainSettingsError::native_exception, "retail settings creation", {}};
    }
    return {};
}

bool NativeRetailMainSettings::access(RetailMainSettingsFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == owner_thread_) return true;
    failure = {RetailMainSettingsError::wrong_thread, "retail settings thread", {}};
    return false;
}

std::optional<bool> NativeRetailMainSettings::override_condition(
    RetailMainSettingsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    RetailGameControlsFailure controls_failure;
    const auto flags = controls_->selection_flags(controls_failure);
    if (!flags) {
        failure = {RetailMainSettingsError::wrong_thread,
            "retail settings game controls read", controls_failure};
        return {};
    }
    return (*flags & 1u) != 0 && override_word_ != 0;
}

std::optional<std::uint32_t> NativeRetailMainSettings::profile_selector(
    RetailMainSettingsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return profile_selector_;
}

std::optional<std::uint32_t> NativeRetailMainSettings::stored_frame_rate(
    RetailMainSettingsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return stored_frame_rate_;
}

std::optional<std::uint32_t> NativeRetailMainSettings::override_word(
    RetailMainSettingsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return override_word_;
}

std::optional<bool> NativeRetailMainSettings::timing_override_active(
    RetailMainSettingsFailure& failure) const noexcept {
    return override_condition(failure);
}

std::optional<std::int32_t> NativeRetailMainSettings::effective_frame_rate(
    RetailMainSettingsFailure& failure) const noexcept {
    const auto active = override_condition(failure);
    if (!active) return {};
    return static_cast<std::int32_t>(*active ? 60u : stored_frame_rate_);
}

bool NativeRetailMainSettings::store_profile_selector_from_1225d0(
    std::uint32_t value, RetailMainSettingsFailure& failure) noexcept {
    if (!access(failure)) return false;
    profile_selector_ = value;
    return true;
}

bool NativeRetailMainSettings::store_video_rate_from_220460(
    std::uint32_t mode, RetailMainSettingsFailure& failure) noexcept {
    if (!access(failure)) return false;
    if (mode > 2) {
        failure = {RetailMainSettingsError::invalid_video_mode,
            "retail settings source video mode", {}};
        return false;
    }
    stored_frame_rate_ = mode == 0 ? 50u : 60u;
    return true;
}

bool NativeRetailMainSettings::store_override_word_from_source(
    std::uint32_t value, RetailMainSettingsFailure& failure) noexcept {
    if (!access(failure)) return false;
    override_word_ = value;
    return true;
}

} // namespace mka::retail
