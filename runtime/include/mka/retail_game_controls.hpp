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

// Proven native game controls: selection word/timing flag and analog-input
// inhibit state. This is no complete game-info resource or PS2 register model.
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
    bool analog_input_inhibited_{}; // Original byte660081 bit6, BSS clear.
};

} // namespace mka::retail
