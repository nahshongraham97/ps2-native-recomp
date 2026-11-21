#include "mka/retail_main.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool dispatch_retail_task_lists(NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        f={StartupError::game_session,"Main12E4E0 owner thread/lifetime"};
        return false;
    }
    const auto session=main.startup.game_session;
    if(!session) {
        f={StartupError::game_session,"Main12E4E0 actual Session missing"};
        return false;
    }
    auto* domain=session->task_domain(f.game);
    if(!domain) {
        f.kind=StartupError::game_session;
        f.operation="Main12E4E0 persistent task domain";
        return false;
    }
    // The admitted source-born task classes have no script/task flags; the
    // second root is original BSS-empty and has no native12F250 producer.
    // That later producer and the full script/delta branches remain strong
    // missing implementations, never silently published into this route.
    // The existing domain performs real wait/body/cleanup/context effects.
    if(!domain->dispatch_first_root_zero_flags_from_12e4e0(f.game.task_domain)) {
        f.kind=StartupError::game_session;
        f.game.kind=RetailGameSessionError::task_domain;
        f.operation="Main12E4E0 source-born task dispatch";
        return false;
    }
    return true;
}
} // namespace mka::retail::main_binding
