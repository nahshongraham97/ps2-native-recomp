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

// The known 32-bit selection state at original g_game_info +4. Its low byte
// supplies the timing flag. Other bit meanings and the complete game-info
// record remain unproved.
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
};

} // namespace mka::retail
