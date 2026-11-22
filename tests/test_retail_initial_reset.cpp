#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_initial_task.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_section_workspace.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    bool pending{}, fail_selected{}, transfer_selected{};
    unsigned scans{};
    std::vector<unsigned> calls;
};
Fixture* fixture{};

void install(NativeRetailMainState& main) {
    StartupFailure failure;
    check(initialize_retail_memory_owners(main.startup, failure), "native memory owners");
    check(main_binding::initialize_retail_loaded_sections(main, failure), "native section table0");
    RetailParticipantFailure participant_failure;
    RetailInputRoleFailure role_failure;
    RetailInputAssignmentFailure assignment_failure;
    main.participants = NativeRetailParticipants::create_first_two(participant_failure);
    main.input_roles = NativeRetailInputRoles::create(role_failure);
    check(main.participants && main.input_roles, "actual participant/role owners");
    check(main.input_roles->initialize_roles_from_281e00(role_failure), "actual game roles");
    main.input_assignments = NativeRetailInputAssignments::create(
        main.input_roles, main.participants, assignment_failure);
    check(bool(main.input_assignments), "actual association owner");
    check(main.input_assignments->increment_count_before_280c50_bind(assignment_failure), "nonzero initial count");
    for (unsigned i = 0; i != 2; ++i) {
        const auto participant = main.participants->first_two(i, participant_failure);
        check(participant->store_source_word_2c(0xffffffffu, participant_failure) &&
              participant->store_initial_game_word_14(0xabcdef01u, participant_failure) &&
              participant->set_game_state(7, participant_failure), "populated live participant fields");
    }
    RetailGameSessionFailure session_failure;
    const auto controls = main.startup.game_session->controls(session_failure);
    check(controls->store_selection_flags_from_source(0xffu, session_failure.controls), "live selection flags");
    check(controls->store_analog_input_inhibited_from_source(true, session_failure.controls), "input suspended before reset");
    auto* state = main.startup.game_session->initial_task_state(session_failure);
    RetailInitialTaskStateFailure state_failure;
    check(state->store_first_transition_latch(true, state_failure) &&
          state->store_second_transition_latch(true, state_failure), "both populated source latches");
    main.mode_history.emplace();
}

void inspect_prefix(NativeRetailMainState& main) {
    RetailParticipantFailure participant_failure;
    for (unsigned i = 0; i != 2; ++i) {
        const auto participant = main.participants->first_two(i, participant_failure);
        const auto state = participant->observe(participant_failure);
        check(state && state->source_word_2c == 0x40 && state->initial_game_words[0] == 0 &&
              state->game_state == 0 && state->assigned_role == -1,
              "full-word participant stores, state clears, preserved assignment");
    }
    RetailGameSessionFailure session_failure;
    const auto session = main.startup.game_session;
    const auto controls = session->controls(session_failure);
    check(controls->selection_flags(session_failure.controls) == 0xfbu &&
          session->scalar_mode(session_failure) == 12, "delay-slot bit2 clear precedes scalar12");
    RetailInitialTaskStateFailure state_failure;
    const auto state = session->initial_task_state(session_failure)->observe(state_failure);
    check(state && state->first_transition_latch == 0 && state->second_transition_latch == 0 &&
          main.mode_history->top() == 3, "same Session latches clear before history3");
    RetailInputAssignmentFailure assignment_failure;
    check(main.input_assignments->association_count(assignment_failure) == 0, "first association count clear before246070");
    RetailSectionWorkspaceFailure section_failure;
    const auto sections = main.startup.section_workspace->observe(section_failure);
    check(sections && sections->selected_table_index == 3, "actual native section selection3");
}
} // namespace

// Explicit fixture-only substitutes for six effects to isolate caller behavior.
// Production bank hooks are covered by test_retail_reset_sound.cpp.
// They verify caller ordering and waits, never stand in for accepted gameplay.
namespace mka::retail::initial_reset_required {
bool reset_saved_profile_children_246070(NativeRetailMainState& main, StartupFailure&) {
    fixture->calls.push_back(246070);
    inspect_prefix(main);
    return true;
}
bool reset_selected_resource_334700(NativeRetailMainState&, StartupFailure&) {
    fixture->calls.push_back(334700);
    if (fixture->transfer_selected) {
        fixture->transfer_selected = false;
        throw detail::NativeMainReentry{nullptr, 1};
    }
    return !fixture->fail_selected;
}
bool request_mode_one_banks_12a4f0(NativeRetailMainState& main, StartupFailure&) {
    fixture->calls.push_back(12);
    RetailGameSessionFailure failure;
    check(main.startup.game_session->game_records_values(failure)->word_660098 == 0xffffffffu,
          "actual165780(-1) store precedes bank requests");
    return true;
}
std::optional<bool> banks_pending_12abd0(NativeRetailMainState&, StartupFailure&) {
    ++fixture->scans;
    return fixture->pending;
}
bool clear_nonowning_references_18da30(NativeRetailMainState&, StartupFailure&) {
    fixture->calls.push_back(18);
    return true;
}
bool configure_selected_resource_zero_16fc70(NativeRetailMainState& main, StartupFailure&) {
    fixture->calls.push_back(16);
    RetailInputAssignmentFailure failure;
    // An intervening effect makes the final count reset independently visible.
    check(main.input_assignments->increment_count_before_280c50_bind(failure), "intervening actual count mutation");
    return true;
}
} // namespace mka::retail::initial_reset_required

// The initial parent shares its exception implementation with the menu caller.
// These child definitions are deliberately unreachable in this reset fixture.
namespace mka::retail::initial_task_required {
std::shared_ptr<NativeRetailInitialChild> begin_intro_18deb0(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected intro"); }
std::shared_ptr<NativeRetailInitialChild> begin_prompt_18dc10(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected prompt"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d450(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d870(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d8e0(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d700(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d800(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d660(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d770(NativeRetailMainState&, NativeRetailGameSession&) { throw std::runtime_error("unexpected menu"); }
void transition_1225d0(NativeRetailMainState&, std::uint32_t, RetailInitialTransitionBody) { throw std::runtime_error("unexpected transition"); }
} // namespace mka::retail::initial_task_required

int main(int argc, char** argv) try {
    check(argc == 2, "requires disposable empty content directory");
    mka::content::ContentVolumeFailure content_failure;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::u8path(argv[1]), content_failure);
    check(bool(volume), "native content volume");
    NativeRetailStartupState startup(std::move(*volume), {});
    StartupFailure startup_failure;
    check(initialize_native_retail_game_session(startup, startup_failure), "persistent native Session");
    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);
    install(main);
    Fixture ordinary;
    fixture = &ordinary;
    auto child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    check(child->advance().kind == RetailInitialChildStep::Kind::completed, "empty pending scan returns immediately");
    check(ordinary.calls == std::vector<unsigned>{246070,334700,12,18,16} && ordinary.scans == 1,
          "source effect order around empty pending scan");
    RetailGameSessionFailure session_failure;
    RetailGameControlsFailure control_failure;
    RetailInputAssignmentFailure assignment_failure;
    check(main.input_assignments->association_count(assignment_failure) == 0 &&
          startup.game_session->controls(session_failure)->analog_input_inhibited(control_failure) == false,
          "actual native input resumes before final count clear");
    check(child->advance().kind == RetailInitialChildStep::Kind::completed && ordinary.calls.size() == 5,
          "completed child never replays effects");

    Fixture pending;
    fixture = &pending;
    pending.pending = true;
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    for (unsigned i = 0; i != 4; ++i) {
        const auto step = child->advance();
        check(step.kind == RetailInitialChildStep::Kind::wait && step.duration == 1.0f,
              "retained source one-unit suspension");
    }
    check(pending.calls == std::vector<unsigned>{246070,334700,12}, "prefix once, post-wait effects deferred");
    pending.pending = false;
    check(child->advance().kind == RetailInitialChildStep::Kind::completed && pending.scans == 5 &&
          pending.calls == std::vector<unsigned>{246070,334700,12,18,16}, "resume scans same live pending owner");

    Fixture timeout;
    fixture = &timeout;
    timeout.pending = true;
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    for (unsigned i = 0; i != 1799; ++i)
        check(child->advance().kind == RetailInitialChildStep::Kind::wait, "source countdown before timeout");
    check(child->advance().kind == RetailInitialChildStep::Kind::completed && timeout.scans == 1800 &&
          timeout.calls == std::vector<unsigned>{246070,334700,12,18,16}, "exact source1799 waits, then continue");

    Fixture failure;
    fixture = &failure;
    failure.fail_selected = true;
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    for (unsigned i = 0; i != 2; ++i) {
        bool refused = false;
        try { child->advance(); } catch (const RetailInitialTaskException&) { refused = true; }
        check(refused, "required native failure blocks further effects");
    }
    check(failure.calls == std::vector<unsigned>{246070,334700}, "partial failure cannot replay or reach banks");

    Fixture transfer;
    fixture = &transfer;
    transfer.transfer_selected = true;
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    bool transferred = false;
    try { child->advance(); } catch (const detail::NativeMainReentry&) { transferred = true; }
    check(transferred && child->advance().kind == RetailInitialChildStep::Kind::completed &&
          transfer.calls == std::vector<unsigned>{246070,334700,334700,12,18,16},
          "known transfer preserves completed prefix and retries only the unfinished required child");

    Fixture guarded;
    fixture = &guarded;
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    bool foreign_thread_refused = false;
    std::thread worker([&] {
        try { child->advance(); } catch (const RetailInitialTaskException&) { foreign_thread_refused = true; }
    });
    worker.join();
    check(foreign_thread_refused && guarded.calls.empty(), "wrong thread before native mutations");
    check(child->advance().kind == RetailInitialChildStep::Kind::completed, "owner proceeds after rejected foreign call");
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    main.lifetime->retire();
    bool retired = false;
    try { child->advance(); } catch (const RetailInitialTaskException&) { retired = true; }
    check(retired && guarded.calls.size() == 5, "retired Main refuses before borrowed state access");
    std::puts("PASS native reset ordering, live fields, suspension,1799-wait timeout, failure and lifetime; six effects substituted in this caller fixture");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL native reset: %s\n", error.what());
    return 1;
}
