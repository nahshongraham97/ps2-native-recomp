#include "mka/retail_profile_menu.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_participants.hpp"
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
    bool live{};
    bool scene_error{};
    bool resource_error{};
    bool resource_changes_status{};
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

// Explicit test-only bodies for production's deliberately undefined children.
namespace mka::retail {
std::optional<bool> observe_retail_profile_live_scene_from_2f18f0(
    NativeRetailMainState&, RetailProfileMenuFailure&) {
    children.calls.push_back("live_scene");
    if (children.scene_error) return std::nullopt;
    return children.live;
}
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
    return true;
}
bool transition_retail_profile_from_18b0d0(
    NativeRetailMainState& main, std::uint32_t role, std::uint32_t argument,
    RetailProfileMenuFailure&) {
    require((role == 0 && argument == 0) || (role == 1 && argument == 9),
        "source 18B0D0 argument pair");
    children.calls.push_back("18b0d0_" + std::to_string(role));
    if (role == 0 && children.first_transition_changes_second)
        state(main, 1, 2);
    return true;
}
bool transition_retail_profile_from_18a260(
    NativeRetailMainState& main, std::uint32_t role, RetailProfileMenuFailure&) {
    require(role <= 1, "source 18A260 participant argument");
    children.calls.push_back("18a260_" + std::to_string(role));
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
            children.calls == std::vector<std::string>{"live_scene"},
            "null source scene returns before participant lookup");

        RetailParticipantFailure participant_failure;
        main.participants = NativeRetailParticipants::create_first_two(participant_failure);
        require(bool(main.participants), "source-born first two participants");
        children = {};
        children.live = true;
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls == std::vector<std::string>{"live_scene"},
            "neither source status one returns before resource action");

        state(main, 0, 1);
        state(main, 1, 2);
        children = {};
        children.live = true;
        children.resource_changes_status = true;
        children.first_transition_changes_second = true;
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls == std::vector<std::string>{
                "live_scene", "p_select", "18a260_0", "18a260_1"},
            "status zero reload follows resource and status one reload follows transition");

        state(main, 0, 1);
        state(main, 1, 1);
        children = {};
        children.live = true;
        children.first_transition_changes_second = true;
        require(run_retail_profile_menu_from_1888c0(main, failure) &&
            children.calls == std::vector<std::string>{
                "live_scene", "p_select", "18b0d0_0", "18a260_1"},
            "first transition changes live second status before final branch");

        children = {};
        children.live = true;
        children.resource_error = true;
        require(!run_retail_profile_menu_from_1888c0(main, failure) &&
            failure.kind == RetailProfileMenuError::required_child &&
            children.calls == std::vector<std::string>{"live_scene", "p_select"},
            "reached missing resource child fails by name");

        children = {};
        children.live = true;
        children.transfer = true;
        bool propagated = false;
        try { (void)run_retail_profile_menu_from_1888c0(main, failure); }
        catch (const detail::NativeMainReentry& transfer) {
            propagated = transfer.owner == &main && transfer.value == 23;
        }
        require(propagated, "Main nonlocal transfer propagates through menu child");

        children = {};
        children.live = true;
        auto retained_participants = std::move(main.participants);
        require(!run_retail_profile_menu_from_1888c0(main, failure) &&
            failure.kind == RetailProfileMenuError::missing_participants,
            "live scene requires actual participant owner");
        main.participants = std::move(retained_participants);

        children = {};
        children.live = true;
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
    std::cout << "PASS source-ordered profile menu driver with explicit test-only children; production scene/action/transition children open\n";
}
