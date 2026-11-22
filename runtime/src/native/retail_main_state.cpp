#include "mka/retail_main.hpp"
#include "mka/retail_game_resource_registry.hpp"
#include "mka/retail_game_info_resources.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_sound_runtime.hpp"
#include "native_retail_startup.hpp"
#include <cstdio>
#include <exception>

namespace mka::retail {
NativeRetailMainState::NativeRetailMainState(NativeRetailStartupState& owner,
    input::ControllerInput& samples)
    : lifetime(std::make_shared<NativeRetailMainLifetime>()),
      startup(owner), controller_samples(samples) {}
NativeRetailMainState::~NativeRetailMainState() {
    if (!game_sound_references.empty()) {
        std::fputs("native retail sound teardown requires game-reference retirement\n", stderr);
        std::terminate();
    }
    if (sound_runtime) {
        RetailSoundRuntimeFailure runtime_failure;
        if (!sound_runtime->shutdown(runtime_failure)) {
            std::fprintf(stderr, "native retail sound output teardown refused: %s (runtime=%d output=%d)\n",
                runtime_failure.operation, static_cast<int>(runtime_failure.kind),
                static_cast<int>(runtime_failure.output.kind));
            std::terminate();
        }
        sound_runtime.reset();
    }
    if (sound_bank_catalog) {
        if (playback && current_sound) {
            sound::PlaybackFailure playback_failure;
            const auto context = playback->observe_sound_context(*current_sound, playback_failure);
            if (!context || context->pending_preparations || context->programs || context->active_voices ||
                context->retired_groups || context->retired_voices || context->active_retired_voices) {
                std::fputs("native retail sound teardown requires actual live program/reference cleanup\n", stderr);
                std::terminate();
            }
        }
        RetailSoundCatalogFailure sound_failure;
        if (!sound_bank_catalog->shutdown(sound_failure)) {
            std::fprintf(stderr, "native retail sound-bank teardown refused: %s (catalog=%d jobs=%d)\n",
                sound_failure.operation, static_cast<int>(sound_failure.kind), static_cast<int>(sound_failure.job.kind));
            std::terminate();
        }
    }
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
