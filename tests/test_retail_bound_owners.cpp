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
    state.auxiliary_words = {0xffffffffu, 0x12345678u};
    state.iteration = 73;
    f = {StartupError::native_exception, "prior fixture error"};
    fail_allocation = true;
    const auto reset_auxiliary = main_binding::reset_retail_auxiliary_words(state, f);
    fail_allocation = false;
    check(reset_auxiliary && f.kind == StartupError::none && state.auxiliary_words.first == 0 &&
        state.auxiliary_words.second == 0 && state.iteration == 73,
        "actual auxiliary reset clears exactly two values without allocation or unrelated reset");
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
