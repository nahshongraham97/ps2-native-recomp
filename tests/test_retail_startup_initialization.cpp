#include "mka/retail_main.hpp"
#include "mka/retail_profile_main_state.hpp"
#include "native_retail_startup.hpp"

#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace {
void check(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(operation);
}
}

int main(int argc, char** argv) {
    using namespace mka::retail;
    try {
        check(argc == 2, "requires original extracted GAMER module directory");
        mka::content::ContentVolumeFailure content_failure;
        auto volume = mka::content::NativeContentVolume::open(
            std::filesystem::u8path(argv[1]), content_failure);
        check(bool(volume), "original content volume");
        NativeRetailStartupState startup(std::move(*volume), {});
        StartupFailure failure;
        check(initialize_native_retail_game_session(startup, failure), "real game session");
        check(initialize_retail_memory_owners(startup, failure), "real startup memory group");
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup, samples);
        auto selector = main_binding::read_retail_profile_selector(main, failure);
        check(bool(selector), "actual persistent profile selector");
        const auto plan = select_native_class_profile(*selector);
        check(main_binding::reset_and_initialize_retail_profile(main, plan, failure),
              "production182670 profile initialization");
        check(main_binding::initialize_retail_active_overlay(main, failure),
              "production3DDA60 native GAMER initialization");
        main.post_overlay_state = 0x12345678;
        check(main_binding::initialize_retail_post_overlay_state(main, failure) &&
              main.post_overlay_state == 0, "production182E50 clears actual game value");
        auto* const profile = startup.profile_main.get();
        auto* const overlay = startup.active_overlay.get();
        RetailProfileMainFailure profile_failure;
        auto first = profile->observe(profile_failure);
        check(first && first->generation == 1 && first->active_wave_classes == 13 &&
              first->companion_present[0] == false && first->companion_present[1] == false &&
              first->companion_present[2] == false, "source initial companion slots absent");

        // Direct lifecycle check of the real later producer. Its production
        // callpoint in game-record initialization is still a separate task.
        check(profile->install_companions_311260(*overlay, *startup.memory_owners,
              profile_failure), "actual overlay capability creates real companion owners");
        auto installed = profile->observe(profile_failure);
        check(installed && installed->companion_present[0] == true &&
              installed->companion_present[1] == true && installed->companion_present[2] == true,
              "all three companion slots owned");
        RetailActiveOverlayFailure overlay_failure;
        auto prior_parent = overlay->retained_companion_parent(overlay_failure);
        check(!main_binding::initialize_retail_active_overlay(main, failure) &&
              failure.kind == StartupError::active_overlay &&
              startup.active_overlay.get() == overlay && prior_parent->observe().active,
              "live companions cannot be silently discarded by overlay reload");
        check(main_binding::reset_and_initialize_retail_profile(main, plan, failure) &&
              startup.profile_main.get() == profile, "same production profile owner resets all slots");
        auto reset = profile->observe(profile_failure);
        check(reset && reset->generation == 2 && reset->active_wave_classes == 13 &&
              reset->companion_present[0] == false && reset->companion_present[1] == false &&
              reset->companion_present[2] == false, "companion retirement and Wave reconstruction");
        check(main_binding::initialize_retail_active_overlay(main, failure) &&
              startup.active_overlay.get() == overlay && !prior_parent->observe().active,
              "actual overlay replaces retired parent after profile reset");
        std::puts("PASS real native startup profile, GAMER initializer and post-overlay state; full entry remains open");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL native startup initialization: %s\n", error.what());
        return 1;
    }
}
