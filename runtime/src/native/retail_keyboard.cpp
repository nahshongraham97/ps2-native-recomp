#include "mka/retail_keyboard.hpp"

#include <algorithm>
#include <array>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mka::retail {
namespace {
struct SourceKeyRow {
    std::uint8_t usage, flags;
    std::uint16_t plain, shifted, alternate, alternate_shifted;
};
// Original SLUS_214.10 5F2980: 74 ten-byte rows, each decoded little-endian.
constexpr std::array<SourceKeyRow, 74> source_keys{{
    {0x35, 0x00, 0x0060, 0x007e, 0x0000, 0x0000},
    {0x1e, 0x00, 0x0031, 0x0021, 0x0000, 0x0000},
    {0x1f, 0x00, 0x0032, 0x0040, 0x0000, 0x0000},
    {0x20, 0x00, 0x0033, 0x0023, 0x0000, 0x0000},
    {0x21, 0x00, 0x0034, 0x0024, 0x0000, 0x0000},
    {0x22, 0x00, 0x0035, 0x0025, 0x0000, 0x0000},
    {0x23, 0x00, 0x0036, 0x005e, 0x0000, 0x0000},
    {0x24, 0x00, 0x0037, 0x0026, 0x0000, 0x0000},
    {0x25, 0x00, 0x0038, 0x002a, 0x0000, 0x0000},
    {0x26, 0x00, 0x0039, 0x0028, 0x0000, 0x0000},
    {0x27, 0x00, 0x0030, 0x0029, 0x0000, 0x0000},
    {0x2d, 0x00, 0x002d, 0x005f, 0x0000, 0x0000},
    {0x2e, 0x00, 0x003d, 0x002b, 0x0000, 0x0000},
    {0x89, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x2a, 0x00, 0x0008, 0x0008, 0x0000, 0x0000},
    {0x2b, 0x00, 0x0009, 0x0009, 0x0000, 0x0000},
    {0x14, 0x01, 0x0071, 0x0051, 0x0000, 0x0000},
    {0x1a, 0x01, 0x0077, 0x0057, 0x0000, 0x0000},
    {0x08, 0x01, 0x0065, 0x0045, 0x0000, 0x0000},
    {0x15, 0x01, 0x0072, 0x0052, 0x0000, 0x0000},
    {0x17, 0x01, 0x0074, 0x0054, 0x0000, 0x0000},
    {0x1c, 0x01, 0x0079, 0x0059, 0x0000, 0x0000},
    {0x18, 0x01, 0x0075, 0x0055, 0x0000, 0x0000},
    {0x0c, 0x01, 0x0069, 0x0049, 0x0000, 0x0000},
    {0x12, 0x01, 0x006f, 0x004f, 0x0000, 0x0000},
    {0x13, 0x01, 0x0070, 0x0050, 0x0000, 0x0000},
    {0x2f, 0x00, 0x005b, 0x007b, 0x0000, 0x0000},
    {0x30, 0x00, 0x005d, 0x007d, 0x0000, 0x0000},
    {0x31, 0x00, 0x005c, 0x007c, 0x0000, 0x0000},
    {0x04, 0x01, 0x0061, 0x0041, 0x0000, 0x0000},
    {0x16, 0x01, 0x0073, 0x0053, 0x0000, 0x0000},
    {0x07, 0x01, 0x0064, 0x0044, 0x0000, 0x0000},
    {0x09, 0x01, 0x0066, 0x0046, 0x0000, 0x0000},
    {0x0a, 0x01, 0x0067, 0x0047, 0x0000, 0x0000},
    {0x0b, 0x01, 0x0068, 0x0048, 0x0000, 0x0000},
    {0x0d, 0x01, 0x006a, 0x004a, 0x0000, 0x0000},
    {0x0e, 0x01, 0x006b, 0x004b, 0x0000, 0x0000},
    {0x0f, 0x01, 0x006c, 0x004c, 0x0000, 0x0000},
    {0x33, 0x00, 0x003b, 0x003a, 0x0000, 0x0000},
    {0x34, 0x00, 0x0027, 0x0022, 0x0000, 0x0000},
    {0x32, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x28, 0x00, 0x000a, 0x000a, 0x0000, 0x0000},
    {0x64, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x1d, 0x01, 0x007a, 0x005a, 0x0000, 0x0000},
    {0x1b, 0x01, 0x0078, 0x0058, 0x0000, 0x0000},
    {0x06, 0x01, 0x0063, 0x0043, 0x0000, 0x0000},
    {0x19, 0x01, 0x0076, 0x0056, 0x0000, 0x0000},
    {0x05, 0x01, 0x0062, 0x0042, 0x0000, 0x0000},
    {0x11, 0x01, 0x006e, 0x004e, 0x0000, 0x0000},
    {0x10, 0x01, 0x006d, 0x004d, 0x0000, 0x0000},
    {0x36, 0x00, 0x002c, 0x003c, 0x0000, 0x0000},
    {0x37, 0x00, 0x002e, 0x003e, 0x0000, 0x0000},
    {0x38, 0x00, 0x002f, 0x003f, 0x0000, 0x0000},
    {0x87, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x8b, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x2c, 0x00, 0x0020, 0x0020, 0x0000, 0x0000},
    {0x8a, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x88, 0x00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x29, 0x00, 0x001b, 0x001b, 0x0000, 0x0000},
    {0x54, 0x02, 0x002f, 0x002f, 0x0000, 0x0000},
    {0x55, 0x02, 0x002a, 0x002a, 0x0000, 0x0000},
    {0x56, 0x02, 0x002d, 0x002d, 0x0000, 0x0000},
    {0x57, 0x02, 0x002b, 0x002b, 0x0000, 0x0000},
    {0x58, 0x02, 0x000a, 0x000a, 0x0000, 0x0000},
    {0x59, 0x02, 0x0031, 0x0000, 0x0000, 0x0000},
    {0x5a, 0x02, 0x0032, 0x0000, 0x0000, 0x0000},
    {0x5b, 0x02, 0x0033, 0x0000, 0x0000, 0x0000},
    {0x5c, 0x02, 0x0034, 0x0000, 0x0000, 0x0000},
    {0x5d, 0x02, 0x0035, 0x0000, 0x0000, 0x0000},
    {0x5e, 0x02, 0x0036, 0x0000, 0x0000, 0x0000},
    {0x5f, 0x02, 0x0037, 0x0000, 0x0000, 0x0000},
    {0x60, 0x02, 0x0038, 0x0000, 0x0000, 0x0000},
    {0x61, 0x02, 0x0039, 0x0000, 0x0000, 0x0000},
    {0x62, 0x02, 0x0030, 0x0000, 0x0000, 0x0000},
}};

struct DecodedKey { std::optional<std::uint16_t> code; bool num_lock_fallback; };
DecodedKey decode_key(std::uint32_t usage, const RetailKeyboardModifiers& mods) noexcept {
    // The source table contains no modifier usages. These are held modifier
    // state in the original report, not ordinary emitted key entries.
    if (usage >= 0xe0 && usage <= 0xe7) return {{}, false};
    const auto it = std::find_if(source_keys.begin(), source_keys.end(),
        [usage](const SourceKeyRow& row) { return row.usage == usage; });
    if (it == source_keys.end()) return {{}, false};
    // SDL MODE/AltGr is not silently equated with the source right-Alt bit.
    if (mods.mode && !mods.right_alt) return {{}, false};
    if ((it->flags & 2u) && !mods.num_lock)
        return {std::uint16_t(usage + 0x100u), true};
    const bool shift = mods.left_shift || mods.right_shift;
    const bool alternate = mods.right_alt;
    const bool shifted = alternate ? shift : (shift != (mods.caps_lock && (it->flags & 1u)));
    const auto code = alternate ? (shifted ? it->alternate_shifted : it->alternate) :
        (shifted ? it->shifted : it->plain);
    return {code ? code : std::uint16_t(usage + 0x100u), false};
}
}

std::unique_ptr<NativeRetailKeyboard> NativeRetailKeyboard::create(RetailKeyboardFailure& failure) noexcept {
    failure = {};
    try { return std::unique_ptr<NativeRetailKeyboard>(new NativeRetailKeyboard); }
    catch (const std::bad_alloc&) {
        failure = {RetailKeyboardError::allocation, "native keyboard owner allocation"};
    } catch (...) {
        failure = {RetailKeyboardError::native_exception, "native keyboard owner construction"};
    }
    return {};
}

bool NativeRetailKeyboard::on_owner_thread(RetailKeyboardFailure& failure) const noexcept {
    if (owner_thread_ == std::this_thread::get_id()) return true;
    failure = {RetailKeyboardError::wrong_thread, "native keyboard owner thread"};
    return false;
}

bool NativeRetailKeyboard::append(RetailKeyboardEvent event, RetailKeyboardFailure& failure) noexcept {
    try {
        events_.push_back(std::move(event));
        return true;
    } catch (const std::bad_alloc&) {
        failure = {RetailKeyboardError::allocation, "native keyboard event allocation"};
    } catch (const std::length_error&) {
        failure = {RetailKeyboardError::host_capacity, "native keyboard event capacity"};
    } catch (...) {
        failure = {RetailKeyboardError::native_exception, "native keyboard event publication"};
    }
    return false;
}

bool NativeRetailKeyboard::publish_scancode(std::uint32_t scancode,
    RetailKeyboardTransition transition, RetailKeyboardModifiers modifiers,
    RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return false;
    if (closed_) {
        failure = {RetailKeyboardError::closed, "native keyboard closed"};
        return false;
    }
    // E0..E7 are the source report's held modifier bits, not one of its
    // ordinary key entries. Preserve state without publishing a key word.
    if (scancode >= 0xe0 && scancode <= 0xe7) {
        modifiers_ = modifiers;
        return true;
    }
    const auto decoded = decode_key(scancode, modifiers);
    RetailKeyboardEvent event;
    event.kind = RetailKeyboardEventKind::physical;
    event.physical = {scancode, transition, modifiers, decoded.code,
        decoded.num_lock_fallback};
    if (!append(std::move(event), failure)) return false;
    modifiers_ = modifiers;
    return true;
}

bool NativeRetailKeyboard::publish_text(char32_t codepoint,
    RetailKeyboardModifiers modifiers, RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return false;
    if (closed_) {
        failure = {RetailKeyboardError::closed, "native keyboard closed"};
        return false;
    }
    if (codepoint == 0 || codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        failure = {RetailKeyboardError::invalid_text, "native keyboard text codepoint"};
        return false;
    }
    RetailKeyboardEvent event;
    event.kind = RetailKeyboardEventKind::text;
    event.text = {codepoint, modifiers};
    if (!append(std::move(event), failure)) return false;
    modifiers_ = modifiers;
    return true;
}

bool NativeRetailKeyboard::publish_utf8(std::string_view bytes,
    RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return false;
    if (closed_) {
        failure = {RetailKeyboardError::closed, "native keyboard closed"};
        return false;
    }
    const auto original_size = events_.size();
    try {
        std::vector<char32_t> scalars;
        for (std::size_t i = 0; i < bytes.size();) {
            auto next = [&]() { return static_cast<unsigned char>(bytes[i++]); };
            const auto first = next();
            char32_t code = first;
            unsigned trailing = 0;
            char32_t minimum = 1;
            if (first >= 0xc2 && first <= 0xdf) { code = first & 0x1f; trailing = 1; minimum = 0x80; }
            else if (first >= 0xe0 && first <= 0xef) { code = first & 0x0f; trailing = 2; minimum = 0x800; }
            else if (first >= 0xf0 && first <= 0xf4) { code = first & 7; trailing = 3; minimum = 0x10000; }
            else if (first == 0 || first >= 0x80) {
                failure = {RetailKeyboardError::invalid_text, "native keyboard UTF-8 leading byte"};
                return false;
            }
            if (trailing > bytes.size() - i) {
                failure = {RetailKeyboardError::invalid_text, "native keyboard truncated UTF-8"};
                return false;
            }
            for (unsigned n = 0; n != trailing; ++n) {
                const auto continuation = next();
                if ((continuation & 0xc0) != 0x80) {
                    failure = {RetailKeyboardError::invalid_text, "native keyboard UTF-8 continuation"};
                    return false;
                }
                code = (code << 6) | (continuation & 0x3f);
            }
            if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) {
                failure = {RetailKeyboardError::invalid_text, "native keyboard UTF-8 scalar"};
                return false;
            }
            scalars.push_back(code);
        }
        for (auto code : scalars) {
            RetailKeyboardEvent event;
            event.kind = RetailKeyboardEventKind::text;
            event.text = {code, modifiers_};
            events_.push_back(event);
        }
        return true;
    } catch (const std::bad_alloc&) {
        failure = {RetailKeyboardError::allocation, "native keyboard UTF-8 batch allocation"};
    } catch (const std::length_error&) {
        failure = {RetailKeyboardError::host_capacity, "native keyboard UTF-8 batch capacity"};
    } catch (...) {
        failure = {RetailKeyboardError::native_exception, "native keyboard UTF-8 batch publication"};
    }
    while (events_.size() != original_size) events_.pop_back();
    return false;
}

bool NativeRetailKeyboard::publish_focus(bool focused, RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return false;
    if (closed_) {
        failure = {RetailKeyboardError::closed, "native keyboard closed"};
        return false;
    }
    RetailKeyboardEvent event;
    event.kind = RetailKeyboardEventKind::focus;
    event.focused = focused;
    if (!append(event, failure)) return false;
    if (!focused) modifiers_ = {};
    return true;
}

std::optional<RetailKeyboardObservation> NativeRetailKeyboard::observe(
    RetailKeyboardFailure& failure) const noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return {};
    return RetailKeyboardObservation{events_.size(), modifiers_, closed_};
}

std::optional<std::size_t> NativeRetailKeyboard::drain(
    std::span<RetailKeyboardEvent> output, RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return {};
    if (closed_) {
        failure = {RetailKeyboardError::closed, "native keyboard closed"};
        return {};
    }
    const auto count = std::min(output.size(), events_.size());
    for (std::size_t i = 0; i < count; ++i) {
        output[i] = std::move(events_.front());
        events_.pop_front();
    }
    return count;
}

bool NativeRetailKeyboard::shutdown(RetailKeyboardFailure& failure) noexcept {
    failure = {};
    if (!on_owner_thread(failure)) return false;
    if (closed_) {
        failure = {RetailKeyboardError::closed, "native keyboard already closed"};
        return false;
    }
    events_.clear();
    modifiers_ = {};
    closed_ = true;
    return true;
}

} // namespace mka::retail
