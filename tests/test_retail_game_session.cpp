#include "mka/retail_game_session.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
namespace {
std::atomic<long> live_allocations{0};
bool count_attempts = false;
long fail_at = -1, attempts = 0;
void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
}
void* operator new(std::size_t bytes) {
    if (count_attempts && attempts++ == fail_at) throw std::bad_alloc();
    if (void* p = std::malloc(bytes ? bytes : 1)) { ++live_allocations; return p; }
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { if (p) { --live_allocations; std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
int main() { try {
    RetailGameSessionFailure f;
    const auto initial_live = live_allocations.load();
    count_attempts = true;
    auto measured = NativeRetailGameSession::create(f);
    count_attempts = false;
    const auto creation_allocations = attempts;
    check(measured && creation_allocations > 0 && f.kind == RetailGameSessionError::none,
        "actual session creates complete native owners");
    measured.reset();
    check(live_allocations == initial_live, "complete destruction releases every session allocation");
    for (long cut = 0; cut < creation_allocations; ++cut) {
        attempts = 0; fail_at = cut; count_attempts = true;
        auto failed = NativeRetailGameSession::create(f);
        count_attempts = false; fail_at = -1;
        check(!failed && f.kind != RetailGameSessionError::none && live_allocations == initial_live,
            "every real object/control-block allocation failure is unpublished and fully released");
    }
    auto session = NativeRetailGameSession::create(f);
    auto retained = session;
    const std::weak_ptr<NativeRetailGameSession> root_lifetime = session;
    auto controls = session->controls(f);
    auto settings = session->settings(f);
    auto* random = session->random(f);
    auto* auxiliary = session->auxiliary_words(f);
    auto* initial_task = session->initial_task_state(f);
    auto* task_domain = session->task_domain(f);
    auto* game_values = session->game_record_values(f);
    check(game_values && game_values->word_185ee0_c3c8 == 1 &&
        game_values->word_152880_88 == 0 && !game_values->partial_reset,
        "same session births actual initialized game value separately from BSS zeros");
    RetailTaskDomainFailure task_failure;
    auto task_birth = task_domain ? task_domain->observe(task_failure) : std::nullopt;
    check(task_birth && task_birth->identity_counter == 0 &&
        task_birth->live_current_word == 5 && task_birth->live_partner_word == 5 &&
        !task_birth->task_roots_born,
        "same session owns source data and BSS task domain before real root birth");
    check(controls && settings && random && auxiliary && auxiliary->first == 0 && auxiliary->second == 0,
        "same session supplies actual controls/settings/BSS auxiliary and PRNG owners");
    RetailInitialTaskStateFailure initial_failure;
    auto initial_words=initial_task ? initial_task->observe(initial_failure) : std::nullopt;
    check(initial_words && initial_words->first_run==0 && initial_words->menu_index==0 &&
        initial_words->game_word_660094==0 && initial_words->cycle_word_6602ec==0,
        "same session owns four source BSS initial-task words");
    check(initial_task->store_menu_index_from_source(22,initial_failure) &&
        initial_task->select_menu_body_from_18db60(initial_failure)==1 &&
        initial_task->observe(initial_failure)->menu_index==2 &&
        initial_task->observe(initial_failure)->cycle_word_6602ec==1 &&
        initial_task->store_first_run_from_18e100(initial_failure) &&
        initial_task->store_start_words_from_18e100(initial_failure) &&
        initial_task->observe(initial_failure)->first_run==1 &&
        initial_task->observe(initial_failure)->menu_index==2 &&
        initial_task->observe(initial_failure)->cycle_word_6602ec==0 &&
        initial_task->observe(initial_failure)->game_word_660094==0x38,
        "source menu wrap/index publication and parent stores retain unrelated words");
    check(initial_task->store_menu_index_from_source(-1,initial_failure) &&
        !initial_task->select_menu_body_from_18db60(initial_failure) &&
        initial_failure.kind==RetailInitialTaskStateError::invalid_menu_index &&
        initial_task->observe(initial_failure)->menu_index==-1 &&
        initial_task->store_menu_index_from_source(2,initial_failure),
        "negative original table address is named and leaves persistent words intact");
    RetailGameControlsFailure cf;
    RetailMainSettingsFailure sf;
    RetailRandomFailure rf;
    check(settings->profile_selector(sf) == 13 && settings->stored_frame_rate(sf) == 60 &&
        settings->override_word(sf) == 0 && controls->selection_state_word(cf) == 0,
        "verified source initial data and BSS defaults");
    check(session->scalar_mode(f)==0,"actual retained scalar mode has source BSS birth");
    check(session->store_scalar_mode_from_source(-1,f)&&session->scalar_mode(f)==-1&&
          settings->profile_selector(sf)==13&&session->store_scalar_mode_from_source(17,f),
          "signed source scalar mode is separate from profile selector and history");
    check(settings->store_video_rate_from_220460(0, sf) &&
        settings->store_override_word_from_source(9, sf) &&
        controls->store_selection_flags_from_source(1, cf) &&
        settings->effective_frame_rate(sf) == 60 &&
        controls->clear_selection_state_from_1303f0(cf) &&
        settings->effective_frame_rate(sf) == 50,
        "live settings share same game-control word and full-word reset");
    check(controls->analog_input_inhibited(cf)==false&&
        controls->store_analog_input_inhibited_from_source(true,cf)&&
        controls->analog_input_inhibited(cf)==true&&controls->selection_state_word(cf)==0&&
        settings->effective_frame_rate(sf)==50,
        "live analog gate is distinct from timing/selection state");
    NativeRetailRandom expected;
    check(random->low16_range(56, rf) == expected.low16_range(56, rf) &&
        random->seed(0xabcdef01u, rf) && expected.seed(0xabcdef01u, rf),
        "pre-seed consumption and explicit source seed share same native PRNG");
    auxiliary->first = 0xabcdef12u; auxiliary->second = 0x12345678u;
    session.reset();
    check(!root_lifetime.expired() && retained->controls(f) == controls &&
        retained->settings(f) == settings && retained->auxiliary_words(f) == auxiliary &&
        retained->initial_task_state(f) == initial_task &&
        retained->controls(f)->analog_input_inhibited(cf)==true && retained->scalar_mode(f)==17 &&
        auxiliary->first == 0xabcdef12u && auxiliary->second == 0x12345678u &&
        initial_task->observe(initial_failure)->first_run==1 &&
        initial_task->observe(initial_failure)->menu_index==2,
        "retained session keeps exact owners and data across main borrower lifetimes");
    for (unsigned i = 0; i < 1500; ++i)
        check(retained->random(f)->next(rf) == expected.next(rf),
            "repeated borrowers never create implicit reseed or reset phase");
    bool wrong_thread = false, controls_wrong_thread = false;
    std::thread foreign([&] {
        RetailGameControlsFailure other_controls;
        controls_wrong_thread=!controls->analog_input_inhibited(other_controls)&&
            other_controls.kind==RetailGameControlsError::wrong_thread&&
            !controls->store_analog_input_inhibited_from_source(false,other_controls)&&
            other_controls.kind==RetailGameControlsError::wrong_thread;
        RetailGameSessionFailure other;
        RetailInitialTaskStateFailure other_initial;
        wrong_thread = !retained->validate_current_thread(other) &&
            other.kind == RetailGameSessionError::wrong_thread &&
            !retained->scalar_mode(other) && other.kind == RetailGameSessionError::wrong_thread &&
            !retained->store_scalar_mode_from_source(0,other) && other.kind == RetailGameSessionError::wrong_thread &&
            !retained->controls(other) && other.kind == RetailGameSessionError::wrong_thread &&
            !retained->settings(other) && other.kind == RetailGameSessionError::wrong_thread &&
            !retained->random(other) && other.kind == RetailGameSessionError::wrong_thread &&
            !retained->auxiliary_words(other) && other.kind == RetailGameSessionError::wrong_thread &&
            !retained->initial_task_state(other) && other.kind == RetailGameSessionError::wrong_thread &&
            !initial_task->observe(other_initial) &&
            other_initial.kind==RetailInitialTaskStateError::wrong_thread;
    });
    foreign.join();
    check(wrong_thread && controls_wrong_thread&&controls->analog_input_inhibited(cf)==true&&retained->scalar_mode(f)==17&&retained->validate_current_thread(f) && f.kind == RetailGameSessionError::none &&
        auxiliary->first == 0xabcdef12u && random->next(rf) == expected.next(rf),
        "cross-thread refusal leaves persistent state intact and next valid access clears failure");
    const std::weak_ptr<NativeRetailGameControls> controls_lifetime = controls;
    retained.reset();
    check(root_lifetime.expired() && settings->effective_frame_rate(sf) == 50,
        "retained settings keep their own actual controls after root retirement");
    controls.reset();
    check(!controls_lifetime.expired(), "settings hold real controls independently of original root");
    settings.reset();
    check(controls_lifetime.expired(), "last settings retirement releases controls");
    std::printf("PASS native persistent session: %ld creation allocation fault cuts, same-owner settings/PRNG/auxiliary, lifetime and thread failures; no game or endpoint\n",
        creation_allocations);
    return 0;
} catch (const std::exception& e) {
    count_attempts = false; fail_at = -1;
    std::fprintf(stderr, "FAIL native game session: %s\n", e.what()); return 1;
}}
