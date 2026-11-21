#include "mka/retail_main.hpp"
#include "mka/retail_platform.hpp"
#include "native_retail_startup.hpp"
#include <bit>

namespace mka::retail {
namespace {
using namespace main_binding;
struct NativeMainReentry {
    NativeRetailMainState* owner;
    std::uint32_t value;
};

constexpr std::array<RetailActionBinding, 16> action_bindings{{
    {0x0001, 0x1607c0, 0x62a008},
    {0x0002, 0x1604c0, 0x62a010},
    {0x0004, 0x160a10, 0x62a018},
    {0x0008, 0x160640, 0x62a020},
    {0x0010, 0x161110, 0x62a028},
    {0x0020, 0x160b90, 0x62a030},
    {0x0040, 0x160ef0, 0x62a038},
    {0x0080, 0x160d40, 0x62a040},
    {0x0100, 0x160100, 0x62a048},
    {0x0200, 0x15ff60, 0x62a058},
    {0x0400, 0x15fe10, 0x62a068},
    {0x0800, 0x160250, 0x62a078},
    {0x1000, 0x1616b0, 0x62a088},
    {0x2000, 0x1612c0, 0x62a090},
    {0x4000, 0x161560, 0x62a098},
    {0x8000, 0x161410, 0x62a0a0},
}};

bool construct_main_owners(NativeRetailMainState& m, StartupFailure& f) {
    if (!construct_retail_initial_aggregate(m, f)) return false;
    if (!construct_retail_controller_owner(m, f)) return false;
    if (!construct_retail_dispatch_object(m, f)) return false;
    if (!construct_retail_scene_records(m, f)) return false;
    if (!construct_retail_default_record(m, f)) return false;
    if (!construct_retail_aux_dispatch_object(m, f)) return false;
    if (!construct_retail_two_service_objects(m, f)) return false;
    m.initial_main_record = {0, 0, 1, 30, -1};
    if (!register_retail_initialized_main_record(m, f)) return false;
    if (!register_retail_existing_service_cleanup(m, f)) return false;
    if (!construct_retail_key_action_records(m, f)) return false;
    if (!construct_retail_first_paired_key_records(m, f)) return false;
    if (!construct_retail_empty_binding(m, f)) return false;
    if (!construct_retail_second_paired_key_records(m, f)) return false;
    if (!construct_retail_first_handler_records(m, f)) return false;
    if (!construct_retail_second_handler_records(m, f)) return false;
    return true;
}

void initialize_frame_timing(NativeRetailMainState& m) {
    if (!retail_timing_override_active(m) && read_retail_frame_rate(m) == 50) {
        m.timing_factors = {
            std::bit_cast<float>(0x3f8c378cu),
            std::bit_cast<float>(0x3f555555u),
            std::bit_cast<float>(0x3f99999au)
        };
    } else {
        m.timing_factors = {1.0f, 1.0f, 1.0f};
    }
    m.milliseconds_per_frame = 1000.0f / static_cast<float>(read_retail_frame_rate(m));
}

bool publish_rising_action_tasks(NativeRetailMainState& m, StartupFailure& f) {
    for (std::uint32_t role = 0; role != 3; ++role) {
        const auto frame_role = observe_retail_frame_role(m, role, f);
        if (!frame_role) return false;
        if (!frame_role->initialized || frame_role->blocked || !frame_role->assigned) continue;
        for (std::uint32_t row = 0; row != action_bindings.size(); ++row) {
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
    ++m.iteration;
    if (!retail_frame_processing_inhibited(m) && !retail_ordinary_counter_inhibited(m)) {
        ++m.ordinary_iteration;
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
        if (read_retail_top_mode(m) == 6) ++m.mode_six_intervals;
    }
    if (!retail_task_exists(m, 0x2065) && !retail_task_exists(m, 0x2066) &&
        retail_mode_dispatch_enabled(m) && !dispatch_retail_enabled_mode(m, f)) return false;
    if (!retail_frame_processing_inhibited(m) && !publish_rising_action_tasks(m, f)) return false;
    if (!service_retail_frame_input(m, f) || !dispatch_retail_task_lists(m, f) ||
        !pump_retail_middleware(m, f) || !pump_retail_current_sound(m, f)) return false;
    if (retail_render_enabled(m) && !render_retail_frame(m, f)) return false;
    return prune_retail_associations(m, f);
}

} // namespace

const std::array<RetailActionBinding, 16>& retail_default_action_bindings() noexcept {
    return action_bindings;
}

std::uint32_t retail_canonical_action_index(std::uint32_t mask, std::uint32_t row) noexcept {
    for (std::uint32_t index = 0; index != action_bindings.size(); ++index) {
        if (action_bindings[index].rising_mask == mask) return index;
    }
    return row;
}

NativeRetailMainState::NativeRetailMainState(NativeRetailStartupState& owner,
    NativeRetailPlatform& platform) noexcept
    : startup(owner), controller_samples(platform.controller_samples()) {}

[[noreturn]] void reenter_native_retail_main(NativeRetailMainState& m, std::uint32_t value) {
    throw NativeMainReentry{&m, value == 0 ? 1 : value};
}

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
    NativeRetailMainState m(startup, *startup.platform);
    if (!construct_main_owners(m, f) || !initialize_retail_memory_owners(startup, f)) return {};
    const auto selector = read_retail_profile_selector(m, f);
    if (!selector || !reset_and_initialize_retail_profile(m, select_native_class_profile(*selector), f) ||
        !start_retail_stopwatch(m, 3, f) || !initialize_retail_parsed_timing_setting(m, f) ||
        !initialize_retail_middleware_files(m, f) || !initialize_retail_active_overlay(m, f)) return {};
    // 212B30 returns an unused constant without a reached side effect.
    if (!initialize_retail_post_overlay_state(m, f)) return {};

    initialize_frame_timing(m);
    if (!clear_retail_mode_stack(m, f) || !enter_retail_mode_zero(m, f) ||
        !initialize_retail_game_records(m, f) || !initialize_retail_loaded_sections(m, f) ||
        !initialize_retail_random_state(m, f) || !initialize_retail_action_service(m, f)) return {};

    auto gate = initialize_retail_timer_and_viewport(m, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready || !initialize_retail_graphics_stage(m, f) ||
        !initialize_retail_controller_stage(m, f)) return {};
    gate = initialize_retail_pad_service(m, 1, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready) return {};
    gate = initialize_retail_game_scene_records(m, f);
    if (gate == RetailStartupGate::source_refused) return 0;
    if (gate != RetailStartupGate::ready) return {};

    const auto boot_word = read_retail_audio_boot_word(m);
    const std::array audio_boot_words{boot_word, boot_word, boot_word, boot_word, boot_word};
    if (!initialize_retail_audio_stage(m, audio_boot_words, f) || !initialize_retail_audio_configuration(m, f) ||
        !initialize_retail_current_sound(m, f) || !initialize_retail_mode_objects(m, f) ||
        !initialize_retail_scene_service(m, f) || !initialize_retail_mode_service(m, f) ||
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
