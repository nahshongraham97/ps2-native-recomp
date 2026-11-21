#include "mka/retail_main.hpp"
#include "mka/retail_game_resource_registry.hpp"
#include "mka/retail_game_info_resources.hpp"
#include "native_retail_startup.hpp"
#include <cstdio>
#include <exception>

namespace mka::retail {
NativeRetailMainState::NativeRetailMainState(NativeRetailStartupState& owner,
    input::ControllerInput& samples)
    : lifetime(std::make_shared<NativeRetailMainLifetime>()),
      startup(owner), controller_samples(samples) {}
NativeRetailMainState::~NativeRetailMainState() {
    lifetime->retire();
    StartupFailure failure;
    if (!main_binding::shutdown_retail_middleware_files(*this, failure)) {
        std::fprintf(stderr, "native retail middleware teardown refused: %s (startup=%d work=%d)\n",
            failure.operation, static_cast<int>(failure.kind), static_cast<int>(failure.middleware.kind));
        // Existing work owners require explicit retirement of active children.
        // Never hide incomplete teardown by dropping live native work.
        std::terminate();
    }
}
} // namespace mka::retail
