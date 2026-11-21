#include "mka/retail_profile_aggregate_main.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_profile_transition_child.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

using namespace mka::retail;
namespace {
void check(bool v, const char* message) { if (!v) throw std::runtime_error(message); }
enum class Action { normal, fail, transfer };
Action action = Action::normal;
unsigned action_calls = 0;
}
// Test-only native children. No such default definitions enter production.
namespace mka::retail {
bool invoke_retail_profile_select_from_2f1630(NativeRetailMainState& main,
    std::string_view path, RetailProfileMenuFailure&) {
    ++action_calls;
    check(path == "common/p_select/p_select", "source menu action literal");
    if (action == Action::transfer) throw detail::NativeMainReentry{&main, 19};
    if (action == Action::fail) return false;
    RetailGameSessionFailure failure;
    main.startup.game_session->game_records_values(failure)->word_64e318 = 0x456;
    return true;
}
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState&,std::uint32_t,RetailProfileMenuFailure&) { return 0u; }
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState&,const std::shared_ptr<NativeRetailInputParticipant>&,
    RetailProfileMenuFailure&) {
    throw std::runtime_error("unexpected source event publication for baseline index zero");
}
bool apply_retail_profile_participant_from_188720(NativeRetailMainState&,std::uint32_t role,
    RetailProfileMenuFailure&) {
    check(role == 0, "expected first participant source task transition");
    return true;
}
}
int main(int argc, char** argv) {
    try {
        check(argc == 2, "existing native content root");
        mka::content::ContentVolumeFailure cf;
        auto content = mka::content::NativeContentVolume::open(std::filesystem::u8path(argv[1]), cf);
        check(bool(content), "native content root");
        NativeRetailStartupState startup(std::move(*content), {});
        StartupFailure startup_failure;
        check(initialize_native_retail_game_session(startup, startup_failure), "real retained Session");
        mka::input::ControllerInput input;
        NativeRetailMainState main(startup, input);
        main.mode_history.emplace();
        RetailModeFailure mf;
        check(main.mode_history->push(0, mf), "source mode0");
        RetailGameSessionFailure sf;
        auto* values = startup.game_session->game_records_values(sf);
        auto* aggregate = startup.game_session->profile_aggregate(sf);
        check(values && aggregate, "actual same-session aggregate and live override");
        std::array<RetailProfileAggregateParentView,2> parents{{{-1,0,{}},{-1,0,{}}}};
        RetailProfileAggregateMainFailure failure;
        RetailProfileAggregateFailure af;
        values->word_64e318 = 0x123;
        check(refresh_retail_profile_aggregate_from_2432a0(main, parents, failure) &&
            aggregate->observe(af)->or_words_660508_510_518_520[1] == 0x123 && action_calls == 0,
            "real aggregate publication precedes source history early return");
        check(main.mode_history->push(4, mf), "source history4");
        RetailProfileSceneValues initial{};
        RetailProfileSceneObjectFailure of;
        auto scene = NativeRetailProfileSceneObject::create(startup.game_session,7,initial,of);
        auto* reference = startup.game_session->live_scene_reference(sf);
        RetailLiveSceneFailure lf;
        check(scene && reference && reference->publish_from_2f1ad0(scene,lf) ==
            RetailLiveScenePublishOutcome::published,"actual retained source scene reference");
        RetailParticipantFailure pf;
        main.participants = NativeRetailParticipants::create_first_two(pf);
        check(main.participants && main.participants->first_two(0,pf)->set_game_state(1,pf),
            "actual live participant status owner");
        check(refresh_retail_profile_aggregate_from_2432a0(main, parents, failure) &&
            values->word_64e318 == 0x456 && action_calls == 1 &&
            aggregate->observe(af)->or_words_660508_510_518_520[1] == 0x123,
            "menu effect runs after data publication and may update next refresh input");
        action = Action::transfer;
        bool transferred = false;
        try { (void)refresh_retail_profile_aggregate_from_2432a0(main, parents, failure); }
        catch (const detail::NativeMainReentry& transfer) {
            transferred = transfer.owner == &main && transfer.value == 19;
        }
        check(transferred && aggregate->observe(af)->or_words_660508_510_518_520[1] == 0x456,
            "typed Main transfer preserves already-published aggregate and live baseline reread");
        action = Action::fail;
        values->word_64e318 = 0x789;
        check(!refresh_retail_profile_aggregate_from_2432a0(main, parents, failure) &&
            failure.kind == RetailProfileAggregateMainError::menu &&
            aggregate->observe(af)->or_words_660508_510_518_520[1] == 0x789,
            "named menu failure never rolls back source-before-call data effects");
        std::puts("PASS same-Session aggregate through actual menu,18B0D0 and1892D0; required action/event/task producers open");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr,"FAIL native aggregate Main flow: %s\n",e.what()); return 1;
    }
}
