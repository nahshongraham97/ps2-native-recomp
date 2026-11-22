#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_participants.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include "mka/retail_profile_menu.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#define NOMINMAX
#include <windows.h>

using namespace mka;
using namespace mka::retail;

// Test-only native children for 1888C0 menu downstream effects.
namespace mka::retail {
bool invoke_retail_profile_select_from_2f1630(NativeRetailMainState&,
    std::string_view, RetailProfileMenuFailure&) {
    return true;
}
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState&, std::uint32_t, RetailProfileMenuFailure&) {
    return 0u;
}
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState&, const std::shared_ptr<NativeRetailInputParticipant>&,
    RetailProfileMenuFailure&) {
    return true;
}
bool apply_retail_profile_participant_from_188720(
    NativeRetailMainState&, std::uint32_t, RetailProfileMenuFailure&) {
    return true;
}
} // namespace mka::retail

namespace {

void check(bool condition, const char* msg) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

std::filesystem::path make_temp_root() {
    const auto temp = std::filesystem::temp_directory_path();
    for (unsigned i = 0; ; ++i) {
        auto root = temp / ("mka-prof-test-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directories(root, error)) return root;
        check(i < 100, "create temp test root");
    }
}

void test_initialization_order_and_card_slots() {
    const auto root = make_temp_root();
    mka::content::ContentVolumeFailure content_failure;
    auto content = mka::content::NativeContentVolume::open(root, content_failure);
    check(bool(content), "native content volume open");

    NativeRetailStartupState startup(std::move(*content), {});
    StartupFailure sf;

    check(initialize_retail_memory_owners(startup, sf), "initialize_retail_memory_owners");
    check(initialize_native_retail_game_session(startup, sf), "initialize_native_retail_game_session");

    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);
    check(main.lifetime && main.lifetime->active_on_owner_thread(), "main lifetime active");

    // Initialize required subsystems for 171B10: participants, roles, assignments, mode history
    RetailParticipantFailure pf;
    main.participants = NativeRetailParticipants::create_first_two(pf);
    check(main.participants != nullptr, "participants non-null");

    RetailInputRoleFailure rf;
    main.input_roles = NativeRetailInputRoles::create(rf);
    check(main.input_roles != nullptr, "input roles non-null");
    check(main.input_roles->initialize_roles_from_281e00(rf), "initialize_roles_from_281e00");

    RetailInputAssignmentFailure af;
    main.input_assignments = NativeRetailInputAssignments::create(main.input_roles, main.participants, af);
    check(main.input_assignments != nullptr, "input assignments non-null");

    main.mode_history.emplace();

    // 171B10 requires saved_profile_storage (from 225000)
    // First test missing storage refusal
    auto gate_fail = main_binding::initialize_retail_saved_profile_records(main, sf);
    check(gate_fail == RetailStartupGate::native_failure, "must fail when storage is missing");
    check(sf.kind == StartupError::saved_profile_storage, "failure kind must be saved_profile_storage");

    // Provide valid storage
    save::SaveFailure store_fail;
    auto storage_path = root / "saves";
    main.saved_profile_storage = save::NativeSaveStore::open_or_create(storage_path, store_fail);
    check(main.saved_profile_storage.has_value(), "save store created");

    // Now initialize saved profile records via 171B10
    auto gate = main_binding::initialize_retail_saved_profile_records(main, sf);
    check(gate == RetailStartupGate::ready, "initialize_retail_saved_profile_records succeeded");
    check(main.saved_profile_records != nullptr, "saved_profile_records allocated");
    check(main.saved_profile_records->is_initialized(), "saved_profile_records is_initialized");

    // Verify card slots 0 and 1
    for (std::size_t slot_idx = 0; slot_idx < 2; ++slot_idx) {
        const auto& slot = main.saved_profile_records->card_slot(slot_idx);
        // After 171B10 completion, both statuses must be 1
        check(slot.status_word() == 1, "card slot status must be 1");
        check(slot.flag_byte_0c() == 0, "flag byte 0c must be 0");

        // 8 profiles each
        for (std::size_t p = 0; p < 8; ++p) {
            check(!slot.profile_in_use(p), "profile in use must be false initially");
            const auto& child = slot.child(p);
            check(child.defaults.initialized, "child defaults initialized");
            check(child.gate_word_3c28 == 0, "child gate_word_3c28 must be 0");
            check(child.or_word_fc == 0, "child or_word_fc must be 0");
            check(child.or_words_100_108_110_118[0] == 0, "child or_word 100 must be 0");
            check(child.and_words_120[0] == 0xffffffffu, "child and_words_120 must be 0xffffffff");
            check(child.and_words_1c0[0] == 0xffffffffu, "child and_words_1c0 must be 0xffffffff");

            auto view = child.make_view();
            check(view.gate_word_3c28 == 0, "view gate_word_3c28 matches child");
            check(view.or_word_fc == 0, "view or_word_fc matches child");
            check(view.and_words_120[0] == 0xffffffffu, "view and_words_120 matches child");
            check(view.and_words_1c0[0] == 0xffffffffu, "view and_words_1c0 matches child");
        }

        // Preferences must match 2955D0 defaults
        const auto& prefs = slot.preferences();
        check(prefs.word_24 == 90, "preferences word_24 default 90");
        check(prefs.word_54 == 0xffffffffu, "preferences word_54 default");
    }

    // Double-initialization refusal
    auto gate_dup = main_binding::initialize_retail_saved_profile_records(main, sf);
    check(gate_dup == RetailStartupGate::native_failure, "double-init must fail");
    check(sf.kind == StartupError::already_initialized, "failure kind already_initialized");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::cout << "PASS: test_initialization_order_and_card_slots\n";
}

void test_active_player_reset_and_association_cleanup() {
    const auto root = make_temp_root();
    mka::content::ContentVolumeFailure content_failure;
    auto content = mka::content::NativeContentVolume::open(root, content_failure);
    check(bool(content), "native content volume open");

    NativeRetailStartupState startup(std::move(*content), {});
    StartupFailure sf;

    check(initialize_retail_memory_owners(startup, sf), "initialize_retail_memory_owners");
    check(initialize_native_retail_game_session(startup, sf), "initialize_native_retail_game_session");

    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);

    RetailParticipantFailure pf;
    main.participants = NativeRetailParticipants::create_first_two(pf);
    RetailInputRoleFailure rf;
    main.input_roles = NativeRetailInputRoles::create(rf);
    main.input_roles->initialize_roles_from_281e00(rf);

    RetailInputAssignmentFailure af;
    main.input_assignments = NativeRetailInputAssignments::create(main.input_roles, main.participants, af);

    save::SaveFailure store_fail;
    auto storage_path = root / "saves";
    main.saved_profile_storage = save::NativeSaveStore::open_or_create(storage_path, store_fail);
    main.mode_history.emplace();

    check(main_binding::initialize_retail_saved_profile_records(main, sf) == RetailStartupGate::ready,
          "init records");

    // Manually attach Player 0 to card slot 1, profile 3
    auto& p0 = main.saved_profile_records->active_player(0);
    p0.associated_card_slot = 1;
    p0.associated_profile_slot = 3;
    p0.dirty_flag = 1;
    p0.player_status = 0;
    main.saved_profile_records->card_slot(1).set_profile_in_use(3, true);

    // Call reset_saved_profile_children_246070
    check(initial_reset_required::reset_saved_profile_children_246070(main, sf),
          "reset_saved_profile_children_246070");

    // Check Player 0 after reset
    check(p0.associated_card_slot == -1, "p0 associated card slot reset to -1");
    check(p0.associated_profile_slot == -1, "p0 associated profile slot reset to -1");
    check(p0.dirty_flag == 0, "p0 dirty flag cleared to 0");
    check(p0.retained_per_player_word == 0, "p0 retained per player word cleared");
    check(p0.player_status == 8, "p0 player status written as 8 (00246040)");
    check(p0.defaults.initialized, "p0 defaults initialized");

    // Card slot 1, profile 3 in-use flag must be released to false
    check(!main.saved_profile_records->card_slot(1).profile_in_use(3),
          "card slot 1 profile 3 in-use released");

    // Check Player 1 after reset
    const auto& p1 = main.saved_profile_records->active_player(1);
    check(p1.associated_card_slot == -1, "p1 associated card slot -1");
    check(p1.associated_profile_slot == -1, "p1 associated profile slot -1");
    check(p1.dirty_flag == 0, "p1 dirty flag 0");
    check(p1.player_status == 8, "p1 player status 8");
    check(p1.retained_per_player_word == 0, "p1 retained per player word 0");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::cout << "PASS: test_active_player_reset_and_association_cleanup\n";
}

void test_wrong_thread_refusal() {
    const auto root = make_temp_root();
    mka::content::ContentVolumeFailure content_failure;
    auto content = mka::content::NativeContentVolume::open(root, content_failure);
    check(bool(content), "native content volume open");

    NativeRetailStartupState startup(std::move(*content), {});
    StartupFailure sf;
    check(initialize_retail_memory_owners(startup, sf), "initialize_retail_memory_owners");
    check(initialize_native_retail_game_session(startup, sf), "initialize_native_retail_game_session");

    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);

    std::thread foreign([&]() {
        StartupFailure thread_sf;
        auto gate = main_binding::initialize_retail_saved_profile_records(main, thread_sf);
        check(gate == RetailStartupGate::native_failure, "171B10 must fail on foreign thread");

        bool res2 = initial_reset_required::reset_saved_profile_children_246070(main, thread_sf);
        check(!res2, "246070 must fail on foreign thread");
    });
    foreign.join();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::cout << "PASS: test_wrong_thread_refusal\n";
}

} // namespace

int main() {
    test_initialization_order_and_card_slots();
    test_active_player_reset_and_association_cleanup();
    test_wrong_thread_refusal();
    std::cout << "ALL SAVED PROFILE RECORDS TESTS PASSED.\n";
    return 0;
}
