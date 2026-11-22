#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_initial_task.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_section_workspace.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "mka/retail_current_sound_required.hpp"
#include "mka/audio_bank_protocol.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace {
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Fixture { std::vector<unsigned> effects; unsigned prepared{}, context{}, frames{}; bool refuse_context{}; };
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
// Only the four still-unported resource children are fixture substitutes.
// The two bank hooks, Main initializer, callback and frame queue are production.
namespace mka::retail::initial_reset_required {
bool reset_saved_profile_children_246070(NativeRetailMainState& main, StartupFailure&) {
    fixture->effects.push_back(246070); inspect_prefix(main); return true;
}
bool reset_selected_resource_334700(NativeRetailMainState&, StartupFailure&) {
    fixture->effects.push_back(334700); return true;
}
bool clear_nonowning_references_18da30(NativeRetailMainState&, StartupFailure&) {
    fixture->effects.push_back(18); return true;
}
bool configure_selected_resource_zero_16fc70(NativeRetailMainState&, StartupFailure&) {
    fixture->effects.push_back(16); return true;
}
}
// These three effects remain unresolved in production. This fixture supplies
// an actual native empty playback context at explicit test rate/role count;
// it does not establish their original retail configuration/game semantics.
namespace mka::retail::current_sound_required {
bool prepare_game_sound_state_12d7e0(NativeRetailMainState&, StartupFailure&) { ++fixture->prepared; return true; }
bool create_playback_context_316320(NativeRetailMainState& main, sound::AudioBankSession&& session, StartupFailure&) {
    ++fixture->context;
    if (fixture->refuse_context) return false;
    sound::AudioProtocolFailure protocol_failure;
    auto protocol = sound::AudioBankProtocol::create(std::move(session), protocol_failure);
    check(bool(protocol), "real playback protocol view of catalog registry");
    sound::BankServiceFailure service_failure;
    auto service = sound::AudioBankService::create(std::move(*protocol), service_failure);
    check(bool(service), "real native bank service");
    sound::PlaybackFailure playback_failure;
    auto playback = sound::NativePlaybackOwner::create(std::move(*service), 48000, playback_failure);
    check(bool(playback), "real native playback owner with explicit test output rate");
    auto context = playback->create_sound_context({1}, playback_failure);
    check(bool(context), "real empty native context with explicit test role count");
    main.current_sound = std::move(context);
    main.playback = std::make_unique<sound::NativePlaybackOwner>(std::move(*playback));
    return true;
}
bool update_programs_after_bank_results_3152c0(NativeRetailMainState& main, StartupFailure&) {
    ++fixture->frames;
    sound::SoundContextUpdate update;
    sound::PlaybackFailure failure;
    return main.playback->update_ordinary_context(*main.current_sound, update, failure);
}
}
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
    check(argc == 2, "expected original native content root");
    mka::content::ContentVolumeFailure content_failure;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), content_failure);
    check(bool(volume), "actual native content volume");
    NativeRetailStartupState startup(std::move(*volume), {});
    StartupFailure failure;
    check(initialize_native_retail_game_session(startup, failure), "actual persistent game Session");
    check(initialize_native_retail_crt(startup, failure) && finalize_native_retail_pre_main(startup, failure), "actual native CRT owner");
    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);
    Fixture ordinary; fixture = &ordinary;
    check(!initial_reset_required::request_mode_one_banks_12a4f0(main, failure) &&
        !initial_reset_required::banks_pending_12abd0(main, failure), "uninitialized sound owner cannot report ready");
    install(main);
    check(main_binding::construct_retail_middleware_work_group(main, failure) &&
        main_binding::initialize_retail_middleware_files(main, failure), "actual native Main file initialization");
    check(main_binding::initialize_retail_current_sound(main, failure) && ordinary.prepared == 1 && ordinary.context == 1 &&
        main.sound_bank_catalog && main.current_sound_phase == RetailCurrentSoundPhase::ready, "production current-sound file/catalog path");
    RetailSoundCatalogFailure catalog_failure;
    auto shared_session = main.sound_bank_catalog->share_bank_session(catalog_failure);
    check(shared_session && shared_session->resource_domain() == main.playback->resource_domain(), "catalog and playback retain same native resource domain");
    auto child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    check(child->advance().kind == RetailInitialChildStep::Kind::wait, "actual reset suspends on real submitted bank work");
    check(ordinary.effects == std::vector<unsigned>{246070,334700}, "post-wait effects remain blocked");
    constexpr std::array<unsigned,4> ids{0,8,2,1}, slots{0,4,22,21}, flags{0,8,2,1};
    for (unsigned i = 0; i != ids.size(); ++i) {
        const auto row = main.sound_bank_catalog->observe(ids[i], catalog_failure);
        const auto selection = main.sound_bank_catalog->selection(slots[i], catalog_failure);
        check(row && row->pending && !row->bank && selection && selection->bank == ids[i] && selection->flags == flags[i],
            "source pending before work and independent slot/flag data");
    }
    check(initial_reset_required::request_mode_one_banks_12a4f0(main, failure), "pending repeated selection keeps same requests");
    for (unsigned i = 0; i != ids.size(); ++i) {
        check(main_binding::pump_retail_current_sound(main, failure), "actual Main bank frame pump");
        const auto row = main.sound_bank_catalog->observe(ids[i], catalog_failure);
        check(row && !row->pending && row->bank && row->last_completion == sound::RetailSoundBankJobStatus::completed,
            "real12B380 publishes owned handle then clears pending");
        sound::AudioBanksFailure bank_failure;
        check(shared_session->load_state(*row->bank, bank_failure) == sound::AudioBankLoadState::eager_prepared_source_deferred_allowed,
            "callback handle belongs to the shared actual native Session");
        const auto step = child->advance();
        check(step.kind == (i + 1 == ids.size() ? RetailInitialChildStep::Kind::completed : RetailInitialChildStep::Kind::wait),
            "reset only releases after last actual callback clears the shared pending scan");
    }
    check(ordinary.effects == std::vector<unsigned>{246070,334700,18,16} && main.sound_frame_counter == 4 && ordinary.frames == 4,
        "source post-wait effects happen once after four real ordered completions");
    RetailGameSessionFailure sf;
    RetailGameControlsFailure cf;
    check(startup.game_session->controls(sf)->analog_input_inhibited(cf) == false, "actual reset input resume occurs after completion");
    child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    // Source prefix assertions expect populated fields/latches; reinstall only
    // the independently checked live input/latch state for a second reset.
    ordinary.effects.clear();
    // The first-prefix inspector is specific to first reset. Its independent
    // checks already passed; second reset verifies actual retained sound owners.
    check(initial_reset_required::request_mode_one_banks_12a4f0(main, failure) &&
        initial_reset_required::banks_pending_12abd0(main, failure) == false,
        "completed selected banks remain live without spurious reload/pending work");
    bool wrong_thread = false;
    std::thread foreign([&] { StartupFailure f; wrong_thread = !initial_reset_required::banks_pending_12abd0(main, f) &&
        f.sound_catalog.kind == RetailSoundCatalogError::wrong_owner; }); foreign.join();
    check(wrong_thread, "foreign pending observer refused before state access");
    main.sound_frame_counter = 0xffffffffu;
    check(main_binding::pump_retail_current_sound(main, failure) && main.sound_frame_counter == 0, "original frame word has defined32-bit wrapping");
    check(main.sound_bank_catalog->shutdown(catalog_failure), "actual native bank retirement");
    sound::PlaybackFailure pf;
    check(main.playback->release_sound_context(*main.current_sound, pf), "idle native context release");
    main.current_sound.reset();
    check(main.playback->close_when_idle(pf), "canonical shared Session closes once");
    main.playback.reset();
    std::puts("PASS_CONNECTED_RETAIL_RESET_SOUND; production bank binding/four real callbacks/wait release; unrelated game/context fixtures; no game/endpoint");
    return 0;
} catch (const std::exception& error) { std::fprintf(stderr, "FAIL connected reset sound: %s\n", error.what()); return 1; }
