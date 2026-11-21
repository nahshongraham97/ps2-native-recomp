#include "mka/retail_profile_menu.hpp"
#include "mka/retail_main.hpp"
#include "retail_main_control.hpp"

#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
constexpr std::string_view profile_select_path = "common/p_select/p_select";

bool fail(RetailProfileMenuFailure& f, RetailProfileMenuError kind,
    const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return false;
}

std::optional<std::uint32_t> status_at(NativeRetailMainState& main,
    std::uint32_t participant, RetailProfileMenuFailure& f) noexcept {
    if (!main.participants)
        return fail(f, RetailProfileMenuError::missing_participants,
            "1888C0 source participant owner"), std::nullopt;
    auto owner = main.participants->first_two(participant, f.participant);
    if (!owner)
        return fail(f, RetailProfileMenuError::participant,
            "1888C0 source participant identity"), std::nullopt;
    const auto observed = owner->observe(f.participant);
    if (!observed)
        return fail(f, RetailProfileMenuError::participant,
            "1888C0 live participant state"), std::nullopt;
    return observed->game_state;
}

bool child_failed(RetailProfileMenuFailure& f, const char* operation) noexcept {
    if (f.kind == RetailProfileMenuError::none)
        return fail(f, RetailProfileMenuError::required_child, operation);
    return false;
}
} // namespace

bool run_retail_profile_menu_from_1888c0(
    NativeRetailMainState& main, RetailProfileMenuFailure& f) {
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f, RetailProfileMenuError::main_lifetime,
            "1888C0 Main owner thread/lifetime");
    if (!main.mode_history)
        return fail(f, RetailProfileMenuError::missing_mode_history,
            "1888C0 source history owner");
    // 1888C8/CC: a0=4 in the call delay slot. The scalar mode is unrelated.
    if (!main.mode_history->contains(4)) return true;

    try {
        const auto scene = observe_retail_profile_live_scene_from_2f18f0(main, f);
        if (!scene) return child_failed(f, "1888DC live scene/cookie child");
        if (!*scene) return true;

        // 188900 then 188910 only if needed. The participant +8 words have
        // one real native owner; never snapshot both across effectful calls.
        const auto first = status_at(main, 0, f);
        if (!first) return false;
        if (*first != 1) {
            const auto second = status_at(main, 1, f);
            if (!second) return false;
            if (*second != 1) return true;
        }

        // 188924 sets a0 to the exact source literal for 2F1630.
        if (!invoke_retail_profile_select_from_2f1630(main, profile_select_path, f))
            return child_failed(f, "188920 profile-select action child");

        // These are fresh 188930 and 188968 loads. The action and first
        // transition may have changed the participant states.
        const auto after_action = status_at(main, 0, f);
        if (!after_action) return false;
        if (*after_action == 1) {
            if (!transition_retail_profile_from_18b0d0(main, 0, 0, f))
                return child_failed(f, "188940 first participant transition");
        } else if (*after_action == 2) {
            if (!transition_retail_profile_from_18a260(main, 0, f))
                return child_failed(f, "188958 first participant transition");
        }

        const auto after_first_transition = status_at(main, 1, f);
        if (!after_first_transition) return false;
        if (*after_first_transition == 1) {
            if (!transition_retail_profile_from_18b0d0(main, 1, 9, f))
                return child_failed(f, "188974 second participant transition");
        } else if (*after_first_transition == 2) {
            if (!transition_retail_profile_from_18a260(main, 1, f))
                return child_failed(f, "18898C second participant transition");
        }
        return true;
    } catch (const detail::NativeMainReentry&) {
        throw;
    } catch (const std::bad_alloc&) {
        return fail(f, RetailProfileMenuError::allocation,
            "1888C0 child allocation after possible effect");
    } catch (const std::length_error&) {
        return fail(f, RetailProfileMenuError::allocation,
            "1888C0 child capacity after possible effect");
    } catch (...) {
        return fail(f, RetailProfileMenuError::native_exception_after_effect,
            "1888C0 child exception after possible effect");
    }
}
} // namespace mka::retail
