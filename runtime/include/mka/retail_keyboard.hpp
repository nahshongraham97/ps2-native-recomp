#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <thread>

namespace mka::retail {

enum class RetailKeyboardError {
    none, wrong_thread, closed, invalid_text,
    allocation, host_capacity, native_exception
};
struct RetailKeyboardFailure {
    RetailKeyboardError kind{RetailKeyboardError::none};
    const char* operation{""};
};

// Semantic host state copied from an SDL key event. MODE is retained
// separately: its relationship to the original right-Alt bit is unproved.
struct RetailKeyboardModifiers {
    bool num_lock{}, caps_lock{}, scroll_lock{}, mode{};
    bool left_control{}, left_shift{}, left_alt{}, left_gui{};
    bool right_control{}, right_shift{}, right_alt{}, right_gui{};
    std::uint16_t unmapped_host_bits{};
};
enum class RetailKeyboardTransition { press, release, repeat };
struct RetailKeyboardPhysicalEvent {
    std::uint32_t scancode{};
    RetailKeyboardTransition transition{};
    RetailKeyboardModifiers modifiers{};
    // Only the low 16-bit US-layout result of the source 74-row key table.
    // No source modifier/lock/event word is claimed without its producer join.
    std::optional<std::uint16_t> source_us_code;
    bool source_keypad_without_num_lock{};
};
struct RetailKeyboardTextEvent {
    char32_t codepoint{};
    RetailKeyboardModifiers modifiers{};
};
enum class RetailKeyboardEventKind { physical, text, focus };
struct RetailKeyboardEvent {
    RetailKeyboardEventKind kind{};
    RetailKeyboardPhysicalEvent physical{};
    RetailKeyboardTextEvent text{};
    bool focused{};
};
struct RetailKeyboardObservation {
    std::size_t pending{};
    RetailKeyboardModifiers modifiers{};
    bool closed{};
};

// Native event owner for the keyboard domain. Input comes from host events;
// no IOP module, USB slot, guest address, or fixed report buffer is present.
// The source 3D9970/3D8E50 word adapter still needs the native UI consumer.
class NativeRetailKeyboard final {
public:
    static std::unique_ptr<NativeRetailKeyboard> create(RetailKeyboardFailure&) noexcept;
    NativeRetailKeyboard(const NativeRetailKeyboard&) = delete;
    NativeRetailKeyboard& operator=(const NativeRetailKeyboard&) = delete;
    ~NativeRetailKeyboard() = default;

    // SDL2's 74 matching scancode values are checked against source usages.
    // All other host scancodes retain identity without a source US code.
    bool publish_scancode(std::uint32_t scancode, RetailKeyboardTransition,
        RetailKeyboardModifiers, RetailKeyboardFailure&) noexcept;
    // Text is a distinct native event; it is never guessed to belong to the
    // last physical key and preserves the full Unicode scalar.
    bool publish_text(char32_t codepoint, RetailKeyboardModifiers,
        RetailKeyboardFailure&) noexcept;
    // Validate the entire batch before publication. Invalid input or capacity
    // failure leaves the queue and modifier state unchanged.
    bool publish_utf8(std::string_view, RetailKeyboardFailure&) noexcept;
    bool publish_focus(bool focused, RetailKeyboardFailure&) noexcept;
    std::optional<std::size_t> drain(std::span<RetailKeyboardEvent>, RetailKeyboardFailure&) noexcept;
    bool shutdown(RetailKeyboardFailure&) noexcept;
    std::optional<RetailKeyboardObservation> observe(RetailKeyboardFailure&) const noexcept;

private:
    NativeRetailKeyboard() noexcept = default;
    bool on_owner_thread(RetailKeyboardFailure&) const noexcept;
    bool append(RetailKeyboardEvent, RetailKeyboardFailure&) noexcept;

    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::deque<RetailKeyboardEvent> events_;
    RetailKeyboardModifiers modifiers_{};
    bool closed_{};
};

} // namespace mka::retail
