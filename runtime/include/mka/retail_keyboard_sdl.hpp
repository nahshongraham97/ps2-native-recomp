#pragma once
#include "mka/retail_keyboard.hpp"
union SDL_Event;

namespace mka::retail {
// Event decoding only; no SDL call, window creation, device polling or host
// state query. The platform passes each event exactly once in poll order.
bool publish_retail_keyboard_sdl_event(NativeRetailKeyboard&, const SDL_Event&,
    std::uint32_t window_id, RetailKeyboardFailure&) noexcept;
} // namespace mka::retail
