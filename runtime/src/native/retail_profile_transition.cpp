#include "mka/retail_profile_transition.hpp"

#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <memory>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
std::optional<bool> fail(RetailProfileMenuFailure& f,
    RetailProfileMenuError kind, const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return std::nullopt;
}
} // namespace

std::optional<bool> run_retail_profile_transition_from_18a260(
    NativeRetailMainState& main, std::uint32_t selected_role,
    RetailProfileMenuFailure& f) {
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f, RetailProfileMenuError::main_lifetime,
            "18A260 Main owner thread/lifetime");
    if (selected_role > 1)
        return fail(f, RetailProfileMenuError::invalid_role,
            "18A260 source selected role outside 0/1");

    // The original getter precedes every participant read. A stale or absent
    // reference is source result zero, distinct from a native failure.
    const auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(f.session))
        return fail(f, RetailProfileMenuError::session,
            "18A260 retained game Session");
    auto* reference = session->live_scene_reference(f.session);
    if (!reference)
        return fail(f, RetailProfileMenuError::session,
            "18A260 live scene reference owner");
    const auto scene = reference->lookup_from_2f18f0(f.scene_reference);
    if (!scene)
        return fail(f, RetailProfileMenuError::scene_reference,
            "18A260 live scene/cookie lookup");
    if (!*scene) return false;

    if (!main.participants)
        return fail(f, RetailProfileMenuError::missing_participants,
            "18A260 source participant owner");
    const auto opposite_role = 1u - selected_role;
    const auto opposite = main.participants->first_two(opposite_role, f.participant);
    if (!opposite)
        return fail(f, RetailProfileMenuError::participant,
            "18A260 opposite participant identity");
    const auto opposite_before = opposite->observe(f.participant);
    if (!opposite_before)
        return fail(f, RetailProfileMenuError::participant,
            "18A260 opposite live status");
    if (opposite_before->game_state == 2) return false;

    // Source demands the concrete menu object only on its reached write arm.
    const auto profile_scene =
        std::dynamic_pointer_cast<NativeRetailProfileSceneObject>(*scene);
    if (!profile_scene || profile_scene->owner_session() != session.get()) {
        f.scene_object.kind = RetailProfileSceneObjectError::incompatible_object;
        f.scene_object.operation = "18A260 live object has no profile fields";
        return fail(f, RetailProfileMenuError::scene_object,
            "18A260 actual profile scene field owner");
    }
    const auto selected = main.participants->first_two(selected_role, f.participant);
    if (!selected)
        return fail(f, RetailProfileMenuError::participant,
            "18A260 selected participant identity");
    const auto selected_before = selected->observe(f.participant);
    if (!selected_before)
        return fail(f, RetailProfileMenuError::participant,
            "18A260 selected live source byte");

    // Original role lane writes (+2C/+24 or +58/+50), then common +60.
    if (!profile_scene->stage_from_18a260(selected_role,
            (selected_before->initial_game_words[0] & 1u) != 0,
            f.scene_object))
        return fail(f, RetailProfileMenuError::scene_object,
            "18A260 profile scene fields");
    if (!selected->set_game_state(1, f.participant))
        return fail(f, RetailProfileMenuError::participant,
            "18A260 selected participant status write");
    const auto mode = session->scalar_mode(f.session);
    if (!mode)
        return fail(f, RetailProfileMenuError::session,
            "18A260 live scalar mode after selected write");
    if (*mode == 4 && !opposite->set_game_state(0, f.participant))
        return fail(f, RetailProfileMenuError::participant,
            "18A260 opposite participant mode-four clear");

    // 18B0D0 re-reads the scene and may transfer control. Earlier writes
    // remain published on a failed child or a nonlocal Main transfer.
    try {
        if (!transition_retail_profile_from_18b0d0(
                main, selected_role, selected_role == 0 ? 0u : 9u, f)) {
            if (f.kind == RetailProfileMenuError::none)
                return fail(f, RetailProfileMenuError::required_child,
                    "18A260 required 18B0D0 transition child");
            return std::nullopt;
        }
    } catch (const detail::NativeMainReentry&) {
        throw;
    } catch (const std::bad_alloc&) {
        return fail(f, RetailProfileMenuError::allocation,
            "18A260 nested child allocation after effects");
    } catch (const std::length_error&) {
        return fail(f, RetailProfileMenuError::allocation,
            "18A260 nested child capacity after effects");
    } catch (...) {
        return fail(f, RetailProfileMenuError::native_exception_after_effect,
            "18A260 nested child exception after effects");
    }
    return true;
}

bool transition_retail_profile_from_18a260(
    NativeRetailMainState& main, std::uint32_t selected_role,
    RetailProfileMenuFailure& f) {
    return run_retail_profile_transition_from_18a260(main, selected_role, f).has_value();
}
} // namespace mka::retail
