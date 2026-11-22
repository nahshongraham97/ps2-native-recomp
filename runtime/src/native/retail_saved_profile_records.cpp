#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_participants.hpp"
#include "mka/retail_profile_aggregate_main.hpp"
#include "mka/retail_profile_preferences.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <new>
#include <stdexcept>
#include <vector>

namespace mka::retail {

NativeRetailSavedProfileRecords::NativeRetailSavedProfileRecords() noexcept
    : owner_thread_(std::this_thread::get_id()) {}

bool NativeRetailSavedProfileRecords::access(StartupFailure& failure, const char* operation) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_thread_) {
        failure.kind = StartupError::native_exception;
        failure.operation = operation;
        return false;
    }
    return true;
}

const NativeRetailSavedCardSlot& NativeRetailSavedProfileRecords::card_slot(std::size_t index) const {
    if (index >= card_slot_count) {
        throw std::out_of_range("card slot index out of range");
    }
    return card_slots_[index];
}

NativeRetailSavedCardSlot& NativeRetailSavedProfileRecords::card_slot(std::size_t index) {
    if (index >= card_slot_count) {
        throw std::out_of_range("card slot index out of range");
    }
    return card_slots_[index];
}

const NativeRetailActivePlayerProfile& NativeRetailSavedProfileRecords::active_player(std::size_t player) const {
    if (player >= active_player_count) {
        throw std::out_of_range("active player index out of range");
    }
    return active_players_[player];
}

NativeRetailActivePlayerProfile& NativeRetailSavedProfileRecords::active_player(std::size_t player) {
    if (player >= active_player_count) {
        throw std::out_of_range("active player index out of range");
    }
    return active_players_[player];
}

bool NativeRetailSavedProfileRecords::initialize_records_from_171b10(
    NativeRetailMainState& main, StartupFailure& failure) {
    if (!access(failure, "171B10 owner thread")) return false;
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::native_exception;
        failure.operation = "171B10 Main lifetime/thread";
        return false;
    }
    if (initialized_) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "171B10 saved profile records already initialized";
        return false;
    }

    // 171B10 card slot initialization: 2 card slots, 8 profiles each
    for (std::size_t slot_idx = 0; slot_idx < card_slot_count; ++slot_idx) {
        auto& slot = card_slots_[slot_idx];

        // 00171B68: sw v0, 0(s2) where v0 = -1
        // Published status -1 per parent before its aggregate pass so that aggregate/menu sees retained state
        slot.set_status_word(-1);

        for (std::size_t p_idx = 0; p_idx < NativeRetailSavedCardSlot::profile_count; ++p_idx) {
            // 00171BA8: sw zero, 0x18(s1) in delay slot of 245CB0 clears parent+0x18 + index*4
            slot.set_profile_in_use(p_idx, false);

            auto& child = slot.child(p_idx);
            child.gate_word_3c28 = 0;
            child.or_word_fc = 0;
            child.or_words_100_108_110_118 = {0, 0, 0, 0};
            child.and_words_120.fill(0xffffffffu);
            child.and_words_1c0.fill(0xffffffffu);

            RetailSavedProfileDefaultFailure def_failure;
            if (!initialize_retail_saved_profile_known_defaults_from_245cb0(
                    child.defaults, main.post_overlay_state, shared_word_7bfc80_, def_failure)) {
                failure.kind = StartupError::allocation;
                failure.operation = "171B10 child defaults initialization";
                return false;
            }
        }

        // 00171BC4: sb zero, 0xc(s2) in delay slot of 2432A0 clears parent+0x0C
        slot.set_flag_byte_0c(0);

        // 00171BC0: jal 0x002432a0
        // Prepare parent views for both card slots
        std::array<std::vector<RetailProfileAggregateChildView>, card_slot_count> child_views;
        for (std::size_t s = 0; s < card_slot_count; ++s) {
            child_views[s].resize(NativeRetailSavedCardSlot::profile_count);
            for (std::size_t c = 0; c < NativeRetailSavedCardSlot::profile_count; ++c) {
                child_views[s][c] = card_slots_[s].child(c).make_view();
            }
        }

        std::array<RetailProfileAggregateParentView, card_slot_count> parent_views{{
            RetailProfileAggregateParentView{
                .status_word = card_slots_[0].status_word(),
                .version_word = card_slots_[0].version_word(),
                .children = child_views[0]
            },
            RetailProfileAggregateParentView{
                .status_word = card_slots_[1].status_word(),
                .version_word = card_slots_[1].version_word(),
                .children = child_views[1]
            }
        }};

        RetailProfileAggregateMainFailure agg_failure;
        // NativeMainReentry must propagate cleanly through to caller
        if (!refresh_retail_profile_aggregate_from_2432a0(main, parent_views, agg_failure)) {
            failure.kind = StartupError::native_exception;
            failure.operation = agg_failure.operation ? agg_failure.operation : "171B10 aggregate/menu refresh";
            return false;
        }

        // 00171BC8: jal 0x002955d0 with parent+0x38 in delay slot
        slot.set_preferences(retail_profile_preference_defaults_from_2955d0());
    }

    // 00171BF4 / 00171BFC: Mark both card slots ready (status 1) after both parents complete
    card_slots_[0].set_status_word(1);
    card_slots_[1].set_status_word(1);

    initialized_ = true;
    return true;
}

bool NativeRetailSavedProfileRecords::reset_player_child_245dd0(
    NativeRetailMainState& main, std::size_t player, StartupFailure& failure) {
    if (!access(failure, "245DD0 owner thread")) return false;
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::native_exception;
        failure.operation = "245DD0 Main lifetime/thread";
        return false;
    }
    if (player >= active_player_count) {
        failure.kind = StartupError::native_exception;
        failure.operation = "245DD0 player index out of bounds";
        return false;
    }

    // 00245DF0: jal 0x00281830 with player in a0
    // Reads participant's actual role index.
    // 00245F0C..00245F18: Conditionally clears returned role's first word (source_control_word)
    // BEFORE active record clear/defaults.
    if (main.participants && main.input_roles) {
        RetailParticipantFailure pf;
        auto part = main.participants->first_two(static_cast<std::uint32_t>(player), pf);
        if (part) {
            const auto obs = part->observe(pf);
            if (obs && obs->assigned_role >= 0) {
                RetailInputRoleFailure rf;
                main.input_roles->clear_source_control_word_from_281830(
                    static_cast<std::size_t>(obs->assigned_role), rf);
            }
        }
    }

    // 00245E88..00245F08: Unset card slot in-use flag if player had an attached card profile
    auto& active = active_players_[player];
    const auto slot_idx = active.associated_card_slot;
    const auto prof_idx = active.associated_profile_slot;
    if (slot_idx >= 0 && slot_idx < static_cast<std::int32_t>(card_slot_count) &&
        prof_idx >= 0 && prof_idx < static_cast<std::int32_t>(NativeRetailSavedCardSlot::profile_count)) {
        card_slots_[slot_idx].set_profile_in_use(static_cast<std::size_t>(prof_idx), false);
    }

    // 00245F1C..00246014: Reset active player defaults
    RetailSavedProfileDefaultFailure def_failure;
    if (!initialize_retail_saved_profile_known_defaults_from_245cb0(
            active.defaults, main.post_overlay_state, shared_word_7bfc80_, def_failure)) {
        failure.kind = StartupError::allocation;
        failure.operation = "245DD0 active player defaults";
        return false;
    }

    // 00246018 / 00246020 / 0024602C:
    // sw zero, 0(s5) -> clear dirty flag (gp - 0x57d0 for p0 / gp - 0x57dc for p1)
    // sw -1, 0(s3)   -> unassign card slot (gp - 0x57d4 for p0 / gp - 0x57e0 for p1)
    // sw -1, 0(s2)   -> unassign profile slot (gp - 0x57d8 for p0 / gp - 0x57e4 for p1)
    active.dirty_flag = 0;
    active.associated_card_slot = -1;
    active.associated_profile_slot = -1;

    // 00246028: jal 0x002817b0 with participant in a0
    // In 2817B0: rereads participant assigned role, clears its source_control_word when present,
    // and clears retained per-player word (gp - 0x565c for p0 / gp - 0x5660 for p1)
    if (main.participants && main.input_roles) {
        RetailParticipantFailure pf;
        auto part = main.participants->first_two(static_cast<std::uint32_t>(player), pf);
        if (part) {
            const auto obs = part->observe(pf);
            if (obs && obs->assigned_role >= 0) {
                RetailInputRoleFailure rf;
                main.input_roles->clear_source_control_word_from_281830(
                    static_cast<std::size_t>(obs->assigned_role), rf);
            }
        }
    }
    active.retained_per_player_word = 0;

    // 00246040: sw a1, 0(v1) where a1 = 8, v1 = gp - 0x79d0 + player * 4 (0x0064C720 + player * 4)
    active.player_status = 8;

    return true;
}

bool NativeRetailSavedProfileRecords::reset_saved_profile_children_246070(
    NativeRetailMainState& main, StartupFailure& failure) {
    if (!access(failure, "246070 owner thread")) return false;
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::native_exception;
        failure.operation = "246070 Main lifetime/thread";
        return false;
    }

    for (std::size_t player = 0; player < active_player_count; ++player) {
        if (!reset_player_child_245dd0(main, player, failure)) {
            return false;
        }
    }
    return true;
}

namespace main_binding {

RetailStartupGate initialize_retail_saved_profile_records(
    NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::native_exception;
        failure.operation = "171B10 Main lifetime/thread";
        return RetailStartupGate::native_failure;
    }
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
        auto records = std::make_shared<NativeRetailSavedProfileRecords>();
        if (!records->initialize_records_from_171b10(main, failure)) {
            return RetailStartupGate::native_failure;
        }
        main.saved_profile_records = std::move(records);
        return RetailStartupGate::ready;
    } catch (const detail::NativeMainReentry&) {
        throw;
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
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::native_exception;
        failure.operation = "246070 Main lifetime/thread";
        return false;
    }
    if (!main.saved_profile_records) {
        main.saved_profile_records = std::make_shared<NativeRetailSavedProfileRecords>();
    }
    return main.saved_profile_records->reset_saved_profile_children_246070(main, failure);
}

} // namespace initial_reset_required

} // namespace mka::retail
