#include "mka/retail_main.hpp"
#include "mka/retail_section_workspace.hpp"
#include "native_retail_startup.hpp"

#include <utility>

namespace mka::retail::main_binding {

bool initialize_retail_loaded_sections(NativeRetailMainState& main,
                                       StartupFailure& failure) {
    failure = {};
    auto& startup = main.startup;
    if (!startup.game_session ||
        !startup.game_session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "19C4C0 native game owner unavailable";
        return false;
    }
    if (!startup.memory_owners) {
        failure.kind = StartupError::memory_owners;
        failure.operation = "19C4C0 Section owner group missing";
        return false;
    }
    if (startup.section_workspace) {
        if (startup.section_workspace->reset_for_reentry(
                *startup.memory_owners, failure.section_workspace)) return true;
    } else {
        auto candidate = NativeRetailSectionWorkspace::initialize(
            *startup.memory_owners, failure.section_workspace);
        if (candidate) {
            startup.section_workspace = std::move(candidate);
            return true;
        }
    }
    failure.kind = StartupError::section_workspace;
    failure.operation = "19C4C0 native Section workspace birth or reset";
    return false;
}

} // namespace mka::retail::main_binding
