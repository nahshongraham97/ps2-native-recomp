#include "mka/retail_profile_transition.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_profile_scene_object.hpp"
#include "mka/retail_live_scene_reference.hpp"
#include "mka/retail_participants.hpp"
#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_profile_transition_child.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#define NOMINMAX
#include <windows.h>

namespace {
void check(bool okay, const char* label) {
    if (!okay) { std::cerr << label << '\n'; std::abort(); }
}
struct ChildState {
    unsigned calls{};
    std::uint32_t role{}, publications{};
    bool refuse{}, transfer{}, saw_live_scene{};
} child;

void state(mka::retail::NativeRetailMainState& main,
    std::uint32_t role, std::uint32_t value) {
    mka::retail::RetailParticipantFailure f;
    const auto owner = main.participants->first_two(role, f);
    check(owner && owner->set_game_state(value, f), "actual participant status write");
}
std::uint32_t status(mka::retail::NativeRetailMainState& main,
    std::uint32_t role) {
    mka::retail::RetailParticipantFailure f;
    const auto owner = main.participants->first_two(role, f);
    const auto value = owner ? owner->observe(f) : std::nullopt;
    check(bool(value), "actual participant status read");
    return value->game_state;
}
} // namespace

// Actual18A260,18B0D0 and1892D0 connect to these explicit missing leaf effects.
namespace mka::retail {
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState&,std::uint32_t,RetailProfileMenuFailure&) { return 0u; }
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState&,const std::shared_ptr<NativeRetailInputParticipant>&,
    RetailProfileMenuFailure&) { ++child.publications;return true; }
bool apply_retail_profile_participant_from_188720(NativeRetailMainState& main,
    std::uint32_t role, RetailProfileMenuFailure&) {
    ++child.calls;
    child.role = role;
    RetailGameSessionFailure sf;
    RetailLiveSceneFailure lf;
    auto* reference = main.startup.game_session->live_scene_reference(sf);
    auto fresh = reference ? reference->lookup_from_2f18f0(lf) : std::nullopt;
    child.saw_live_scene = fresh && bool(*fresh);
    if (child.transfer) throw detail::NativeMainReentry{&main, 29};
    return !child.refuse;
}
} // namespace mka::retail

int main() {
    using namespace mka::retail;
    const auto temp = std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for (unsigned i = 0; ; ++i) {
        root = temp / ("mka-profile-transition-owned-" +
            std::to_string(GetCurrentProcessId()) + "-" +
            std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directory(root, error)) break;
        check(i < 100, "exclusive transition fixture root");
    }
    {
        mka::content::ContentVolumeFailure cf;
        auto content = mka::content::NativeContentVolume::open(root, cf);
        check(bool(content), "actual native content root for Main lifetime");
        NativeRetailStartupState startup(std::move(*content), {});
        StartupFailure startup_failure;
        check(initialize_native_retail_game_session(startup, startup_failure),
            "actual retained game Session birth");
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup, samples);
        RetailProfileMenuFailure f;
        RetailGameSessionFailure sf;
        check(bool(startup.game_session), "actual retained game Session");
        std::array<RetailProfileAggregateParentView,2> parents{{{5,0,{}},{5,0,{}}}};
        RetailProfileAggregateFailure aggregate_failure;
        check(startup.game_session->profile_aggregate(sf)->refresh_data_from_2432a0(parents,0,aggregate_failure),
            "actual source aggregate before nested entry decision");

        // The source getter precedes participant access and returns zero for
        // BSS-null, stale cookie, and an expired nonowning reference.
        auto result = run_retail_profile_transition_from_18a260(main, 0, f);
        check(result && !*result && f.kind == RetailProfileMenuError::none &&
            child.calls == 0, "BSS-null scene is source result zero");
        check(!transition_retail_profile_from_18a260(main, 2, f) &&
            f.kind == RetailProfileMenuError::invalid_role,
            "unknown role is a named native failure");

        RetailParticipantFailure pf;
        main.participants = NativeRetailParticipants::create_first_two(pf);
        check(bool(main.participants), "real first-two participant owner");
        RetailProfileSceneValues initial{};
        initial.lanes[0] = {63, 12, 13};
        initial.lanes[1] = {63, 22, 23};
        initial.word_60 = 0;
        RetailProfileSceneObjectFailure of;
        auto scene = NativeRetailProfileSceneObject::create(
            startup.game_session, 9, initial, of);
        check(bool(scene), "typed source scene object with explicit original values");
        auto* reference = startup.game_session->live_scene_reference(sf);
        check(reference != nullptr, "actual same-Session live reference");
        RetailLiveSceneFailure lf;
        auto published = reference->publish_from_2f1ad0(scene, lf);
        check(published && *published == RetailLiveScenePublishOutcome::published,
            "real 2F1AD0 scene publication");

        state(main, 1, 2);
        result = run_retail_profile_transition_from_18a260(main, 0, f);
        auto observed = scene->observe(of);
        check(result && !*result && observed && observed->values.word_60 == 0 &&
            observed->values.lanes[0].word_2c == 13 && child.calls == 0,
            "opposite status two returns before concrete scene demand or write");

        state(main, 1, 3);
        check(startup.game_session->store_scalar_mode_from_source(4, sf),
            "live source scalar mode four");
        result = run_retail_profile_transition_from_18a260(main, 0, f);
        observed = scene->observe(of);
        check(result && *result && observed &&
            observed->values.lanes[0].word_2c == 1 &&
            observed->values.lanes[0].word_24 == 0 &&
            observed->values.lanes[1].word_2c == 23 &&
            observed->values.word_60 == 0xffffffffu &&
            status(main, 0) == 1 && status(main, 1) == 0 &&
            child.calls == 1 && child.role == 0 && child.publications == 1 &&
            observed->values.lanes[0].word_10 == 0 &&
            child.saw_live_scene,
            "role zero ordered scene stores, wrap, selected one, opposite zero, child");

        // The source's role-one branch changes the other scene lane and
        // supplies nested argument nine. Ordinary child failure retains all
        // earlier stores and reports a missing native child separately.
        check(startup.game_session->store_scalar_mode_from_source(3, sf),
            "live source scalar mode three");
        state(main, 0, 3);
        child = {};
        child.refuse = true;
        result = run_retail_profile_transition_from_18a260(main, 1, f);
        observed = scene->observe(of);
        check(!result && f.kind == RetailProfileMenuError::required_child &&
            observed && observed->values.lanes[1].word_2c == 1 &&
            observed->values.lanes[1].word_24 == 0 &&
            observed->values.word_60 == 0xfffffffeu &&
            status(main, 1) == 1 && status(main, 0) == 3 &&
            child.calls == 1 && child.role == 1 && child.publications == 1 &&
            observed->values.lanes[1].word_10 == 9,
            "role one child failure retains preceding source writes");

        child = {};
        child.transfer = true;
        bool transferred = false;
        try { (void)run_retail_profile_transition_from_18a260(main, 1, f); }
        catch (const detail::NativeMainReentry& transfer) {
            transferred = transfer.owner == &main && transfer.value == 29;
        }
        check(transferred && status(main, 1) == 1,
            "typed Main transfer propagates after source effects");

        child = {};
        RetailProfileMenuFailure foreign_failure;
        std::optional<bool> foreign_result = true;
        std::thread foreign([&] {
            foreign_result = run_retail_profile_transition_from_18a260(
                main, 0, foreign_failure);
        });
        foreign.join();
        check(!foreign_result &&
            foreign_failure.kind == RetailProfileMenuError::main_lifetime &&
            child.calls == 0, "foreign Main thread refuses before effects");

        check(reference->reset_from_2f1750(lf), "real scene reference reset");
        result = run_retail_profile_transition_from_18a260(main, 0, f);
        check(result && !*result && child.calls == 0,
            "cleared scene reference returns original zero");
        check(scene->stage_from_18a260(1, true, of),
            "source bit-one branch directly on typed scene owner");
        observed = scene->observe(of);
        check(observed && observed->values.lanes[1].word_2c == 2,
            "source byte +14 bit one maps to scene value two");
    }
    check(std::filesystem::remove(root), "retire empty exclusive fixture root");
    std::cout << "PASS source-ordered18A260 through actual18B0D0 and1892D0; scene/action/event/task producers open\n";
}
