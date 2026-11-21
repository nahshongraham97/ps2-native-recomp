#pragma once

#include "mka/retail_game_controls.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailMainSettingsError {
    none, wrong_thread, invalid_owner, invalid_video_mode, allocation, native_exception
};
struct RetailMainSettingsFailure {
    RetailMainSettingsError kind{RetailMainSettingsError::none};
    const char* operation{""};
    RetailGameControlsFailure controls;
};

// Source scalar state shared by initial main and later reentry. The separate
// game-control owner supplies the live flag; no copied flag or host switch.
class NativeRetailMainSettings final {
public:
    static std::shared_ptr<NativeRetailMainSettings> create(
        std::shared_ptr<NativeRetailGameControls>, RetailMainSettingsFailure&) noexcept;
    NativeRetailMainSettings(const NativeRetailMainSettings&) = delete;
    NativeRetailMainSettings& operator=(const NativeRetailMainSettings&) = delete;

    std::optional<std::uint32_t> profile_selector(
        RetailMainSettingsFailure&) const noexcept;
    std::optional<std::uint32_t> stored_frame_rate(
        RetailMainSettingsFailure&) const noexcept;
    std::optional<std::uint32_t> override_word(
        RetailMainSettingsFailure&) const noexcept;
    std::optional<bool> timing_override_active(
        RetailMainSettingsFailure&) const noexcept;
    std::optional<std::int32_t> effective_frame_rate(
        RetailMainSettingsFailure&) const noexcept;

    // Narrow source stores; these do not complete their enclosing functions.
    bool store_profile_selector_from_1225d0(
        std::uint32_t, RetailMainSettingsFailure&) noexcept;
    bool store_video_rate_from_220460(
        std::uint32_t mode, RetailMainSettingsFailure&) noexcept;
    bool store_override_word_from_source(
        std::uint32_t, RetailMainSettingsFailure&) noexcept;

private:
    explicit NativeRetailMainSettings(std::shared_ptr<NativeRetailGameControls>) noexcept;
    bool access(RetailMainSettingsFailure&) const noexcept;
    std::optional<bool> override_condition(RetailMainSettingsFailure&) const noexcept;

    std::shared_ptr<NativeRetailGameControls> controls_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::uint32_t profile_selector_{13}; // Original initialized data.
    std::uint32_t stored_frame_rate_{60}; // Original initialized data.
    std::uint32_t override_word_{}; // Original BSS zero fill.
};

} // namespace mka::retail
