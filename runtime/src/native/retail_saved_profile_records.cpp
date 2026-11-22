#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "native_retail_startup.hpp"

#include <new>

namespace mka::retail {

NativeRetailSavedProfileRecords::NativeRetailSavedProfileRecords() noexcept
    : owner_(std::this_thread::get_id()) {}

bool NativeRetailSavedProfileRecords::initialize_records_from_171b10(
    std::int32_t language,
    RetailSavedProfileRecordsFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSavedProfileRecordsError::wrong_thread;
        failure.operation = "171B10 owner thread";
        return false;
    }
    if (initialized_) {
        failure.kind = RetailSavedProfileRecordsError::already_initialized;
        failure.operation = "171B10 records already initialized";
        return false;
    }

    // 171B10 card slot initialization: 2 card slots, 8 profiles each
    for (std::size_t slot_idx = 0; slot_idx < card_slot_count; ++slot_idx) {
        auto& slot = card_slots_[slot_idx];
        slot.set_status_word(-1);
        for (std::size_t p_idx = 0; p_idx < NativeRetailSavedCardSlot::profile_count; ++p_idx) {
            slot.set_profile_in_use(p_idx, false);
            auto& child = slot.child(p_idx);
            RetailSavedProfileDefaultFailure def_failure;
            if (!initialize_retail_saved_profile_known_defaults_from_245cb0(
                    child.defaults, language, shared_word_7bfc80_, def_failure)) {
                failure.kind = RetailSavedProfileRecordsError::defaults_failure;
                failure.operation = "171B10 child defaults initialization";
                failure.defaults = def_failure;
                return false;
            }
        }
        slot.set_flag_byte_12(0);
        slot.set_preferences(retail_profile_preference_defaults_from_2955d0());
    }

    if (!refresh_aggregate(failure)) {
        return false;
    }

    // After 2432A0 and 2955D0, mark both card slots ready (status 1)
    card_slots_[0].set_status_word(1);
    card_slots_[1].set_status_word(1);

    // Initial reset of active player profiles (players 0 and 1)
    for (std::size_t p = 0; p < active_player_count; ++p) {
        if (!reset_player_child_245dd0(p, language, failure)) {
            return false;
        }
    }

    initialized_ = true;
    return true;
}

bool NativeRetailSavedProfileRecords::refresh_aggregate(
    RetailSavedProfileRecordsFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSavedProfileRecordsError::wrong_thread;
        failure.operation = "2432A0 owner thread";
        return false;
    }

    std::array<std::vector<RetailProfileAggregateChildView>, card_slot_count> child_views;
    for (std::size_t s = 0; s < card_slot_count; ++s) {
        child_views[s].resize(NativeRetailSavedCardSlot::profile_count);
        for (std::size_t c = 0; c < NativeRetailSavedCardSlot::profile_count; ++c) {
            child_views[s][c] = card_slots_[s].child(c).make_aggregate_view();
        }
    }

    std::array<RetailProfileAggregateParentView, card_slot_count> parent_views{{
        {card_slots_[0].status_word(), card_slots_[0].version_word(), child_views[0]},
        {card_slots_[1].status_word(), card_slots_[1].version_word(), child_views[1]}
    }};

    RetailProfileAggregateFailure agg_failure;
    if (!aggregate_.refresh_data_from_2432a0(parent_views, 0, agg_failure)) {
        failure.kind = RetailSavedProfileRecordsError::aggregate_failure;
        failure.operation = "2432A0 aggregate refresh";
        failure.aggregate = agg_failure;
        return false;
    }

    return true;
}

bool NativeRetailSavedProfileRecords::reset_children_246070(
    std::int32_t language,
    RetailSavedProfileRecordsFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSavedProfileRecordsError::wrong_thread;
        failure.operation = "246070 owner thread";
        return false;
    }
    if (!initialized_) {
        failure.kind = RetailSavedProfileRecordsError::not_initialized;
        failure.operation = "246070 records not initialized";
        return false;
    }

    for (std::size_t p = 0; p < active_player_count; ++p) {
        if (!reset_player_child_245dd0(p, language, failure)) {
            return false;
        }
    }
    return true;
}

bool NativeRetailSavedProfileRecords::reset_player_child_245dd0(
    std::size_t player_index,
    std::int32_t language,
    RetailSavedProfileRecordsFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSavedProfileRecordsError::wrong_thread;
        failure.operation = "245DD0 owner thread";
        return false;
    }
    if (player_index >= active_player_count) {
        failure.kind = RetailSavedProfileRecordsError::invalid_player_index;
        failure.operation = "245DD0 player index out of bounds";
        return false;
    }

    auto& player = active_players_[player_index];
    const auto slot = player.card_slot;
    const auto idx = player.profile_index;
    if (slot >= 0 && slot < static_cast<std::int32_t>(card_slot_count) &&
        idx >= 0 && idx < static_cast<std::int32_t>(NativeRetailSavedCardSlot::profile_count)) {
        card_slots_[slot].set_profile_in_use(idx, false);
    }

    RetailSavedProfileDefaultFailure def_failure;
    if (!initialize_retail_saved_profile_known_defaults_from_245cb0(
            player.profile_data, language, shared_word_7bfc80_, def_failure)) {
        failure.kind = RetailSavedProfileRecordsError::defaults_failure;
        failure.operation = "245DD0 player profile defaults";
        failure.defaults = def_failure;
        return false;
    }

    player.card_slot = -1;
    player.profile_index = -1;
    player.dirty_flag = 0;
    player.context_word = 0;
    player.status = 8; // DAT_0064c720[player] = 8

    return true;
}

bool NativeRetailSavedProfileRecords::attach_player_profile(
    std::size_t player_index,
    std::size_t card_slot,
    std::size_t profile_index,
    RetailSavedProfileRecordsFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailSavedProfileRecordsError::wrong_thread;
        failure.operation = "attach_player_profile owner thread";
        return false;
    }
    if (!initialized_) {
        failure.kind = RetailSavedProfileRecordsError::not_initialized;
        failure.operation = "attach_player_profile not initialized";
        return false;
    }
    if (player_index >= active_player_count) {
        failure.kind = RetailSavedProfileRecordsError::invalid_player_index;
        failure.operation = "attach_player_profile player index";
        return false;
    }
    if (card_slot >= card_slot_count) {
        failure.kind = RetailSavedProfileRecordsError::invalid_slot;
        failure.operation = "attach_player_profile card slot";
        return false;
    }
    if (profile_index >= NativeRetailSavedCardSlot::profile_count) {
        failure.kind = RetailSavedProfileRecordsError::invalid_profile_index;
        failure.operation = "attach_player_profile profile index";
        return false;
    }

    auto& player = active_players_[player_index];
    if (player.card_slot >= 0 && player.card_slot < static_cast<std::int32_t>(card_slot_count) &&
        player.profile_index >= 0 && player.profile_index < static_cast<std::int32_t>(NativeRetailSavedCardSlot::profile_count)) {
        card_slots_[player.card_slot].set_profile_in_use(player.profile_index, false);
    }

    player.card_slot = static_cast<std::int32_t>(card_slot);
    player.profile_index = static_cast<std::int32_t>(profile_index);
    player.profile_data = card_slots_[card_slot].child(profile_index).defaults;
    player.dirty_flag = 0;
    player.status = 0;
    card_slots_[card_slot].set_profile_in_use(profile_index, true);

    return true;
}

namespace main_binding {

RetailStartupGate initialize_retail_saved_profile_records(
    NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.saved_profile_storage) {
        failure.kind = StartupError::saved_profile_storage;
        failure.operation = "171B10 native profile storage missing";
        return RetailStartupGate::native_failure;
    }
    if (!main.saved_profile_storage->validate(failure.saved_profile_storage)) {
        failure.kind = StartupError::saved_profile_storage;
        failure.operation = "171B10 native profile storage invalid";
        return RetailStartupGate::native_failure;
    }
    if (main.saved_profile_records) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "171B10 saved profile records already initialized";
        return RetailStartupGate::native_failure;
    }

    try {
        auto records = std::make_unique<NativeRetailSavedProfileRecords>();
        RetailSavedProfileRecordsFailure record_failure;
        if (!records->initialize_records_from_171b10(main.post_overlay_state, record_failure)) {
            failure.kind = StartupError::allocation;
            failure.operation = record_failure.operation ? record_failure.operation : "171B10 records init";
            return RetailStartupGate::native_failure;
        }
        main.saved_profile_records = std::move(records);
        return RetailStartupGate::ready;
    } catch (const std::bad_alloc&) {
        failure.kind = StartupError::allocation;
        failure.operation = "171B10 allocation";
        return RetailStartupGate::native_failure;
    } catch (...) {
        failure.kind = StartupError::native_exception;
        failure.operation = "171B10 native exception";
        return RetailStartupGate::native_failure;
    }
}

} // namespace main_binding

namespace initial_reset_required {

bool reset_saved_profile_children_246070(
    NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.saved_profile_records) {
        failure.kind = StartupError::saved_profile_storage;
        failure.operation = "246070 saved profile records missing";
        return false;
    }
    RetailSavedProfileRecordsFailure record_failure;
    if (!main.saved_profile_records->reset_children_246070(main.post_overlay_state, record_failure)) {
        failure.kind = StartupError::allocation;
        failure.operation = record_failure.operation ? record_failure.operation : "246070 reset children";
        return false;
    }
    return true;
}

} // namespace initial_reset_required

} // namespace mka::retail
