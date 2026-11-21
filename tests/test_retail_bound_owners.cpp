#include "mka/retail_main.hpp"
#include "mka/retail_dispatch_owners.hpp"
#include "mka/retail_record_owners.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_participants.hpp"
#include "mka/retail_input_assignments.hpp"
#include <thread>
#include "native_retail_startup.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main(int argc, char** argv) { try {
    check(argc == 2, "fixture needs its disposable content directory");
    mka::content::ContentVolumeFailure volume_failure;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), volume_failure);
    check(volume.has_value(), "actual native content root opened");
    NativeRetailStartupState startup(std::move(*volume), {});
    mka::input::ControllerInput samples;
    NativeRetailMainState state(startup, samples);
    check(&state.controller_samples == &samples && !state.mode_history && !state.audio_statistics,
        "main retains same real native sample store, does not manufacture readiness");
    StartupFailure f;
    check(!main_binding::read_retail_profile_selector(state, f) && f.kind == StartupError::game_session &&
        !main_binding::retail_timing_override_active(state, f) && f.kind == StartupError::game_session &&
        !main_binding::read_retail_frame_rate(state, f) && f.kind == StartupError::game_session &&
        !main_binding::seed_retail_startup_random_state(state, 7, f) && f.kind == StartupError::game_session &&
        !main_binding::reset_retail_auxiliary_words(state, f) && f.kind == StartupError::game_session,
        "missing persistent native owners fail explicitly, never default selector/rate/seed/reset");
    fail_allocation = true;
    const bool failed_session = initialize_native_retail_game_session(startup, f);
    fail_allocation = false;
    check(!failed_session && !startup.game_session && f.kind == StartupError::game_session,
        "failed session startup publishes no partial native owner");
    check(initialize_native_retail_game_session(startup, f) && startup.game_session,
        "startup retains one real persistent session");
    const auto session = startup.game_session;
    RetailGameSessionFailure game_failure;
    auto settings = session->settings(game_failure);
    auto controls = session->controls(game_failure);
    auto* random = session->random(game_failure);
    auto* auxiliary = session->auxiliary_words(game_failure);
    check(settings && controls && random && auxiliary &&
        main_binding::read_retail_profile_selector(state, f) == 13 &&
        main_binding::retail_timing_override_active(state, f) == false &&
        main_binding::read_retail_frame_rate(state, f) == 60,
        "main readers consume verified ELF/BSS scalar defaults through real session");
    RetailMainSettingsFailure settings_failure;
    RetailGameControlsFailure controls_failure;
    check(settings->store_profile_selector_from_1225d0(2, settings_failure) &&
        settings->store_video_rate_from_220460(0, settings_failure) &&
        settings->store_override_word_from_source(1, settings_failure) &&
        controls->store_selection_flags_from_source(1, controls_failure) &&
        main_binding::read_retail_profile_selector(state, f) == 2 &&
        main_binding::retail_timing_override_active(state, f) == true &&
        main_binding::read_retail_frame_rate(state, f) == 60,
        "main observes actual shared live settings and game-control low bit");
    check(controls->clear_selection_state_from_1303f0(controls_failure) &&
        main_binding::read_retail_frame_rate(state, f) == 50,
        "actual full-word source clear changes live effective rate");
    RetailRandomFailure random_failure;
    NativeRetailRandom reference_random;
    check(random->low16_range(56, random_failure) == reference_random.low16_range(56, random_failure) &&
        main_binding::seed_retail_startup_random_state(state, 0x12345678u, f) &&
        reference_random.seed(0x12345678u, random_failure),
        "pre-seed game draw and later explicit source seed share one phase");
    for (unsigned i = 0; i < 700; ++i)
        check(random->next(random_failure) == reference_random.next(random_failure),
            "bound seed uses source algorithm and retained consumed phase");
    *auxiliary = {0xffffffffu, 0x12345678u};
    state.iteration = 73;
    f = {StartupError::native_exception, "prior fixture error"};
    fail_allocation = true;
    const auto reset_auxiliary = main_binding::reset_retail_auxiliary_words(state, f);
    fail_allocation = false;
    check(reset_auxiliary && f.kind == StartupError::none && auxiliary->first == 0 &&
        auxiliary->second == 0 && state.iteration == 73,
        "actual auxiliary reset clears exactly two values without allocation or unrelated reset");
    auxiliary->first = 42;
    NativeRetailMainState reentry(startup, samples);
    fail_allocation = true;
    const bool reused_session = initialize_native_retail_game_session(startup, f);
    fail_allocation = false;
    check(reused_session && startup.game_session == session &&
        main_binding::read_retail_profile_selector(reentry, f) == 2 &&
        main_binding::read_retail_frame_rate(reentry, f) == 50 && auxiliary->first == 42 &&
        random->next(random_failure) == reference_random.next(random_failure),
        "new main control object does not reset/reseed persistent game data");
    check(!main_binding::read_retail_startup_seed_component(state, f) && f.kind == StartupError::crt,
        "native seed entropy requires real retained CRT clock");
    check(initialize_native_retail_crt(startup, f), "actual CPU native CRT constructed");
    check(!main_binding::read_retail_startup_seed_component(state, f) &&
        f.crt.kind == RetailCrtError::not_finalized, "seed counter does not bypass native CRT readiness");
    check(finalize_native_retail_pre_main(startup, f) &&
        main_binding::read_retail_startup_seed_component(state, f).has_value(),
        "seed entropy uses actual validated native counter without EE Count emulation");
    bool session_wrong_thread = false;
    std::thread session_worker([&] {
        StartupFailure other;
        session_wrong_thread = !initialize_native_retail_game_session(startup, other) &&
            other.game.kind == RetailGameSessionError::wrong_thread &&
            !main_binding::read_retail_profile_selector(state, other) &&
            other.game.kind == RetailGameSessionError::wrong_thread &&
            !main_binding::retail_timing_override_active(state, other) &&
            !main_binding::read_retail_frame_rate(state, other) &&
            !main_binding::seed_retail_startup_random_state(state, 9, other) &&
            !main_binding::reset_retail_auxiliary_words(state, other) &&
            !main_binding::read_retail_startup_seed_component(state, other) &&
            other.crt.kind == RetailCrtError::wrong_thread;
    });
    session_worker.join();
    check(session_wrong_thread, "all persistent main bindings refuse cross-thread mutation/read");
    check(startup.crt->shutdown(f.crt), "native CRT closed without endpoint");
    check(!main_binding::read_retail_startup_seed_component(state, f) && f.crt.kind == RetailCrtError::closed,
        "seed counter retains closed-clock failure");
    check(!main_binding::observe_retail_frame_role(state, 0, f) &&
        f.kind == StartupError::native_exception && !state.input_roles,
        "frame observer cannot manufacture missing retail role ownership");
    RetailInputRoleFailure role_failure;
    state.input_roles = NativeRetailInputRoles::create(role_failure);
    check(bool(state.input_roles), "actual role owner for bound observation");
    const auto role = main_binding::observe_retail_frame_role(state, 0, f);
    const auto row = main_binding::observe_retail_action_row(state, 0, 15, f);
    check(role && !role->initialized && !role->blocked && !role->assigned && row &&
        row->action.rising_mask == 0x8000 && row->rising == 0 && f.kind == StartupError::none,
        "bound observer reads actual live role and original action row");
    check(!main_binding::observe_retail_frame_role(state, 256, f) &&
        !main_binding::observe_retail_action_row(state, 0, 16, f),
        "invalid role is refused before enum narrowing; invalid action row is refused");
    bool off_thread = false;
    std::thread foreign([&] {
        StartupFailure other;
        off_thread = !main_binding::observe_retail_frame_role(state, 0, other) &&
            other.kind == StartupError::native_exception;
    });
    foreign.join();
    check(off_thread, "actual main role observation retains native thread failure");
    RetailParticipantFailure participant_failure;
    state.participants = NativeRetailParticipants::create_first_two(participant_failure);
    RetailInputAssignmentFailure assignment_failure;
    state.input_assignments = NativeRetailInputAssignments::create(
        state.input_roles, state.participants, assignment_failure);
    check(bool(state.input_assignments), "main fixture owns actual same role and participant domain");
    const auto first_participant = state.participants->first_two(0, participant_failure);
    check(state.input_assignments->bind_other_to_role_two(first_participant, assignment_failure),
        "actual source role-two assignment updates real role owner");
    const auto assigned_role = main_binding::observe_retail_frame_role(state, 2, f);
    check(assigned_role && assigned_role->initialized && assigned_role->assigned && !assigned_role->blocked,
        "production main observer sees real assignment and initialized source flag");
    check(state.input_assignments->detach(first_participant, assignment_failure), "actual source detach");
    const auto detached_role = main_binding::observe_retail_frame_role(state, 2, f);
    check(detached_role && !detached_role->initialized && !detached_role->assigned,
        "production main observer sees inverse link retirement and source flag clear");
    check(!main_binding::read_retail_top_mode(state, f) && f.kind == StartupError::native_exception,
        "uninitialized mode reader fails by name");
    check(!main_binding::enter_retail_mode_zero(state, f) && f.kind == StartupError::native_exception,
        "missing ordered reset cannot be replaced by implicit source success");
    check(main_binding::clear_retail_mode_stack(state, f) && state.mode_history, "original reset creates actual history owner");
    fail_allocation = true;
    const bool entered = main_binding::enter_retail_mode_zero(state, f);
    fail_allocation = false;
    check(!entered && f.kind == StartupError::allocation && !state.mode_history->contains(0),
        "production mode wrapper retains actual allocation failure");
    check(main_binding::enter_retail_mode_zero(state, f) && main_binding::read_retail_top_mode(state, f) == 0 &&
        state.mode_history->contains(0), "actual native zero entry and actual bound top read");
    RetailModeFailure mode_failure;
    check(state.mode_history->push(6, mode_failure) && main_binding::read_retail_top_mode(state, f) == 6,
        "1830A0 read uses live history top");
    check(main_binding::clear_retail_mode_stack(state, f) && main_binding::read_retail_top_mode(state, f) == 0 &&
        !state.mode_history->contains(6), "later source reset clears same main-owned history");
    fail_allocation = true;
    const bool audio = main_binding::construct_retail_audio_statistics(state, f);
    fail_allocation = false;
    check(!audio && !state.audio_statistics && f.kind == StartupError::allocation,
        "actual audio constructor wrapper does not publish failed owner");
    check(main_binding::construct_retail_audio_statistics(state, f) && state.audio_statistics,
        "actual original audio-statistics constructor bound");
    const auto* exact_audio = state.audio_statistics.get();
    check(!main_binding::construct_retail_audio_statistics(state, f) && f.kind == StartupError::already_initialized &&
        state.audio_statistics.get() == exact_audio, "duplicate audio owner cannot reset actual statistics");
    check(main_binding::construct_retail_dispatch_object(state, f) &&
        main_binding::construct_retail_record_owners(state, f) &&
        main_binding::construct_retail_aux_dispatch_object(state, f), "real production constructor wrappers publish native owners");
    check(state.dispatch_object != state.auxiliary_dispatch_object &&
        !state.dispatch_object->configured() && !state.auxiliary_dispatch_object->configured(),
        "distinct prefixes are not falsely registered message handlers");
    const auto* exact_records = state.record_owners.get();
    check(!main_binding::construct_retail_record_owners(state, f) && f.kind == StartupError::already_initialized &&
        state.record_owners.get() == exact_records, "duplicate request owner cannot replace live identity");
    fail_allocation = true;
    const bool history_created = main_binding::initialize_retail_action_service(state, f);
    fail_allocation = false;
    check(!history_created && f.kind == StartupError::allocation && !state.action_history,
        "actual journal allocation failure leaves main without a published history");
    check(main_binding::initialize_retail_action_service(state, f) && f.kind == StartupError::none && state.action_history,
        "actual action initializer retries with cleared failure and owns real records");
    auto* exact_history = state.action_history.get();
    check(exact_history->journals[0].cursor == -1 && exact_history->journals[0].records[0].action == -1 &&
        exact_history->recent[0].cursor == 0 && exact_history->recent[0].canonical[0] == -1,
        "production initializer applies source stores");
    exact_history->journals[0].cursor = 7;
    auto& history_record = exact_history->journals[0].records[3];
    history_record.action = 4; history_record.frame = 66; history_record.label = "owned event label";
    history_record.context = 19; history_record.canonical = 8;
    exact_history->recent[0].cursor = 3; exact_history->recent[0].canonical[1] = 9;
    f = {StartupError::native_exception, "prior fixture error"};
    fail_allocation = true;
    const bool history_reset = main_binding::initialize_retail_action_service(state, f);
    fail_allocation = false;
    check(history_reset && f.kind == StartupError::none && state.action_history.get() == exact_history &&
        exact_history->journals[0].cursor == 7 && history_record.action == -1 && history_record.frame == -1 &&
        history_record.label.empty() && history_record.context == 19 && history_record.canonical == 8 &&
        exact_history->recent[0].cursor == 0 && exact_history->recent[0].canonical[1] == -1,
        "source repeat initialization is selective, allocation-free, and preserves actual owner identity");
    std::puts("PASS actual native main constructor/mode bindings and allocation failures; no game or endpoint");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL bound owners: %s\n", e.what()); return 1;
}}
