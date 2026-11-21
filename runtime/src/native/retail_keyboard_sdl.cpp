#include "mka/retail_keyboard_sdl.hpp"
#include <SDL_events.h>
#include <cstring>

namespace mka::retail {
namespace {
RetailKeyboardModifiers modifiers(std::uint16_t bits) noexcept {
    RetailKeyboardModifiers result;
    result.num_lock = (bits & KMOD_NUM) != 0;
    result.caps_lock = (bits & KMOD_CAPS) != 0;
    result.scroll_lock = (bits & KMOD_SCROLL) != 0;
    result.mode = (bits & KMOD_MODE) != 0;
    result.left_control = (bits & KMOD_LCTRL) != 0;
    result.left_shift = (bits & KMOD_LSHIFT) != 0;
    result.left_alt = (bits & KMOD_LALT) != 0;
    result.left_gui = (bits & KMOD_LGUI) != 0;
    result.right_control = (bits & KMOD_RCTRL) != 0;
    result.right_shift = (bits & KMOD_RSHIFT) != 0;
    result.right_alt = (bits & KMOD_RALT) != 0;
    result.right_gui = (bits & KMOD_RGUI) != 0;
    constexpr auto known = KMOD_NUM | KMOD_CAPS | KMOD_SCROLL | KMOD_MODE |
        KMOD_LCTRL | KMOD_LSHIFT | KMOD_LALT | KMOD_LGUI |
        KMOD_RCTRL | KMOD_RSHIFT | KMOD_RALT | KMOD_RGUI;
    result.unmapped_host_bits = bits & ~known;
    return result;
}
}
bool publish_retail_keyboard_sdl_event(NativeRetailKeyboard& keyboard,
    const SDL_Event& event, std::uint32_t window_id, RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        if (event.key.windowID != window_id) return true;
        const auto transition = event.type == SDL_KEYUP ? RetailKeyboardTransition::release :
            event.key.repeat ? RetailKeyboardTransition::repeat : RetailKeyboardTransition::press;
        return keyboard.publish_scancode(static_cast<std::uint32_t>(event.key.keysym.scancode),
            transition, modifiers(event.key.keysym.mod), failure);
    }
    if (event.type == SDL_TEXTINPUT) {
        if (event.text.windowID != window_id) return true;
        const auto* end = static_cast<const char*>(std::memchr(event.text.text, '\0', sizeof(event.text.text)));
        if (!end) {
            failure = {RetailKeyboardError::invalid_text, "native SDL text lacks terminator"};
            return false;
        }
        return keyboard.publish_utf8({event.text.text, static_cast<std::size_t>(end - event.text.text)}, failure);
    }
    if (event.type == SDL_WINDOWEVENT && event.window.windowID == window_id &&
        (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || event.window.event == SDL_WINDOWEVENT_FOCUS_LOST))
        return keyboard.publish_focus(event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED, failure);
    return true;
}
} // namespace mka::retail
