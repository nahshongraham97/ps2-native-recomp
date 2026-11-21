#include "mka/retail_profile_task_retirement.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_task_domain.hpp"
#include "retail_main_control.hpp"
#include "native_retail_startup.hpp"

#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
bool fail(RetailProfileMenuFailure& f, RetailProfileMenuError kind,
    const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
} // namespace

bool retire_retail_profile_tasks_from_12edc0(
    NativeRetailMainState& main, std::uint32_t full_kind,
    RetailProfileMenuFailure& f) {
    f={};
    if(!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f,RetailProfileMenuError::main_lifetime,
            "12EDC0 Main owner thread/lifetime");
    // Keep the real Session and its single task root alive across cleanup.
    const auto session=main.startup.game_session;
    if(!session)
        return fail(f,RetailProfileMenuError::session,
            "12EDC0 retained game Session missing");
    auto* domain=session->task_domain(f.session);
    if(!domain)
        return fail(f,RetailProfileMenuError::session,
            "12EDC0 retained task domain missing");
    auto* registry=domain->registry(f.session.task_domain);
    if(!registry)
        return fail(f,RetailProfileMenuError::session,
            "12EDC0 source first task root unborn");
    try {
        if(!registry->retire_matching_from_12edc0(
            full_kind,f.session.task_domain.registry)) {
            f.session.kind=RetailGameSessionError::task_domain;
            f.session.task_domain.kind=RetailTaskDomainError::registry;
            return fail(f,RetailProfileMenuError::session,
                "12EDC0 source selective task cleanup");
        }
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(const std::bad_alloc&) {
        return fail(f,RetailProfileMenuError::allocation,
            "12EDC0 allocation after selective task effect");
    } catch(const std::length_error&) {
        return fail(f,RetailProfileMenuError::allocation,
            "12EDC0 capacity after selective task effect");
    } catch(...) {
        return fail(f,RetailProfileMenuError::native_exception_after_effect,
            "12EDC0 exception after selective task effect");
    }
    return true;
}
} // namespace mka::retail
