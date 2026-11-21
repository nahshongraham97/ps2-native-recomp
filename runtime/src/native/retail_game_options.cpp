#include "mka/retail_game_options.hpp"

namespace mka::retail {

bool NativeRetailGameOptions::validate_current_thread(
    RetailGameOptionsFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == owner_thread_) return true;
    failure.kind = RetailGameOptionsError::wrong_thread;
    failure.operation = "native retail game options owner thread";
    return false;
}

bool NativeRetailGameOptions::parse_selected_record_from_3433c0(
    std::optional<std::span<const std::uint8_t>> selected_record,
    RetailGameOptionsFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return false;
    values_.live_mode = 0; // 3433C4: branch delay slot, even for null input.
    if (!selected_record) return true;
    const auto record = *selected_record;
    if (record.empty()) {
        failure.kind = RetailGameOptionsError::truncated_selected_record;
        failure.operation = "3433C0 selected record missing first byte";
        return false;
    }
    const std::uint8_t first = record[0];
    if (first == 0) return true; // 3433CC: branch-likely annuls second read.
    if (record.size() < 2) {
        failure.kind = RetailGameOptionsError::truncated_selected_record;
        failure.operation = "3433C0 selected record missing second byte";
        return false;
    }
    if (record[1] != static_cast<std::uint8_t>('.')) return true;
    switch (first) {
    case static_cast<std::uint8_t>('2'): values_.live_mode = 1; break;
    case static_cast<std::uint8_t>('3'): values_.live_mode = 2; break;
    case static_cast<std::uint8_t>('4'): values_.live_mode = 3; break;
    default: break; // '1' and all other bytes leave the reset zero.
    }
    return true;
}

bool NativeRetailGameOptions::publish_six_words_from_164130(
    RetailGameOptionsFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return false;
    // Exact source store order; the fourth store precedes the 34EE30 call,
    // and the fifth is in that call's delay slot. 34EE30 returns four.
    values_.word_660380 = values_.live_mode == 3 ? 1u : 0u;
    values_.word_660384 = 3;
    values_.word_66038c = 3;
    values_.word_660388 = 1;
    values_.word_660390 = 1;
    values_.word_660394 = 4u - 2u;
    return true;
}

std::optional<RetailGameOptionsSnapshot> NativeRetailGameOptions::observe(
    RetailGameOptionsFailure& failure) const noexcept {
    if (!validate_current_thread(failure)) return {};
    return values_;
}

} // namespace mka::retail
