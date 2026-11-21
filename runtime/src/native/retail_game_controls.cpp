#include "mka/retail_game_controls.hpp"

#include <new>

namespace mka::retail {

std::shared_ptr<NativeRetailGameControls> NativeRetailGameControls::create(
    RetailGameControlsFailure& failure) noexcept {
    failure = {};
    try {
        return std::shared_ptr<NativeRetailGameControls>(new NativeRetailGameControls);
    } catch (const std::bad_alloc&) {
        failure = {RetailGameControlsError::allocation, "retail game controls allocation"};
    } catch (...) {
        failure = {RetailGameControlsError::native_exception, "retail game controls creation"};
    }
    return {};
}

bool NativeRetailGameControls::access(RetailGameControlsFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == owner_thread_) return true;
    failure = {RetailGameControlsError::wrong_thread, "retail game controls thread"};
    return false;
}

std::optional<std::uint8_t> NativeRetailGameControls::selection_flags(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return static_cast<std::uint8_t>(selection_state_);
}

std::optional<std::uint32_t> NativeRetailGameControls::selection_state_word(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return selection_state_;
}

std::optional<bool> NativeRetailGameControls::analog_input_inhibited(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return (primary_game_info_word_ & 0x4000u) != 0;
}
bool NativeRetailGameControls::store_analog_input_inhibited_from_source(
    bool value, RetailGameControlsFailure& failure) noexcept {
    if (!access(failure)) return false;
    primary_game_info_word_ = value ?
        primary_game_info_word_ | 0x4000u :
        primary_game_info_word_ & ~0x4000u;
    return true;
}

std::optional<std::uint32_t> NativeRetailGameControls::primary_game_info_word(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return primary_game_info_word_;
}
bool NativeRetailGameControls::reset_primary_game_info_word_from_16a9d0(
    RetailGameControlsFailure& failure) noexcept {
    if (!access(failure)) return false;
    primary_game_info_word_ = 0;
    return true;
}
bool NativeRetailGameControls::clear_primary_byte0_bit7_from_185ee0(
    RetailGameControlsFailure& failure) noexcept {
    if (!access(failure)) return false;
    primary_game_info_word_ &= ~0x80u;
    return true;
}
std::optional<bool> NativeRetailGameControls::primary_byte0_bit7(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return (primary_game_info_word_ & 0x80u) != 0;
}
bool NativeRetailGameControls::clear_task_gates_from_12f0d0(
    RetailGameControlsFailure& failure) noexcept {
    if (!access(failure)) return false;
    primary_game_info_word_ &= ~(0x2000u | 0x0800u);
    return true;
}
std::optional<bool> NativeRetailGameControls::task_dispatch_inhibited_bit5(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return (primary_game_info_word_ & 0x2000u) != 0;
}
std::optional<bool> NativeRetailGameControls::task_wait_frame_delta_bit3(
    RetailGameControlsFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return (primary_game_info_word_ & 0x0800u) != 0;
}

bool NativeRetailGameControls::store_selection_flags_from_source(
    std::uint8_t value, RetailGameControlsFailure& failure) noexcept {
    if (!access(failure)) return false;
    selection_state_ = (selection_state_ & 0xffffff00u) | value;
    return true;
}

bool NativeRetailGameControls::clear_selection_state_from_1303f0(
    RetailGameControlsFailure& failure) noexcept {
    if (!access(failure)) return false;
    selection_state_ = 0;
    return true;
}

} // namespace mka::retail
