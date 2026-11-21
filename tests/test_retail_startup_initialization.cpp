#include "mka/retail_main.hpp"
#include "mka/retail_profile_main_state.hpp"
#include "mka/retail_game_info_resources.hpp"
#include "mka/retail_input_roles.hpp"
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
        // Execute the actual185EE0 child on its real owners. The enclosing
        // complete1303F0/130350 sequence remains an unresolved production call.
        check(main_binding::clear_retail_mode_stack(main, failure) &&
              main_binding::enter_retail_mode_zero(main, failure), "real source mode owner");
        RetailGameResourceRegistryFailure registry_failure;
        main.resource_registry = NativeRetailGameResourceRegistry::create(registry_failure);
        RetailInputRoleFailure role_failure;
        main.input_roles = NativeRetailInputRoles::create(role_failure);
        RetailGameInfoResourceFailure value_failure;
        main.game_info_resources = NativeRetailGameInfoResources::create(startup.game_session, value_failure);
        check(main.resource_registry && main.input_roles && main.game_info_resources,
              "real registry, roles and typed game values");
        RetailGameSessionFailure session_failure;
        auto controls = startup.game_session->controls(session_failure);
        RetailGameControlsFailure control_failure;
        check(controls->store_analog_input_inhibited_from_source(true, control_failure),
              "actual shared flag set before full-word reset");
        auto* const domain = startup.game_session->task_domain(session_failure);
        check(domain && startup.game_record_reset->run_185ee0(*domain,
              *main.mode_history, *main.input_roles, *main.game_info_resources,
              *main.resource_registry, failure.game_record_reset),
              "real185EE0 ordered reset and four native registrations");
        auto names = main.resource_registry->observe(registry_failure);
        auto* values = startup.game_session->game_record_values(session_failure);
        check(names && names->global_names == 4 && values &&
              values->word_185ee0_c3c8 == 0 && values->word_16a9d0_2d4 == 2 &&
              controls->analog_input_inhibited(control_failure) == false,
              "same game values, initialized-word transition and input reset");
        // Exercise the real next section initializer independently. The
        // intervening full1303F0 controller remains unresolved in production.
        check(main_binding::initialize_retail_loaded_sections(main, failure),
              "production19C4C0 native section workspace");
        auto* const sections = startup.section_workspace.get();
        RetailSectionWorkspaceFailure section_failure;
        auto section_state = sections->observe(section_failure);
        check(section_state && section_state->record_count == 7 &&
              section_state->active_transient_records == 0,
              "real initial section records and empty active list");
        check(main_binding::initialize_retail_loaded_sections(main, failure) &&
              startup.section_workspace.get() == sections,
              "section reentry retains same initialized owner");
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
        std::puts("PASS real native startup profile, GAMER, post-overlay and section initialization; full entry remains open");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL native startup initialization: %s\n", error.what());
        return 1;
    }
}
