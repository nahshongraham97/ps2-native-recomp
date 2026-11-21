#include "mka/retail_main.hpp"
#include "mka/retail_profile_main_state.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool reset_and_initialize_retail_profile(NativeRetailMainState& main,
    const NativeClassProfilePlan& plan, StartupFailure& failure) {
    failure = {};
    auto& startup = main.startup;
    if (!startup.memory_owners) {
        failure.kind = StartupError::memory_owners;
        failure.operation = "182670 actual memory group missing";
        return false;
    }
    if (!startup.profile_main) {
        auto profile = NativeRetailProfileMainState::create(
            *startup.memory_owners, failure.profile_main);
        if (!profile) {
            failure.kind = StartupError::profile_main;
            failure.operation = "182670 persistent profile owner construction";
            return false;
        }
        startup.profile_main = std::move(profile);
    }
    // Retain the owner before entering 182670: numeric publication and a
    // conditional retirement can have source-visible effects on failure.
    if (!startup.profile_main->apply_182670(
            plan, *startup.memory_owners, failure.profile_main)) {
        failure.kind = StartupError::profile_main;
        failure.operation = "182670 profile numeric and 16-slot reset";
        return false;
    }
    return true;
}
} // namespace mka::retail::main_binding
