#include "mka/retail_keyboard.hpp"
#include "mka/retail_keyboard_sdl.hpp"

#include <SDL_events.h>
#include <SDL_keycode.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>

using namespace mka::retail;

namespace {
std::atomic<int> fail_after{-1};
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void arm(int n) noexcept { fail_after.store(n, std::memory_order_relaxed); }
void disarm() noexcept { arm(-1); }

std::array<std::uint8_t, 740> raw_table(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    check(bool(stream), "frozen original 740-byte table opens");
    std::array<std::uint8_t, 740> bytes{};
    stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    check(stream.gcount() == static_cast<std::streamsize>(bytes.size()) &&
        stream.peek() == std::char_traits<char>::eof(), "original table has exactly 740 bytes");
    return bytes;
}

std::uint16_t halfword(const std::uint8_t* p) noexcept {
    return std::uint16_t(p[0]) | (std::uint16_t(p[1]) << 8);
}

std::unique_ptr<NativeRetailKeyboard> owner() {
    RetailKeyboardFailure failure;
    auto value = NativeRetailKeyboard::create(failure);
    check(value && failure.kind == RetailKeyboardError::none, "keyboard owner creation");
    return value;
}

RetailKeyboardObservation observation(NativeRetailKeyboard& keyboard) {
    RetailKeyboardFailure failure;
    auto value = keyboard.observe(failure);
    check(value && failure.kind == RetailKeyboardError::none, "keyboard observation");
    return *value;
}

RetailKeyboardEvent take_one(NativeRetailKeyboard& keyboard) {
    std::array<RetailKeyboardEvent, 1> output{};
    RetailKeyboardFailure failure;
    auto count = keyboard.drain(output, failure);
    check(count == 1 && failure.kind == RetailKeyboardError::none, "one keyboard event drained");
    return output[0];
}

void source_table_cases(NativeRetailKeyboard& keyboard, const std::array<std::uint8_t, 740>& raw) {
    for (std::size_t row = 0; row != 74; ++row) {
        const auto* p = raw.data() + row * 10;
        const auto flags = p[0], usage = p[1];
        const std::array<std::uint16_t, 4> values{
            halfword(p+2), halfword(p+4), halfword(p+6), halfword(p+8)};
        for (unsigned mode = 0; mode != 5; ++mode) {
            RetailKeyboardModifiers mods;
            mods.num_lock = mode != 4;
            mods.left_shift = mode == 1 || mode == 3;
            mods.caps_lock = mode == 2;
            mods.right_alt = mode == 3;
            const bool keypad_fallback = (flags & 2u) && !mods.num_lock;
            const unsigned selected = mode == 1 ? 1 : mode == 2 ? ((flags & 1u) ? 1 : 0) :
                mode == 3 ? 3 : 0;
            const auto expected = keypad_fallback || values[selected] == 0 ?
                std::uint16_t(usage + 0x100u) : values[selected];
            RetailKeyboardFailure failure;
            check(keyboard.publish_scancode(usage, RetailKeyboardTransition::press, mods, failure) &&
                failure.kind == RetailKeyboardError::none, "source table row published");
            const auto event = take_one(keyboard);
            check(event.kind == RetailKeyboardEventKind::physical &&
                event.physical.scancode == usage && event.physical.source_us_code == expected &&
                event.physical.source_keypad_without_num_lock == keypad_fallback,
                "source table row matches independent original bytes");
        }
    }
}

void ordering_and_decoder_cases(NativeRetailKeyboard& keyboard) {
    RetailKeyboardFailure failure;
    SDL_Event event{};
    constexpr std::uint32_t window = 17;
    event.type = SDL_KEYDOWN;
    event.key.windowID = window + 1;
    event.key.keysym.scancode = SDL_SCANCODE_A;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure) &&
        observation(keyboard).pending == 0, "foreign-window key ignored");
    event.type = SDL_TEXTINPUT;
    event.text.windowID = window + 1;
    std::memcpy(event.text.text, "foreign", 8);
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure) &&
        observation(keyboard).pending == 0, "foreign-window text ignored");

    event = {};
    event.type = SDL_KEYDOWN;
    event.key.windowID = window;
    event.key.keysym.scancode = SDL_SCANCODE_A;
    event.key.keysym.mod = KMOD_LSHIFT | KMOD_CAPS | KMOD_NUM;
    event.key.repeat = 0;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure), "SDL key press");
    event.key.repeat = 1;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure), "SDL key repeat");
    event.type = SDL_TEXTINPUT;
    event.text.windowID = window;
    std::memcpy(event.text.text, "\xF0\x9F\x98\x80", 5);
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure), "SDL non-BMP text");
    event.type = SDL_KEYUP;
    event.key.windowID = window;
    event.key.keysym.scancode = SDL_SCANCODE_A;
    event.key.keysym.mod = KMOD_CAPS;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure), "SDL key release");
    check(observation(keyboard).pending == 4, "physical/text/release kept separately");
    const auto press = take_one(keyboard), repeat = take_one(keyboard);
    const auto text = take_one(keyboard), release = take_one(keyboard);
    const bool press_expected = press.kind == RetailKeyboardEventKind::physical &&
        press.physical.transition == RetailKeyboardTransition::press &&
        press.physical.modifiers.left_shift && press.physical.modifiers.caps_lock &&
        press.physical.modifiers.num_lock && press.physical.source_us_code == 'a';
    if (!press_expected) {
        std::fprintf(stderr,
            "DIAGNOSTIC press kind=%d transition=%d scancode=%u mods shift=%d caps=%d num=%d "
            "rshift=%d mode=%d source_code=%d keypad_fallback=%d order kinds=%d,%d,%d,%d "
            "transitions=%d,%d,%d\n",
            int(press.kind), int(press.physical.transition), press.physical.scancode,
            int(press.physical.modifiers.left_shift), int(press.physical.modifiers.caps_lock),
            int(press.physical.modifiers.num_lock), int(press.physical.modifiers.right_shift),
            int(press.physical.modifiers.mode),
            press.physical.source_us_code ? int(*press.physical.source_us_code) : -1,
            int(press.physical.source_keypad_without_num_lock), int(press.kind),
            int(repeat.kind), int(text.kind), int(release.kind),
            int(press.physical.transition), int(repeat.physical.transition),
            int(release.physical.transition));
    }
    check(press_expected, "SDL modifiers and Caps/Shift table selection");
    check(repeat.kind == RetailKeyboardEventKind::physical &&
        repeat.physical.transition == RetailKeyboardTransition::repeat &&
        text.kind == RetailKeyboardEventKind::text && text.text.codepoint == 0x1f600 &&
        release.kind == RetailKeyboardEventKind::physical &&
        release.physical.transition == RetailKeyboardTransition::release,
        "repeat, delayed non-BMP text and physical release order");

    RetailKeyboardModifiers mods;
    mods.left_control = true;
    check(keyboard.publish_scancode(0x1234, RetailKeyboardTransition::press, mods, failure),
        "host scancode outside source eight-bit field");
    const auto large = take_one(keyboard);
    check(large.physical.scancode == 0x1234 && !large.physical.source_us_code,
        "large host scancode identity retained without invented source code");
    check(keyboard.publish_scancode(0xe0, RetailKeyboardTransition::press, mods, failure) &&
        observation(keyboard).pending == 0 && observation(keyboard).modifiers.left_control,
        "source held-modifier scancode changes state without ordinary key");

    event = {};
    event.type = SDL_KEYDOWN;
    event.key.windowID = window;
    event.key.keysym.scancode = SDL_SCANCODE_B;
    event.key.keysym.mod = KMOD_LCTRL | KMOD_RCTRL | KMOD_LSHIFT | KMOD_RSHIFT |
        KMOD_LALT | KMOD_RALT | KMOD_LGUI | KMOD_RGUI |
        KMOD_NUM | KMOD_CAPS | KMOD_SCROLL | KMOD_MODE | 0x0004;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure),
        "all known SDL modifier bits decoded");
    const auto all_mods = take_one(keyboard).physical.modifiers;
    check(all_mods.left_control && all_mods.right_control &&
        all_mods.left_shift && all_mods.right_shift &&
        all_mods.left_alt && all_mods.right_alt &&
        all_mods.left_gui && all_mods.right_gui && all_mods.num_lock &&
        all_mods.caps_lock && all_mods.scroll_lock && all_mods.mode &&
        all_mods.unmapped_host_bits == 0x0004, "SDL modifier fields remain distinct");
    RetailKeyboardModifiers mode_only;
    mode_only.mode = true;
    check(keyboard.publish_scancode(4, RetailKeyboardTransition::press, mode_only, failure),
        "unproved MODE-only translation retains physical event");
    check(!take_one(keyboard).physical.source_us_code,
        "MODE alone does not invent a source right-Alt mapping");
    RetailKeyboardModifiers held_for_focus;
    held_for_focus.left_control = true;
    held_for_focus.right_alt = true;
    held_for_focus.mode = true;
    check(keyboard.publish_scancode(0xe0, RetailKeyboardTransition::press,
        held_for_focus, failure) && observation(keyboard).modifiers.left_control &&
        observation(keyboard).modifiers.right_alt && observation(keyboard).modifiers.mode,
        "multiple held modifiers precede focus loss");

    event = {};
    event.type = SDL_WINDOWEVENT;
    event.window.windowID = window + 1;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure) &&
        observation(keyboard).pending == 0, "foreign-window focus ignored");
    event.window.windowID = window;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure), "SDL focus loss");
    check(observation(keyboard).pending == 1 && !observation(keyboard).modifiers.left_control &&
        !observation(keyboard).modifiers.right_alt && !observation(keyboard).modifiers.mode,
        "focus loss clears held modifiers after event publication");
    const auto focus_lost = take_one(keyboard);
    check(focus_lost.kind == RetailKeyboardEventKind::focus && !focus_lost.focused,
        "focus loss remains typed event");
    event.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
    check(publish_retail_keyboard_sdl_event(keyboard, event, window, failure) &&
        take_one(keyboard).focused, "focus gain remains typed event");

    for (unsigned i = 0; i != 20; ++i)
        check(keyboard.publish_scancode(0x200+i, RetailKeyboardTransition::press, {}, failure),
            "unbounded native queue accepts more than six events");
    std::array<RetailKeyboardEvent, 7> first{};
    check(keyboard.drain(first, failure) == 7 && observation(keyboard).pending == 13,
        "partial drain preserves queue suffix");
    for (unsigned i = 0; i != 7; ++i)
        check(first[i].physical.scancode == 0x200+i, "partial drain order");
    for (unsigned i = 7; i != 20; ++i)
        check(take_one(keyboard).physical.scancode == 0x200+i, "queue tail order");
}

void text_validation_cases(NativeRetailKeyboard& keyboard) {
    RetailKeyboardFailure failure;
    RetailKeyboardModifiers mods;
    mods.left_shift = true;
    check(keyboard.publish_scancode(4, RetailKeyboardTransition::press, mods, failure),
        "baseline physical event before invalid text");
    const auto before = observation(keyboard);
    const std::array<std::string_view, 8> bad{
        std::string_view("A\0B", 3), std::string_view("\xC2", 1),
        std::string_view("\xE2\x28\xA1", 3), std::string_view("\xC0\xAF", 2),
        std::string_view("\xE0\x80\x80", 3), std::string_view("\xED\xA0\x80", 3),
        std::string_view("\xF4\x90\x80\x80", 4), std::string_view("\xF5\x80\x80\x80", 4)};
    for (auto bytes : bad) {
        check(!keyboard.publish_utf8(bytes, failure) &&
            failure.kind == RetailKeyboardError::invalid_text, "invalid UTF-8 rejected");
        const auto after = observation(keyboard);
        check(after.pending == before.pending && after.modifiers.left_shift,
            "invalid entire text batch leaves queue and modifier state unchanged");
    }
    for (char32_t cp : {char32_t(0), char32_t(0xd800), char32_t(0x110000)})
        check(!keyboard.publish_text(cp, {}, failure) &&
            failure.kind == RetailKeyboardError::invalid_text,
            "NUL or invalid committed text codepoint rejected");
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    event.text.windowID = 17;
    std::memset(event.text.text, 'x', sizeof(event.text.text));
    check(!publish_retail_keyboard_sdl_event(keyboard, event, 17, failure) &&
        failure.kind == RetailKeyboardError::invalid_text &&
        observation(keyboard).pending == before.pending,
        "non-terminated SDL 32-byte text fails without publication");
    check(take_one(keyboard).physical.scancode == 4, "baseline event survives text failures");
}

void allocation_cases() {
    RetailKeyboardFailure failure;
    arm(0);
    auto missing = NativeRetailKeyboard::create(failure);
    disarm();
    check(!missing && failure.kind == RetailKeyboardError::allocation,
        "owner allocation failure named");
    bool key_failure = false, text_failure = false;
    for (int cut = 0; cut != 20; ++cut) {
        auto keyboard = owner();
        arm(cut);
        bool reached_cut = false;
        for (unsigned i = 0; i != 300; ++i) {
            const auto before = observation(*keyboard);
            RetailKeyboardModifiers mods;
            mods.caps_lock = (i & 1u) != 0;
            const bool ok = keyboard->publish_scancode(4, RetailKeyboardTransition::press, mods, failure);
            if (!ok) {
                reached_cut = true;
                disarm();
                const auto after = observation(*keyboard);
                check(failure.kind == RetailKeyboardError::allocation &&
                    after.pending == before.pending &&
                    after.modifiers.caps_lock == before.modifiers.caps_lock,
                    "failed key append leaves queue and modifier state unchanged");
                break;
            }
        }
        disarm();
        key_failure = key_failure || reached_cut;
    }
    for (int cut = 0; cut != 20; ++cut) {
        auto keyboard = owner();
        RetailKeyboardModifiers mods;
        mods.left_shift = true;
        check(keyboard->publish_scancode(4, RetailKeyboardTransition::press, mods, failure),
            "baseline event before text allocation cut");
        arm(cut);
        const bool ok = keyboard->publish_utf8(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", failure);
        disarm();
        if (!ok) {
            text_failure = true;
            const auto state = observation(*keyboard);
            check(failure.kind == RetailKeyboardError::allocation && state.pending == 1 &&
                state.modifiers.left_shift && take_one(*keyboard).physical.scancode == 4,
                "failed text batch rolls back queue without changing modifiers");
        } else check(observation(*keyboard).pending == 81, "text batch survives fault cut");
    }
    check(key_failure && text_failure, "allocation sweeps reached both key and text failures");
}

void thread_and_close_cases() {
    auto keyboard = owner();
    RetailKeyboardFailure failure;
    std::array<bool, 6> wrong{};
    std::thread other([&] {
        RetailKeyboardFailure f;
        wrong[0] = !keyboard->publish_scancode(4, RetailKeyboardTransition::press, {}, f) &&
            f.kind == RetailKeyboardError::wrong_thread;
        wrong[1] = !keyboard->publish_text(U'A', {}, f) && f.kind == RetailKeyboardError::wrong_thread;
        wrong[2] = !keyboard->publish_utf8("A", f) && f.kind == RetailKeyboardError::wrong_thread;
        wrong[3] = !keyboard->publish_focus(false, f) && f.kind == RetailKeyboardError::wrong_thread;
        std::array<RetailKeyboardEvent, 1> output{};
        wrong[4] = !keyboard->drain(output, f) && f.kind == RetailKeyboardError::wrong_thread;
        wrong[5] = !keyboard->observe(f) && f.kind == RetailKeyboardError::wrong_thread &&
            !keyboard->shutdown(f) && f.kind == RetailKeyboardError::wrong_thread;
    });
    other.join();
    for (bool ok : wrong) check(ok, "off-owner operation rejected by name");
    check(observation(*keyboard).pending == 0, "wrong thread published nothing");
    check(keyboard->publish_scancode(4, RetailKeyboardTransition::press, {}, failure),
        "event before shutdown");
    check(keyboard->shutdown(failure) && failure.kind == RetailKeyboardError::none,
        "first shutdown succeeds");
    const auto state = observation(*keyboard);
    check(state.closed && state.pending == 0, "shutdown retires queue");
    check(!keyboard->shutdown(failure) && failure.kind == RetailKeyboardError::closed,
        "duplicate shutdown fails by name");
    check(!keyboard->publish_scancode(4, RetailKeyboardTransition::press, {}, failure) &&
        failure.kind == RetailKeyboardError::closed, "closed owner rejects key");
    check(!keyboard->publish_text(U'A', {}, failure) && failure.kind == RetailKeyboardError::closed,
        "closed owner rejects text");
    check(!keyboard->publish_utf8("A", failure) && failure.kind == RetailKeyboardError::closed,
        "closed owner rejects batch");
    check(!keyboard->publish_focus(true, failure) && failure.kind == RetailKeyboardError::closed,
        "closed owner rejects focus");
    std::array<RetailKeyboardEvent, 1> output{};
    check(!keyboard->drain(output, failure) && failure.kind == RetailKeyboardError::closed,
        "closed owner rejects drain");
}
} // namespace

void* operator new(std::size_t size) {
    const int count = fail_after.load(std::memory_order_relaxed);
    if (count >= 0 && fail_after.fetch_sub(1, std::memory_order_relaxed) == 0)
        throw std::bad_alloc();
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
    try {
        check(argc == 2, "fixture needs independently frozen original 740-byte table path");
        const auto raw = raw_table(argv[1]);
        auto keyboard = owner();
        source_table_cases(*keyboard, raw);
        ordering_and_decoder_cases(*keyboard);
        text_validation_cases(*keyboard);
        allocation_cases();
        thread_and_close_cases();
        std::puts("PASS native keyboard queue and SDL event decoding; physical UI/game open");
        return 0;
    } catch (const std::exception& e) {
        disarm();
        std::fprintf(stderr, "FAIL native keyboard queue: %s\n", e.what());
        return 1;
    }
}
