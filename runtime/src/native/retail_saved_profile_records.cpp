#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <bit>
#include <new>

namespace mka::retail {
namespace {
bool fail(RetailSavedProfileRecordsFailure& f, RetailSavedProfileRecordsError kind,
          const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return false;
}
struct Busy {
    bool& active;
    explicit Busy(bool& value) noexcept : active(value) { active = true; }
    ~Busy() { active = false; }
};
} // namespace

bool NativeRetailSavedProfileRecords::access(RetailSavedProfileRecordsFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != owner_thread_)
        return fail(f, RetailSavedProfileRecordsError::wrong_thread, "saved profile Session owner thread");
    return true;
}
bool NativeRetailSavedProfileRecords::admit(NativeRetailMainState& main,
    RetailSavedProfileRecordsFailure& f) const noexcept {
    if (!access(f)) return false;
    if (!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f, RetailSavedProfileRecordsError::main_lifetime, "saved profile actual Main lifetime");
    if (main.startup.game_session.get() != session_)
        return fail(f, RetailSavedProfileRecordsError::foreign_session, "saved profile actual Main Session identity");
    if (active_call_)
        return fail(f, RetailSavedProfileRecordsError::reentrant, "saved profile recursive initializer/reset");
    if (failed_after_effect_)
        return fail(f, RetailSavedProfileRecordsError::prior_partial_effect, "saved profile prior partial native effect");
    return true;
}
bool NativeRetailSavedProfileRecords::after_effect_failure(RetailSavedProfileRecordsFailure& f,
    RetailSavedProfileRecordsError kind, const char* operation) noexcept {
    failed_after_effect_ = true;
    return fail(f, kind, operation);
}
NativeRetailSavedCardSlot* NativeRetailSavedProfileRecords::card_slot(
    std::uint32_t index, RetailSavedProfileRecordsFailure& f) noexcept {
    if (!access(f)) return nullptr;
    if (index >= card_slots_.size()) {
        fail(f, RetailSavedProfileRecordsError::invalid_slot, "retail logical card slot");
        return nullptr;
    }
    return &card_slots_[index];
}
NativeRetailActivePlayerProfile* NativeRetailSavedProfileRecords::active_player(
    std::uint32_t index, RetailSavedProfileRecordsFailure& f) noexcept {
    if (!access(f)) return nullptr;
    if (index >= active_players_.size()) {
        fail(f, RetailSavedProfileRecordsError::invalid_player, "retail active profile player");
        return nullptr;
    }
    return &active_players_[index];
}
std::uint32_t* NativeRetailSavedProfileRecords::working_control_word(
    RetailSavedProfileRecordsFailure& f) noexcept {
    if (!access(f)) return nullptr;
    return &working_control_word_;
}

bool NativeRetailSavedProfileRecords::initialize_from_171b10(NativeRetailMainState& main,
    RetailSavedProfileRecordsFailure& f) {
    if (!admit(main, f)) return false;
    // Retain the actual owner before Busy. Menu callouts may release every
    // external Session reference; its records and guard must survive unwind.
    const auto retained = main.startup.game_session;
    const auto lifetime = main.lifetime;
    if (!main.saved_profile_storage || !main.saved_profile_storage->validate(f.storage))
        return fail(f, RetailSavedProfileRecordsError::storage,
                    "171B10 existing actual native save storage before profile mutation");
    Busy busy(active_call_);
    try {
        for (auto& parent : card_slots_) {
            parent.status_word = -1;
            for (auto& child : parent.children) {
                child.in_use_word = 0; // 171BA8, before the child call.
                if (!child.value.reset_from_245cb0(std::bit_cast<std::int32_t>(main.post_overlay_state),
                                                  working_control_word_, f.child))
                    return after_effect_failure(f, RetailSavedProfileRecordsError::child, f.child.operation);
            }
            parent.flag_byte_12 = 0; // 171BC4 call delay, before2432A0.
            std::array<std::array<RetailProfileAggregateChildView, 8>, 2> children;
            std::array<RetailProfileAggregateParentView, 2> parents;
            for (std::size_t index = 0; index != card_slots_.size(); ++index) {
                const auto& source = card_slots_[index];
                for (std::size_t child = 0; child != source.children.size(); ++child) {
                    const auto view = source.children[child].value.aggregate_view(f.child);
                    if (!view)
                        return after_effect_failure(f, RetailSavedProfileRecordsError::child, f.child.operation);
                    children[index][child] = *view;
                }
                parents[index] = {source.status_word, source.version_word, children[index]};
            }
            // Aggregate lives on the same Session, and1888C0 may perform a
            // legitimate Main transfer. Never replace this with a data-only call.
            if (!refresh_retail_profile_aggregate_from_2432a0(main, parents, f.aggregate))
                return after_effect_failure(f, RetailSavedProfileRecordsError::aggregate, f.aggregate.operation);
            if (!lifetime->active_on_owner_thread())
                return after_effect_failure(f, RetailSavedProfileRecordsError::main_lifetime,
                                             "171B10 Main retired during profile menu");
            if (main.startup.game_session != retained)
                return after_effect_failure(f, RetailSavedProfileRecordsError::foreign_session,
                                             "171B10 Session changed during profile menu");
            parent.preferences = retail_profile_preference_defaults_from_2955d0();
        }
        // 171BF4 then171BFC, only after both aggregate/menu/preference calls.
        card_slots_[0].status_word = 1;
        card_slots_[1].status_word = 1;
        return true;
    } catch (const detail::NativeMainReentry&) {
        throw; // Source control transfer retains the already-published prefix.
    } catch (const std::bad_alloc&) {
        return after_effect_failure(f, RetailSavedProfileRecordsError::allocation,
                                    "171B10 native allocation after profile effect");
    } catch (...) {
        return after_effect_failure(f, RetailSavedProfileRecordsError::native_exception,
                                    "171B10 native exception after profile effect");
    }
}

bool NativeRetailSavedProfileRecords::reset_first_two_from_246070(NativeRetailMainState& main,
    RetailSavedProfileRecordsFailure& f) {
    if (!admit(main, f)) return false;
    const auto retained = main.startup.game_session;
    Busy busy(active_call_);
    bool changed = false;
    auto error = [&](RetailSavedProfileRecordsError kind, const char* operation) {
        if (changed) failed_after_effect_ = true;
        return fail(f, kind, operation);
    };
    try {
        for (std::uint32_t player = 0; player != active_players_.size(); ++player) {
            if (!main.input_assignments || !main.input_roles || !main.participants)
                return error(RetailSavedProfileRecordsError::missing_input_owner,
                             "245DD0 actual input assignment/role/participant owners");
            if (!main.input_assignments->validate_domain(*main.input_roles, *main.participants, f.assignment))
                return error(RetailSavedProfileRecordsError::input_assignment, f.assignment.operation);
            // Capture281830 FIRST. Empty with no error is the source null case.
            auto captured = main.input_assignments->role_control_from_281830(player, f.assignment);
            if (!captured && f.assignment.kind != RetailInputAssignmentError::none)
                return error(RetailSavedProfileRecordsError::input_assignment, f.assignment.operation);
            const auto participant = main.participants->first_two(player, f.participant);
            if (!participant)
                return error(RetailSavedProfileRecordsError::participant, f.participant.operation);
            auto& active = active_players_[player];
            const auto slot = active.card_slot, index = active.profile_index;
            if (slot >= 0 && slot < 2 && index >= 0 && index < 8) {
                changed = true;
                card_slots_[static_cast<std::size_t>(slot)].children[static_cast<std::size_t>(index)].in_use_word = 0;
            }
            // 245F08 precedes245F14. The captured capability remains attached to
            // its original role even if a later source operation rebinds input.
            if (captured) {
                changed = true;
                if (!captured->clear_from_245dd0(f.role))
                    return error(RetailSavedProfileRecordsError::input_role, "245DD0 captured actual role control clear");
            }
            changed = true;
            if (!active.value.reset_child_body_from_245dd0(
                    std::bit_cast<std::int32_t>(main.post_overlay_state), working_control_word_, f.child))
                return error(RetailSavedProfileRecordsError::child, f.child.operation);
            active.status_word = 0;
            active.card_slot = -1;
            active.profile_index = -1; // 24602C call delay, before2817B0.
            if (!main.input_assignments->reset_profile_controls_from_2817b0(participant, f.assignment))
                return error(RetailSavedProfileRecordsError::input_assignment, f.assignment.operation);
            active.player_status_word = 8;
        }
        return true;
    } catch (const detail::NativeMainReentry&) {
        throw;
    } catch (const std::bad_alloc&) {
        return error(RetailSavedProfileRecordsError::allocation, "246070 native allocation after profile effect");
    } catch (...) {
        return error(RetailSavedProfileRecordsError::native_exception, "246070 native exception after profile effect");
    }
}
} // namespace mka::retail
