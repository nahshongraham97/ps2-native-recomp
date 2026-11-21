#include "mka/retail_main.hpp"
#include "mka/retail_initial_task.hpp"
#include "mka/retail_observer_list.hpp"
#include "mka/retail_task_domain.hpp"
#include "retail_main_control.hpp"
#include "native_retail_startup.hpp"

#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail::main_binding {
namespace {
bool fail(StartupFailure& f,StartupError kind,const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

bool create_retail_initial_mode_task(
    NativeRetailMainState& main,const RetailMainTaskRequest& request,
    StartupFailure& f) {
    f={};
    // Main 122994..A8 supplies this one concrete 12DCC0 request. The
    // 1229A8 delay slot supplies t0=0, its final argument.
    if(request.kind!=0x2001u || request.priority!=0x20u ||
       request.body!=RetailMainTaskBody::startup_18e100 ||
       request.argument!=0 || request.argument_size!=0)
        return fail(f,StartupError::game_session,
                    "1229A4 unsupported native initial task request");
    if(!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f,StartupError::game_session,
                    "1229A4 Main owner thread/lifetime unavailable");

    // Retain the one real Session and original source pool across all
    // allocation, cookie and ordered-publication effects.
    const auto session=main.startup.game_session;
    if(!session)
        return fail(f,StartupError::game_session,
                    "1229A4 retained game Session absent");
    auto* domain=session->task_domain(f.game);
    if(!domain)
        return fail(f,StartupError::game_session,
                    "1229A4 source task domain unavailable");
    auto* registry=domain->registry(f.game.task_domain);
    if(!registry) {
        f.game.kind=RetailGameSessionError::task_domain;
        return fail(f,StartupError::game_session,
                    "1229A4 source task first root unborn");
    }
    const auto pool=main.startup.game_pool;
    if(!pool)
        return fail(f,StartupError::game_session,
                    "1229A4 source 12DA70 native game pool absent");
    if(!pool->observe(f.game.task_domain.registry.pool)) {
        f.game.kind=RetailGameSessionError::task_domain;
        f.game.task_domain.kind=RetailTaskDomainError::registry;
        f.game.task_domain.registry.kind=RetailTaskRegistryError::game_pool;
        return fail(f,StartupError::game_session,
                    "1229A4 source game pool owner unavailable");
    }
    const auto registry_pool=registry->retained_pool(f.game.task_domain.registry);
    if(!registry_pool || registry_pool!=pool) {
        f.game.kind=RetailGameSessionError::task_domain;
        f.game.task_domain.kind=RetailTaskDomainError::registry;
        if(f.game.task_domain.registry.kind==RetailTaskRegistryError::none)
            f.game.task_domain.registry.kind=RetailTaskRegistryError::game_pool;
        return fail(f,StartupError::game_session,
                    "1229A4 task root and startup game pool identity differ");
    }

    // Allocate/authenticate the real 18E100 body and both native observer
    // identities before 130A50 consumes its one-shot/sequence cookie.
    RetailInitialTaskFailure body_failure;
    auto body=NativeRetailInitialTaskBody::create(main,session,body_failure);
    if(!body)
        return fail(f,body_failure.kind==RetailInitialTaskError::allocation
            ? StartupError::allocation : StartupError::game_session,
            body_failure.operation);
    std::shared_ptr<NativeRetailObserverList> e8,ec;
    try {
        e8=std::make_shared<NativeRetailObserverList>(pool);
        ec=std::make_shared<NativeRetailObserverList>(pool);
    } catch(const std::bad_alloc&) {
        return fail(f,StartupError::allocation,
                    "12EA90 initial task native observer allocation");
    } catch(const std::length_error&) {
        return fail(f,StartupError::allocation,
                    "12EA90 initial task native observer capacity");
    } catch(...) {
        return fail(f,StartupError::native_exception,
                    "12EA90 initial task native observer construction");
    }

    const auto cookie=domain->next_cookie_from_130a50(f.game.task_domain);
    if(!cookie) {
        f.game.kind=RetailGameSessionError::task_domain;
        return fail(f,StartupError::game_session,
                    "12EA90 actual 130A50 task identity producer");
    }
    // Source 12DCC0 calls 12EA90, installs 18E100 through 12F100 and then
    // publishes the priority link. The native frame is the typed body and
    // continuation; there is no 0x4000-byte guest stack or register image.
    try {
        auto task=registry->publish_retained_task_from_12ea90(
            request.kind,static_cast<std::int32_t>(request.priority),*cookie,
            std::move(body),std::move(e8),std::move(ec),
            f.game.task_domain.registry);
        if(!task) {
            f.game.kind=RetailGameSessionError::task_domain;
            f.game.task_domain.kind=RetailTaskDomainError::registry;
            return fail(f,StartupError::game_session,
                        "12DCC0 native task publication failed after 130A50");
        }
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(const std::bad_alloc&) {
        return fail(f,StartupError::allocation,
                    "12DCC0 allocation after task identity consumption");
    } catch(const std::length_error&) {
        return fail(f,StartupError::allocation,
                    "12DCC0 capacity after task identity consumption");
    } catch(...) {
        return fail(f,StartupError::native_exception,
                    "12DCC0 exception after task identity consumption");
    }
    return true;
}
} // namespace mka::retail::main_binding
