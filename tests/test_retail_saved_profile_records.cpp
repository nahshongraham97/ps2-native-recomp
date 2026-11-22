#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "native_retail_startup.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#define NOMINMAX
#include <windows.h>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL retail saved profile records: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using namespace mka::retail;

    const auto temp = std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for (unsigned i = 0; ; ++i) {
        root = temp / ("mka-save-records-owned-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directory(root, error)) break;
        require(i < 100, "create exclusively owned test root");
    }

    const auto saves_dir = root / "saves";
    std::filesystem::create_directories(saves_dir);

    mka::content::ContentVolumeFailure content_failure;
    auto content = mka::content::NativeContentVolume::open(root, content_failure);
    require(bool(content), "native test content volume");

    NativeRetailStartupState startup(std::move(*content), {});
    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);

    mka::save::SaveFailure save_failure;
    auto storage = mka::save::NativeSaveStore::open_or_create(saves_dir, save_failure);
    require(bool(storage), "native save store opened");
    main.saved_profile_storage = std::move(storage);
    main.post_overlay_state = 0; // Language: English

    // 1. Initial 171B10 startup
    StartupFailure failure;
    const auto gate = main_binding::initialize_retail_saved_profile_records(main, failure);
    require(gate == RetailStartupGate::ready, "171B10 ready gate");
    require(failure.kind == StartupError::none, "171B10 clear failure");
    require(main.saved_profile_records != nullptr, "saved_profile_records owner present");
    require(main.saved_profile_records->is_initialized(), "saved_profile_records initialized");

    // Verify Slot 0 and Slot 1 properties
    for (std::size_t slot = 0; slot < 2; ++slot) {
        const auto& card = main.saved_profile_records->slot(slot);
        require(card.status_word() == 1, "card status word ready (1)");
        require(card.flag_byte_12() == 0, "flag byte 12 cleared");
        require(card.preferences().levels[0] == 0.75f, "default preferences level 0");
        require(card.preferences().levels[1] == 0.75f, "default preferences level 1");

        for (std::size_t p = 0; p < NativeRetailSavedCardSlot::profile_count; ++p) {
            require(!card.is_profile_in_use(p), "profile slot initially not in use");
            const auto& child = card.child(p);
            require(child.defaults.initialized, "child profile defaults initialized");
            require(child.defaults.empty_name == "EMPTY", "empty name initialized to EMPTY");
            require(child.defaults.first_action_masks[0] == 1, "first action mask bit 0");
            require(child.defaults.second_action_masks[1] == 2, "second action mask bit 1");
            require(child.defaults.gameplay.bitword_1164 == 18, "gameplay bitword 1164");
            require(child.defaults.first_unassigned_indices[0] == -1, "unassigned index 0 is -1");
            require(child.defaults.second_unassigned_indices[0] == -1, "second unassigned index 0 is -1");
        }
    }

    // Verify Active Players (Player 0 and Player 1)
    for (std::size_t p = 0; p < 2; ++p) {
        const auto& player = main.saved_profile_records->active_player(p);
        require(player.card_slot == -1, "active player card_slot unassigned (-1)");
        require(player.profile_index == -1, "active player profile_index unassigned (-1)");
        require(player.dirty_flag == 0, "active player dirty flag clean (0)");
        require(player.status == 8, "active player status is 8 (DAT_0064c720 default unattached)");
        require(player.profile_data.initialized, "active player defaults initialized");
        require(player.profile_data.empty_name == "EMPTY", "active player default name is EMPTY");
    }

    // Verify Aggregate observe
    RetailProfileAggregateFailure agg_failure;
    const auto agg_data = main.saved_profile_records->aggregate().observe(agg_failure);
    require(agg_data.has_value(), "aggregate observation valid");
    require(agg_failure.kind == RetailProfileAggregateError::none, "aggregate observation clear failure");

    // 2. Test Player Profile Attachment and 246070 Reset
    RetailSavedProfileRecordsFailure rec_failure;
    require(main.saved_profile_records->attach_player_profile(0, 0, 3, rec_failure),
            "attach player 0 to slot 0 profile 3");
    require(main.saved_profile_records->slot(0).is_profile_in_use(3),
            "slot 0 profile 3 now marked in use");
    require(main.saved_profile_records->active_player(0).card_slot == 0,
            "player 0 card_slot is 0");
    require(main.saved_profile_records->active_player(0).profile_index == 3,
            "player 0 profile_index is 3");
    require(main.saved_profile_records->active_player(0).status == 0,
            "player 0 status is 0 (attached)");

    require(main.saved_profile_records->attach_player_profile(1, 1, 6, rec_failure),
            "attach player 1 to slot 1 profile 6");
    require(main.saved_profile_records->slot(1).is_profile_in_use(6),
            "slot 1 profile 6 now marked in use");
    require(main.saved_profile_records->active_player(1).card_slot == 1,
            "player 1 card_slot is 1");
    require(main.saved_profile_records->active_player(1).profile_index == 6,
            "player 1 profile_index is 6");
    require(main.saved_profile_records->active_player(1).status == 0,
            "player 1 status is 0 (attached)");

    // Modify dirty flag on player 0
    main.saved_profile_records->active_player(0).dirty_flag = 1;

    // Call 246070 reset
    require(initial_reset_required::reset_saved_profile_children_246070(main, failure),
            "246070 reset execution");
    require(failure.kind == StartupError::none, "246070 clear failure");

    // Verify in-use flags were cleared on both card slots!
    require(!main.saved_profile_records->slot(0).is_profile_in_use(3),
            "slot 0 profile 3 in-use cleared by 246070");
    require(!main.saved_profile_records->slot(1).is_profile_in_use(6),
            "slot 1 profile 6 in-use cleared by 246070");

    // Verify both players reset to default unattached state
    for (std::size_t p = 0; p < 2; ++p) {
        const auto& player = main.saved_profile_records->active_player(p);
        require(player.card_slot == -1, "player reset card_slot is -1");
        require(player.profile_index == -1, "player reset profile_index is -1");
        require(player.dirty_flag == 0, "player reset dirty_flag is 0");
        require(player.status == 8, "player reset status is 8");
        require(player.profile_data.initialized, "player reset profile data initialized");
    }

    // 3. Re-entry & Error Handling
    // Double initialization must fail
    const auto double_gate = main_binding::initialize_retail_saved_profile_records(main, failure);
    require(double_gate == RetailStartupGate::native_failure, "double 171B10 init fails");
    require(failure.kind == StartupError::already_initialized, "already initialized error");

    // Test missing records on reset
    auto preserved_records = std::move(main.saved_profile_records);
    require(!initial_reset_required::reset_saved_profile_children_246070(main, failure),
            "reset fails when records missing");
    require(failure.kind == StartupError::saved_profile_storage, "missing records error");
    main.saved_profile_records = std::move(preserved_records);

    // 4. Multi-language Test
    {
        NativeRetailSavedProfileRecords records_de;
        require(records_de.initialize_records_from_171b10(2, rec_failure),
                "initialize with German language (2)");
        require(records_de.slot(0).child(0).defaults.empty_name == "LEER",
                "German empty name is LEER");
        require(records_de.active_player(0).profile_data.empty_name == "LEER",
                "German active player default name is LEER");

        NativeRetailSavedProfileRecords records_fr;
        require(records_fr.initialize_records_from_171b10(3, rec_failure),
                "initialize with French language (3)");
        require(records_fr.slot(0).child(0).defaults.empty_name == "VIDE",
                "French empty name is VIDE");
    }

    // Cleanup test root
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    std::cout << "PASS native retail saved profile records and reset 246070\n";
    return 0;
}
