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
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace sound = mka::sound;
namespace {
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Fixture { std::vector<unsigned> effects; unsigned context{}, frames{}; bool refuse_context{}; };
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
// These two effects remain unresolved in production. This fixture supplies
// an actual native empty playback context at explicit test rate/role count;
// it does not establish their original retail configuration/game semantics.
namespace mka::retail::current_sound_required {
bool create_playback_context_316320(NativeRetailMainState& main, sound::AudioBankSession&& session, StartupFailure&) {
    check(main.game_sound_state_prepared && main.game_sound_references.empty() && main.sound_auxiliary_word == 0,
        "source12D7E0 game clears are visible before the context factory");
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
    check(argc == 3, "expected original native content root and new disposable negative root");
    mka::content::ContentVolumeFailure content_failure;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), content_failure);
    check(bool(volume), "actual native content volume");
    NativeRetailStartupState startup(std::move(*volume), {});
    StartupFailure failure;
    check(initialize_native_retail_game_session(startup, failure), "actual persistent game Session");
    check(initialize_native_retail_crt(startup, failure) && finalize_native_retail_pre_main(startup, failure), "actual native CRT owner");
    mka::input::ControllerInput samples;
    {
        NativeRetailMainState guarded(startup, samples);
        guarded.sound_auxiliary_word = 0xaabbccddu;
        guarded.game_sound_references.push_back({});
        check(!current_sound_required::prepare_game_sound_state_12d7e0(guarded, failure) &&
            guarded.game_sound_references.size() == 1 && !guarded.game_sound_state_prepared &&
            guarded.sound_auxiliary_word == 0xaabbccddu,
            "unexpected existing references are refused before native state is erased");
        guarded.game_sound_references.clear(); // Explicitly retire test-only default records.
        bool foreign_refused = false;
        std::thread foreign([&] { StartupFailure f;
            foreign_refused = !current_sound_required::prepare_game_sound_state_12d7e0(guarded, f) &&
                f.sound_catalog.kind == RetailSoundCatalogError::wrong_owner;
        });
        foreign.join();
        check(foreign_refused && !guarded.game_sound_state_prepared && guarded.sound_auxiliary_word == 0xaabbccddu,
            "foreign-thread preparation cannot change game fields");
    }
    NativeRetailMainState main(startup, samples);
    main.sound_auxiliary_word = 0xffffffffu;
    Fixture ordinary; fixture = &ordinary;
    check(!initial_reset_required::request_mode_one_banks_12a4f0(main, failure) &&
        !initial_reset_required::banks_pending_12abd0(main, failure), "uninitialized sound owner cannot report ready");
    install(main);
    check(main_binding::construct_retail_middleware_work_group(main, failure) &&
        main_binding::initialize_retail_middleware_files(main, failure), "actual native Main file initialization");
    check(main_binding::initialize_retail_current_sound(main, failure) && main.game_sound_state_prepared && ordinary.context == 1 &&
        main.sound_bank_catalog && main.current_sound_phase == RetailCurrentSoundPhase::ready, "production current-sound file/catalog path");
    check(!current_sound_required::prepare_game_sound_state_12d7e0(main, failure) &&
        main.current_sound_phase == RetailCurrentSoundPhase::ready && main.sound_auxiliary_word == 0,
        "repeated preparation cannot erase an initialized sound owner");
    RetailSoundCatalogFailure catalog_failure;
    auto shared_session = main.sound_bank_catalog->share_bank_session(catalog_failure);
    check(shared_session && shared_session->resource_domain() == main.playback->resource_domain(), "catalog and playback retain same native resource domain");
    auto child = initial_task_required::begin_reset_18d970(main, *startup.game_session);
    check(child->advance().kind == RetailInitialChildStep::Kind::wait, "actual reset suspends on real submitted bank work");
    check(ordinary.effects == std::vector<unsigned>{246070,334700}, "post-wait effects remain blocked");
    constexpr std::array<unsigned,4> ids{0,8,2,1}, slots{0,4,22,21}, flags{0,0,0,0};
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
    ordinary.effects.clear();
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
    // A valid sentinel-only native archive has no bank names. These actual
    // missing loads must clear source pending fields but still refuse the reset
    // wait as a named native error, never masquerade as successful completion.
    const std::filesystem::path negative_root(argv[2]);
    check(std::filesystem::create_directory(negative_root) &&
        std::filesystem::create_directory(negative_root / "sndsps2"), "new negative content root");
    {
        std::array<unsigned char,25> bytes{};
        constexpr std::array<std::uint32_t,6> words{2,1,13,0,0,25};
        for (std::size_t i = 0; i < words.size(); ++i)
            for (unsigned j = 0; j < 4; ++j) bytes[i*4+j] = static_cast<unsigned char>(words[i] >> (j*8));
        std::ofstream ms2(negative_root / "sndsps2" / "MSLASSET.MS2", std::ios::binary);
        ms2.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::ofstream ms4(negative_root / "sndsps2" / "MSLASSET.MS4", std::ios::binary);
        check(bool(ms2) && bool(ms4), "actual synthetic missing-bank files");
    }
    {
        auto volume = mka::content::NativeContentVolume::open(negative_root, content_failure);
        check(bool(volume), "negative native content volume");
        NativeRetailStartupState negative_startup(std::move(*volume), {});
        NativeRetailMainState rejected(negative_startup, samples);
        RetailFilePathFailure path_failure;
        rejected.file_paths = NativeRetailFilePaths::create(path_failure);
        Fixture reject; fixture = &reject; reject.refuse_context = true;
        check(!main_binding::initialize_retail_current_sound(rejected, failure) &&
            rejected.current_sound_phase == RetailCurrentSoundPhase::failed &&
            rejected.game_sound_state_prepared && rejected.game_sound_references.empty() &&
            rejected.sound_auxiliary_word == 0 &&
            !initial_reset_required::banks_pending_12abd0(rejected, failure),
            "unimplemented/failed required context cannot turn file-open into ready success");
    }
    {
        auto volume = mka::content::NativeContentVolume::open(negative_root, content_failure);
        check(bool(volume), "second independent negative native volume");
        NativeRetailStartupState negative_startup(std::move(*volume), {});
        NativeRetailMainState missing(negative_startup, samples);
        RetailFilePathFailure path_failure;
        missing.file_paths = NativeRetailFilePaths::create(path_failure);
        Fixture missing_fixture; fixture = &missing_fixture;
        check(main_binding::initialize_retail_current_sound(missing, failure) &&
            initial_reset_required::request_mode_one_banks_12a4f0(missing, failure) &&
            initial_reset_required::banks_pending_12abd0(missing, failure) == true,
            "missing actual bank starts pending before real native work");
        check(!main_binding::pump_retail_current_sound(missing, failure) &&
            failure.sound_catalog.kind == RetailSoundCatalogError::bank_failed &&
            failure.sound_catalog.job.bank.archive.kind == sound::ArchiveError::absent_bank,
            "actual failure reaches the Main error path with original archive detail");
        const auto failed = missing.sound_bank_catalog->observe(0, catalog_failure);
        check(failed && !failed->pending && !failed->bank &&
            failed->last_completion == sound::RetailSoundBankJobStatus::failed &&
            !initial_reset_required::banks_pending_12abd0(missing, failure) &&
            missing.sound_frame_counter == 0 && missing_fixture.frames == 0,
            "failed callback state is source-correct but cannot release reset as success");
        // Destruction must complete real cancellation of the other pending
        // requests, not drop their callbacks or leave active native work.
    }
    {
        auto volume = mka::content::NativeContentVolume::open(negative_root, content_failure);
        check(bool(volume), "revocation native content volume");
        NativeRetailStartupState revoked_startup(std::move(*volume), {});
        NativeRetailMainState revoked(revoked_startup, samples);
        RetailFilePathFailure path_failure;
        revoked.file_paths = NativeRetailFilePaths::create(path_failure);
        Fixture revoke_fixture; fixture = &revoke_fixture;
        check(main_binding::initialize_retail_current_sound(revoked, failure), "actual ready empty catalog before revocation");
        auto service_view = revoked.sound_bank_catalog->share_bank_session(catalog_failure);
        sound::AudioBanksFailure bank_failure;
        check(service_view && service_view->close(bank_failure), "explicit canonical native Session close");
        check(!initial_reset_required::banks_pending_12abd0(revoked, failure) &&
            failure.sound_catalog.kind == RetailSoundCatalogError::bank_session,
            "closed shared service cannot masquerade as no-pending reset success");
    }
    fixture = &ordinary;
    std::puts("PASS_CONNECTED_RETAIL_RESET_SOUND; production bank binding/four real callbacks/wait release; unrelated game/context fixtures; no game/endpoint");
    return 0;
} catch (const std::exception& error) { std::fprintf(stderr, "FAIL connected reset sound: %s\n", error.what()); return 1; }
