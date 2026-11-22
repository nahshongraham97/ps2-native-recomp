#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <new>
namespace mka::retail {
namespace {
std::optional<std::uint32_t> fail(RetailProfileMenuFailure& f,
 RetailProfileMenuError kind,const char* operation)noexcept {
 f.kind=kind;f.operation=operation;return {};
}
}
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState& main, std::uint32_t player, RetailProfileMenuFailure& f) {
    f = {};
    const auto lifetime = main.lifetime;
    if (!lifetime || !lifetime->active_on_owner_thread())
        return fail(f, RetailProfileMenuError::main_lifetime, "4EEE60 actual Main lifetime/thread");
    const auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(f.session))
        return fail(f, RetailProfileMenuError::session, "4EEE60 retained actual Session");
    const auto controls = session->controls(f.session);
    RetailGameControlsFailure control_failure;
    const auto flags = controls ? controls->selection_flags(control_failure) : std::nullopt;
    if (!flags)
        return fail(f, RetailProfileMenuError::controls, "4EEE60 actual selection byte");
    // LBU and the shift pair select ONLY bit2; it skips245BD0 entirely.
    if ((*flags & 4u) != 0) return 1u;
    if (player > 1u)
        return fail(f, RetailProfileMenuError::invalid_role,
                    "4EEE60/245BD0 active player outside source0/1 pointer selection");
    auto* records = session->saved_profile_records(f.session);
    RetailSavedProfileRecordsFailure records_failure;
    auto* active = records ? records->active_player(player, records_failure) : nullptr;
    if (!active)
        return fail(f, RetailProfileMenuError::session, "4EEE60 actual active saved child");
    RetailSavedProfileChildFailure child_failure;
    const auto customization = active->value.admitted_customization_flags(child_failure);
    if (!customization)
        return fail(f, RetailProfileMenuError::required_child,
                    "4EEE60 required complete admitted saved customization projection");
    if ((*customization & 1u) == 0) return 1u;
    // The current admitted BSS/reset values have bit0 clear. Populated data
    // cannot reach this continuation through the required/refusing importer.
    // Keep the real continuation connected to its existing live owners.
    if ((*flags & 1u) != 0) {
        auto* options = session->game_options(f.session);
        RetailGameOptionsFailure options_failure;
        const auto values = options ? options->observe(options_failure) : std::nullopt;
        if (!values)
            return fail(f, RetailProfileMenuError::session, "4EEE60/340BF0 actual options mode");
        if (values->live_mode == 1u) return 1u;
    }
    auto* domain = session->task_domain(f.session);
    RetailTaskDomainFailure domain_failure;
    // Source12ED10 null root returns not-found; do not require12F0D0 birth.
    auto* registry = domain ? domain->prior_registry_if_born(domain_failure) : nullptr;
    if (!domain || domain_failure.kind != RetailTaskDomainError::none)
        return fail(f, RetailProfileMenuError::task_domain, "4EEE60/12ED10 actual task root");
    if (!registry) return 0u;
    RetailTaskRegistryFailure task_failure;
    std::optional<std::shared_ptr<NativeRetailTaskObject>> found;
    try { found = registry->find_first_live(0x9012u, task_failure); }
    catch (const detail::NativeMainReentry&) { throw; }
    catch (const std::bad_alloc&) {
        return fail(f, RetailProfileMenuError::allocation, "4EEE60 native allocation during task cleanup");
    } catch (...) {
        return fail(f, RetailProfileMenuError::native_exception_after_effect, "4EEE60 native exception during task cleanup");
    }
    if (!found)
        return fail(f, RetailProfileMenuError::task_domain, "4EEE60/12ED10 actual live task lookup");
    // Stale-link retirement may call native cleanup. Retain owner and admit
    // the actual authority before returning its result to the Main caller.
    if (!lifetime->active_on_owner_thread() || main.startup.game_session != session)
        return fail(f, RetailProfileMenuError::session, "4EEE60 Main/Session retired during task cleanup");
    return *found ? 1u : 0u;
}

}
