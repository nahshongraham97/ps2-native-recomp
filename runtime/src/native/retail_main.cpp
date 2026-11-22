#include "mka/retail_main.hpp"
#include "mka/retail_platform.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <bit>

namespace mka::retail {
namespace {
using namespace main_binding;
using detail::NativeMainReentry;

bool construct_main_owners(NativeRetailMainState& m, StartupFailure& f) {
    if (!construct_retail_initial_aggregate(m, f)) return false;
    if (!construct_retail_middleware_work_group(m, f)) return false;
    if (!construct_retail_dispatch_object(m, f)) return false;
    if (!construct_retail_record_owners(m, f)) return false;
    if (!construct_retail_audio_statistics(m, f)) return false;
    if (!construct_retail_aux_dispatch_object(m, f)) return false;
    if (!construct_retail_two_service_objects(m, f)) return false;
    // 648A40/648A90 registered USB module bookkeeping and its guest cleanup.
    // The native platform already owns the keyboard queue and retires it with
    // the host input lifetime; no module records or fake registrations remain.
    if (!construct_retail_key_action_records(m, f)) return false;
    if (!construct_retail_first_paired_key_records(m, f)) return false;
    if (!construct_retail_empty_binding(m, f)) return false;
    if (!construct_retail_second_paired_key_records(m, f)) return false;
    if (!construct_retail_first_handler_records(m, f)) return false;
    if (!construct_retail_second_handler_records(m, f)) return false;
    return true;
}

bool initialize_frame_timing(NativeRetailMainState& m, StartupFailure& f) {
    if (!refresh_retail_timing_factors_122ad0(m, f)) return false;
    const auto rate = read_retail_frame_rate(m, f);
    if (!rate) return false;
    if (*rate <= 0) {
        f = {StartupError::main_timing, "retail frame rate must be positive"};
        return false;
    }
    m.milliseconds_per_frame = 1000.0f / static_cast<float>(*rate);
    return true;
}

bool publish_rising_action_tasks(NativeRetailMainState& m, StartupFailure& f) {
    for (std::uint32_t role = 0; role != 3; ++role) {
        const auto frame_role = observe_retail_frame_role(m, role, f);
        if (!frame_role) return false;
        if (!frame_role->initialized || frame_role->blocked || !frame_role->assigned) continue;
        for (std::uint32_t row = 0; row != retail_default_action_bindings().size(); ++row) {
            const auto observed = observe_retail_action_row(m, role, row, f);
            if (!observed) return false;
            if (!(observed->rising & observed->action.rising_mask)) continue;
            if (!create_retail_action_task(m, {role, row, m.iteration, 0x3002, 6, 0x0c, observed->action}, f)) {
                return false;
            }
        }
    }
    return true;
}

bool run_frame(NativeRetailMainState& m, StartupFailure& f) {
    const auto frame_inhibited = retail_frame_processing_inhibited(m, f);
    if (!frame_inhibited) return false;
    ++m.iteration; // Original branch delay slot increments total even when inhibited.
    if (!*frame_inhibited) {
        const auto counter_inhibited = retail_ordinary_counter_inhibited(m, f);
        if (!counter_inhibited) return false;
        if (!*counter_inhibited) ++m.ordinary_iteration;
    }
    // Pump only work already queued at this frame boundary. Work admitted by
    // a callout cannot extend this batch past the sampled work count.
    const auto queued = m.startup.crt->observe().queued;
    if (!m.startup.crt->pump(queued, f.crt)) {
        f.kind = StartupError::crt;
        f.operation = "retail main frame CRT work";
        return false;
    }
    if (!retire_retail_deferred_storage(m, f) || !poll_retail_controller_roles(m, f)) return false;

    m.rising_input = m.accumulated_input & (m.accumulated_input ^ m.previous_input);
    m.previous_input = m.accumulated_input;
    m.accumulated_input = 0;
    if (!advance_retail_game_services(m, f)) return false;
    if (std::bit_cast<std::int32_t>(++m.mode_tick) > 59) {
        m.mode_tick = 0;
        ++m.sixty_step_intervals;
        const auto mode = read_retail_top_mode(m, f);
        if (!mode) return false;
        if (*mode == 6) ++m.mode_six_intervals;
    }
    const auto first_mode_task = retail_task_exists(m, 0x2065, f);
    if (!first_mode_task) return false;
    bool mode_task_present = *first_mode_task;
    if (!mode_task_present) {
        const auto second_mode_task = retail_task_exists(m, 0x2066, f);
        if (!second_mode_task) return false;
        mode_task_present = *second_mode_task;
    }
    if (!mode_task_present) {
        const auto mode_enabled = retail_mode_dispatch_enabled(m, f);
        if (!mode_enabled) return false;
        if (*mode_enabled && !dispatch_retail_enabled_mode(m, f)) return false;
    }
    const auto action_scan_inhibited = retail_frame_processing_inhibited(m, f);
    if (!action_scan_inhibited) return false;
    if (!*action_scan_inhibited && !publish_rising_action_tasks(m, f)) return false;
    if (!service_retail_frame_input(m, f) || !dispatch_retail_task_lists(m, f) ||
        !pump_retail_middleware(m, f) || !pump_retail_current_sound(m, f)) return false;
    const auto render_enabled = retail_render_enabled(m, f);
    if (!render_enabled) return false;
    if (*render_enabled && !render_retail_frame(m, f)) return false;
    return prune_retail_associations(m, f);
}

} // namespace

std::optional<std::int64_t> continue_native_retail_main(NativeRetailStartupState& startup, StartupFailure& f) {
    f = {};
    if (!startup.crt) {
        f.kind = StartupError::crt;
        f.operation = "retail main CRT owner missing";
        return {};
    }
    const auto crt_state = startup.crt->observe();
    if (crt_state.closed || !crt_state.finalized) {
        f = {StartupError::crt, "retail main CRT owner inactive"};
        f.crt.kind = crt_state.closed ? RetailCrtError::closed : RetailCrtError::not_finalized;
        return {};
    }
    if (!startup.platform) {
        f.kind = StartupError::platform;
        f.operation = "retail main platform owner missing";
        return {};
    }
    NativeRetailMainState m(startup, startup.platform->controller_samples());
    if (!construct_main_owners(m, f) || !initialize_retail_memory_owners(startup, f)) return {};
    const auto selector = read_retail_profile_selector(m, f);
    if (!selector || !reset_and_initialize_retail_profile(m, select_native_class_profile(*selector), f) ||
        !start_retail_stopwatch(m, 3, f) || !initialize_retail_parsed_timing_setting(m, f) ||
        !initialize_retail_middleware_files(m, f) || !initialize_retail_active_overlay(m, f)) return {};
    // 212B30 returns an unused constant without a reached side effect.
    if (!initialize_retail_post_overlay_state(m, f)) return {};

    if (!initialize_frame_timing(m, f)) return {};
    if (!clear_retail_mode_stack(m, f) || !enter_retail_mode_zero(m, f) ||
        !initialize_retail_game_records(m, f) || !initialize_retail_loaded_sections(m, f) ||
        !reset_retail_auxiliary_words(m, f) || !initialize_retail_action_service(m, f)) return {};

    auto gate = initialize_retail_timer_and_viewport(m, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready) return {};
    // 2E06E0 only created the private ten-slot asynchronous file transport.
    // The earlier native file initializer and following real Package owner
    // supply native open/read/seek/lifetime directly, with no slot-table cap.
    if (!initialize_retail_package_stage(m, f)) return {};
    gate = initialize_retail_pad_service(m, 1, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready) return {};
    // Original171B10 first calls225000 and returns before profile mutations
    // on service failure. Native storage replaces that console transport.
    if (!initialize_retail_saved_profile_storage(m, f)) return {};
    gate = initialize_retail_saved_profile_records(m, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready) return {};

    const auto boot_word = read_retail_online_boot_word(m);
    const std::array online_boot_words{boot_word, boot_word, boot_word, boot_word, boot_word};
    if (!initialize_retail_online_stage(m, online_boot_words, f) || !initialize_retail_game_options(m, f) ||
        !initialize_retail_current_sound(m, f) || !initialize_retail_mode_objects(m, f) ||
        !initialize_retail_current_profile_preferences(m, f) || !initialize_retail_mode_service(m, f) ||
        !initialize_retail_section_service(m, f)) return {};

    gate = initialize_retail_first_scene(m, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready || !initialize_retail_post_scene_timing(m, f) ||
        !initialize_retail_post_scene_sections(m, f)) return {};

    m.pre_loop_source_flag = true;
    if (!initialize_retail_first_frame_graphics(m, f)) return {};
    const auto elapsed = stop_retail_stopwatch(m, 3, f);
    if (!elapsed) return {};
    const auto seed_component = read_retail_startup_seed_component(m, f);
    if (!seed_component) return {};
    const auto seed = (*elapsed & 0xffffu) | (static_cast<std::uint32_t>(*seed_component) << 16);
    if (!seed_retail_startup_random_state(m, seed, f) ||
        !create_retail_initial_mode_task(m, {0x2001, 0x20, RetailMainTaskBody::startup_18e100, 0, 0}, f)) {
        return {};
    }

    m.iteration = 0;
    m.ordinary_iteration = 0;
    // 1229C0 stores the initial zero returned by 1DDF40.
    m.reentry_value = 0;
    for (;;) {
        try {
            if (!run_frame(m, f)) return {};
        } catch (const NativeMainReentry& transfer) {
            if (transfer.owner != &m) throw;
            // The same 1229C0 store is reached on source nonlocal return.
            m.reentry_value = transfer.value;
        }
    }
}

} // namespace mka::retail
