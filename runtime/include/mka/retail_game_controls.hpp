#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailGameControlsError { none, wrong_thread, allocation, native_exception };
struct RetailGameControlsFailure {
    RetailGameControlsError kind{RetailGameControlsError::none};
    const char* operation{""};
};

// Proven native game controls: selection word/timing flag and the distinct
// primary game-info word. Its byte 1 contains the actual input/task gates.
// This is no complete game-info resource or PS2 register model.
// Other flag meanings, writers and the full resource remain separate.
class NativeRetailGameControls final {
public:
    static std::shared_ptr<NativeRetailGameControls> create(
        RetailGameControlsFailure&) noexcept;
    NativeRetailGameControls(const NativeRetailGameControls&) = delete;
    NativeRetailGameControls& operator=(const NativeRetailGameControls&) = delete;

    std::optional<std::uint8_t> selection_flags(
        RetailGameControlsFailure&) const noexcept;
    std::optional<std::uint32_t> selection_state_word(
        RetailGameControlsFailure&) const noexcept;
    // Live logical state originally read by 280A60. The complete 163930
    // role-mask coordinator changes this alongside the three role gates.
    std::optional<bool> analog_input_inhibited(RetailGameControlsFailure&) const noexcept;
    bool store_analog_input_inhibited_from_source(bool, RetailGameControlsFailure&) noexcept;
    std::optional<std::uint32_t> primary_game_info_word(
        RetailGameControlsFailure&) const noexcept;
    // 16A9D0 clears the full word. 185EE0 later clears only byte-0 bit 7.
    bool reset_primary_game_info_word_from_16a9d0(
        RetailGameControlsFailure&) noexcept;
    bool clear_primary_byte0_bit7_from_185ee0(
        RetailGameControlsFailure&) noexcept;
    std::optional<bool> primary_byte0_bit7(
        RetailGameControlsFailure&) const noexcept;
    // 12F0D0 -> 182BB0(0) clears byte-1 bits 5 and 3 only.
    bool clear_task_gates_from_12f0d0(RetailGameControlsFailure&) noexcept;
    std::optional<bool> task_dispatch_inhibited_bit5(
        RetailGameControlsFailure&) const noexcept;
    std::optional<bool> task_wait_frame_delta_bit3(
        RetailGameControlsFailure&) const noexcept;
    // A source byte writer changes only the low byte of the known word.
    bool store_selection_flags_from_source(
        std::uint8_t, RetailGameControlsFailure&) noexcept;
    // 1303F0 stores zero to the complete 32-bit word.
    bool clear_selection_state_from_1303f0(RetailGameControlsFailure&) noexcept;

private:
    NativeRetailGameControls() noexcept = default;
    bool access(RetailGameControlsFailure&) const noexcept;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::uint32_t selection_state_{}; // Original BSS zero fill.
    std::uint32_t primary_game_info_word_{}; // Original 660080 BSS zero fill.
};

} // namespace mka::retail
