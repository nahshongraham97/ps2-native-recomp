#include "mka/retail_profile_menu.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_participants.hpp"
#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_profile_transition_child.hpp"
#include "mka/retail_input_assignments.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#define NOMINMAX
#include <windows.h>

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}

struct TestChildren {
    bool resource_error{};
    bool resource_changes_status{};
    bool resource_blocks_both{};
    bool first_transition_changes_second{};
    bool transfer{};
    std::vector<std::string> calls;
} children;

void state(mka::retail::NativeRetailMainState& main,
    std::uint32_t role, std::uint32_t value) {
    mka::retail::RetailParticipantFailure failure;
    const auto owner = main.participants->first_two(role, failure);
    require(bool(owner) && owner->set_game_state(value, failure),
        "set actual participant game-state word");
}
} // namespace

// Getter,18A260,18B0D0 and1892D0 are production. Only actual missing
// action/permission/event/task effects below are fixture-defined.
namespace mka::retail {
bool invoke_retail_profile_select_from_2f1630(
    NativeRetailMainState& main, std::string_view path, RetailProfileMenuFailure&) {
    require(path == "common/p_select/p_select", "exact source menu literal");
    children.calls.push_back("p_select");
    if (children.resource_error) return false;
    if (children.transfer) throw detail::NativeMainReentry{&main, 23};
    if (children.resource_changes_status) {
        state(main, 0, 2);
        state(main, 1, 1);
    }
    if (children.resource_blocks_both) {
        state(main, 0, 2);
        state(main, 1, 2);
    }
    return true;
}
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState&,std::uint32_t,RetailProfileMenuFailure&) { return 0u; }
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState&,const std::shared_ptr<NativeRetailInputParticipant>&,
    RetailProfileMenuFailure&) {
    require(false,"baseline index zero must not reach publication in this fixture");
    return false;
}
bool apply_retail_profile_participant_from_188720(
    NativeRetailMainState& main, std::uint32_t role,
    RetailProfileMenuFailure&) {
    children.calls.push_back("188720_" + std::to_string(role));
    if (role == 0 && children.first_transition_changes_second)
        state(main, 1, 2);
    return true;
}
} // namespace mka::retail

int main() {
    using namespace mka::retail;
    const auto temp = std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for (unsigned i = 0; ; ++i) {
        root = temp / ("mka-profile-menu-owned-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directory(root, error)) break;
        require(i < 100, "create exclusively owned profile menu fixture");
    }
    {
        mka::content::ContentVolumeFailure cf;
        auto content = mka::content::NativeContentVolume::open(root, cf);
        require(bool(content), "real native content root for Main lifetime");
        NativeRetailStartupState startup(std::move(*content), {});
        StartupFailure startup_failure;
        require(initialize_native_retail_game_session(startup, startup_failure),
            "actual retained game Session birth");
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup, samples);
        RetailProfileMenuFailure failure;
        RetailModeFailure mode_failure;

        require(!run_retail_profile_menu_from_1888c0(main, failure) &&
            failure.kind == RetailProfileMenuError::missing_mode_history,
            "missing real history owner is named");
        main.mode_history.emplace();
        require(main.mode_history->push(0, mode_failure), "source mode zero");
        children = {};
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls.empty(), "history absent mode four returns before scene child");

        require(main.mode_history->push(4, mode_failure), "source history contains four");
        children = {};
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls.empty(),
            "null source scene returns before participant lookup");

        RetailParticipantFailure participant_failure;
        main.participants = NativeRetailParticipants::create_first_two(participant_failure);
        require(bool(main.participants), "source-born first two participants");
        RetailInputRoleFailure role_failure;
        auto roles = NativeRetailInputRoles::create(role_failure);
        RetailInputAssignmentFailure assignment_failure;
        auto assignments = NativeRetailInputAssignments::create(roles,main.participants,assignment_failure);
        require(assignments && assignments->bind_direct(
            main.participants->first_two(0,participant_failure),RetailInputRole::role0,assignment_failure),
            "real assignment changes first ordinal from five to zero");
        RetailGameSessionFailure session_failure;
        std::array<RetailProfileAggregateParentView,2> parents{{{5,0,{}},{5,0,{}}}};
        RetailProfileAggregateFailure aggregate_failure;
        require(startup.game_session->profile_aggregate(session_failure)->refresh_data_from_2432a0(
            parents,0,aggregate_failure),"actual profile mask computation before decision child");
        require(startup.game_session->store_scalar_mode_from_source(3, session_failure),
            "explicit non-four source mode for ordered lane writes");
        RetailProfileSceneValues initial{};
        initial.word_60 = 10;
        RetailProfileSceneObjectFailure object_failure;
        auto scene = NativeRetailProfileSceneObject::create(
            startup.game_session, 7, initial, object_failure);
        auto* reference = startup.game_session->live_scene_reference(session_failure);
        RetailLiveSceneFailure live_failure;
        require(scene && reference && reference->publish_from_2f1ad0(scene, live_failure) ==
            RetailLiveScenePublishOutcome::published, "real typed menu scene publication");
        children = {};
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls.empty(),
            "neither source status one returns before resource action");

        state(main, 0, 1);
        state(main, 1, 2);
        children = {};
        children.resource_changes_status = true;
        children.first_transition_changes_second = true;
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls == std::vector<std::string>{
                "p_select", "188720_0", "188720_1"} &&
            scene->observe(object_failure)->values.word_60 == 8 &&
            scene->observe(object_failure)->values.lanes[0].word_2c == 1 &&
            scene->observe(object_failure)->values.lanes[1].word_2c == 1,
            "status zero reload follows resource and status one reload follows transition");

        state(main, 0, 1);
        state(main, 1, 1);
        children = {};
        children.first_transition_changes_second = true;
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls == std::vector<std::string>{
                "p_select", "188720_0", "188720_1"} &&
            scene->observe(object_failure)->values.word_60 == 7,
            "first transition changes live second status before final branch");

        state(main, 0, 1);
        state(main, 1, 1);
        children = {};
        children.resource_blocks_both = true;
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls == std::vector<std::string>{"p_select"} &&
            failure.kind == RetailProfileMenuError::none &&
            scene->observe(object_failure)->values.word_60 == 7,
            "real 18A260 source-zero results are successful no-effect calls to its parent");

        state(main, 0, 1);
        children = {};
        children.resource_error = true;
        require(!run_retail_profile_menu_from_1888c0(main, failure) &&
            failure.kind == RetailProfileMenuError::required_child &&
            children.calls == std::vector<std::string>{"p_select"},
            "reached missing resource child fails by name");

        children = {};
        children.transfer = true;
        bool propagated = false;
        try { (void)run_retail_profile_menu_from_1888c0(main, failure); }
        catch (const detail::NativeMainReentry& transfer) {
            propagated = transfer.owner == &main && transfer.value == 23;
        }
        require(propagated, "Main nonlocal transfer propagates through menu child");

        children = {};
        auto retained_participants = std::move(main.participants);
        require(!run_retail_profile_menu_from_1888c0(main, failure) &&
            failure.kind == RetailProfileMenuError::missing_participants,
            "live scene requires actual participant owner");
        main.participants = std::move(retained_participants);

        auto retained_session = std::move(startup.game_session);
        require(!run_retail_profile_menu_from_1888c0(main, failure) &&
            failure.kind == RetailProfileMenuError::session,
            "actual scene getter refuses a missing Session rather than reporting source null");
        startup.game_session = std::move(retained_session);

        children = {};
        RetailProfileMenuFailure foreign_failure;
        bool foreign_result = true;
        std::thread foreign([&] {
            foreign_result = run_retail_profile_menu_from_1888c0(main, foreign_failure);
        });
        foreign.join();
        require(!foreign_result && foreign_failure.kind == RetailProfileMenuError::main_lifetime &&
            children.calls.empty(), "foreign Main call refuses before child effects");
    }
    require(std::filesystem::remove(root), "retire exclusively owned empty fixture root");
    std::cout << "PASS profile menu with actual scene lookup,18A260,18B0D0 and1892D0; required action/event/task producers open\n";
}
