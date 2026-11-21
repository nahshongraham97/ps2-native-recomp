#include "mka/retail_main.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
std::optional<bool> retail_task_exists(
    NativeRetailMainState& main,std::uint32_t kind,StartupFailure& f) {
    f={};
    // Keep the actual Session alive across effectful12ED10 retirement. This
    // must not become a metadata lookup or lazily create a different root.
    auto session=main.startup.game_session;
    if(!session) {
        f.kind=StartupError::game_session;
        f.operation="Main2807B0 retained native task Session missing";
        return {};
    }
    auto* domain=session->task_domain(f.game);
    if(!domain) {
        f.kind=StartupError::game_session;
        f.operation="Main2807B0 native task domain access";
        return {};
    }
    auto* registry=domain->registry(f.game.task_domain);
    if(!registry) {
        f.kind=StartupError::game_session;
        f.game.kind=RetailGameSessionError::task_domain;
        f.operation="Main2807B0 source task registry not available";
        return {};
    }
    // No catch/noexcept here: a real cleanup may reenter Main nonlocally.
    const auto found=registry->find_first_live(kind,f.game.task_domain.registry);
    if(!found) {
        f.kind=StartupError::game_session;
        f.game.kind=RetailGameSessionError::task_domain;
        f.game.task_domain.kind=RetailTaskDomainError::registry;
        f.operation="Main2807B0 effectful12ED10 task lookup";
        return {};
    }
    return static_cast<bool>(*found); // Engaged null is the original not-found.
}
} // namespace mka::retail::main_binding
