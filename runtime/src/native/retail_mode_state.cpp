#include "mka/retail_mode_state.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>

namespace mka::retail {

void NativeRetailModeState::clear() noexcept {
    history_.clear();
}

bool NativeRetailModeState::push(std::int32_t value, RetailModeFailure& failure) noexcept {
    failure = {};
    // 185D94 compares the current top before 185D9C clamps the old index.
    if (!history_.empty() && history_.back() == value) return true;
    // Old index 7 advances to slot 8. Every subsequent distinct push writes
    // slot 8 again; this is source game behavior, not a host storage limit.
    // No signed value is rejected: BGEZL admits nonnegative values, while
    // every negative signed32 value satisfies the following SLTI 27.
    if (history_.size() > 8) history_.resize(8);
    try {
        history_.push_back(value);
        return true;
    } catch (const std::bad_alloc&) {
        failure = {RetailModeError::allocation, "retail mode push"};
    } catch (const std::length_error&) {
        failure = {RetailModeError::allocation, "retail mode push"};
    }
    return false;
}

void NativeRetailModeState::pop() noexcept {
    if (!history_.empty()) history_.pop_back();
}

std::int32_t NativeRetailModeState::top() const noexcept {
    return history_.empty() ? 0 : history_.back();
}

std::int32_t NativeRetailModeState::top_past_21() const noexcept {
    if (history_.empty()) return 0;
    for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
        if (*it != 21) return *it;
    }
    // 185E18 loads before 185E24 decrements. If every entry is 21,
    // 185E30 returns the last loaded 21 after the index becomes -1.
    return 21;
}

bool NativeRetailModeState::contains(std::int32_t value) const noexcept {
    // 185E54 starts at slot zero and checks through the current top.
    return std::find(history_.begin(), history_.end(), value) != history_.end();
}

} // namespace mka::retail
