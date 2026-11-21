#include "mka/retail_profile_transition_child.hpp"
#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <array>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
void check(bool value,const char* label) { if(!value) throw std::runtime_error(label); }
struct Effects {
    unsigned permission_calls{}, publications{}, tasks{};
    bool permission_allow{}, permission_refuse{}, permission_transfer{};
    bool publication_refuse{}, task_refuse{}, task_transfer{}, first_task_sets_first_two{};
    std::weak_ptr<NativeRetailProfileSceneObject> scene;
    std::vector<std::uint32_t> lane_at_task;
    std::vector<std::uint32_t> first_state_at_task;
    std::shared_ptr<NativeRetailInputParticipant> published_participant;
} effects;
std::uint32_t state(NativeRetailMainState& main,std::uint32_t role) {
    RetailParticipantFailure f;
    const auto value=main.participants->first_two(role,f)->observe(f);
    check(bool(value),"observe actual participant");return value->game_state;
}
void state(NativeRetailMainState& main,std::uint32_t role,std::uint32_t value) {
    RetailParticipantFailure f;
    check(main.participants->first_two(role,f)->set_game_state(value,f),"write actual source1319E0 state");
}
class OtherScene final : public NativeRetailSceneObject {
public:
    explicit OtherScene(std::shared_ptr<NativeRetailGameSession> owner):owner_(owner),identity_(owner.get()){}
    const NativeRetailGameSession* owner_session() const noexcept override { return identity_; }
    bool scene_identity_live() const noexcept override { return live_&&!owner_.expired(); }
    std::uint32_t source_cookie() const noexcept override { return 19; }
    void cleanup_from_12fa00() override { live_=false; }
private:
    std::weak_ptr<NativeRetailGameSession> owner_;
    const NativeRetailGameSession* identity_;
    bool live_{true};
};
}

// These three leaf effects have no production success defaults. Both18B0D0
// and1892D0 below are the real linked implementations.
namespace mka::retail {
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState& main,std::uint32_t role,RetailProfileMenuFailure&) {
    ++effects.permission_calls;
    check(role<=1,"source caller role to4EEE60");
    if(effects.permission_transfer) throw detail::NativeMainReentry{&main,41};
    if(effects.permission_refuse) return std::nullopt;
    return effects.permission_allow?1u:0u;
}
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState&,const std::shared_ptr<NativeRetailInputParticipant>& participant,
    RetailProfileMenuFailure&) {
    ++effects.publications;effects.published_participant=participant;
    return !effects.publication_refuse;
}
bool apply_retail_profile_participant_from_188720(
    NativeRetailMainState& main,std::uint32_t role,RetailProfileMenuFailure&) {
    ++effects.tasks;
    check(role<=1,"source role to188720");
    effects.first_state_at_task.push_back(state(main,0));
    if(const auto scene=effects.scene.lock()) {
        RetailProfileSceneObjectFailure f;
        const auto value=scene->observe(f);
        check(bool(value),"observe retained scene at actual task call");
        effects.lane_at_task.push_back(value->values.lanes[role].word_10);
    }
    if(effects.first_task_sets_first_two && effects.tasks==1) state(main,0,2);
    if(effects.task_transfer) throw detail::NativeMainReentry{&main,43};
    return !effects.task_refuse;
}
}

int main(int argc,char** argv) {
    try {
        check(argc==2,"native content root argument");
        mka::content::ContentVolumeFailure cf;
        auto content=mka::content::NativeContentVolume::open(std::filesystem::u8path(argv[1]),cf);
        check(bool(content),"actual native content directory");
        NativeRetailStartupState startup(std::move(*content),{});
        StartupFailure startup_failure;
        check(initialize_native_retail_game_session(startup,startup_failure),"actual retained Session");
        mka::input::ControllerInput input;
        NativeRetailMainState main(startup,input);
        RetailParticipantFailure pf;
        main.participants=NativeRetailParticipants::create_first_two(pf);
        check(bool(main.participants),"actual participant owner");
        RetailGameSessionFailure sf;
        const auto session=startup.game_session;
        std::array<RetailProfileAggregateParentView,2> parents{{{5,0,{}},{5,0,{}}}};
        RetailProfileAggregateFailure af;
        check(session->profile_aggregate(sf)->refresh_data_from_2432a0(parents,0,af),
            "actual aggregate computed before source entry decision");
        RetailProfileMenuFailure f;
        check(transition_retail_profile_from_18b0d0(main,0,0,f) && effects.tasks==0,
            "source null scene is native successful void early return");
        RetailProfileSceneValues initial{};
        initial.lanes[0].word_10=63;initial.lanes[1].word_10=63;
        initial.word_60=4;
        RetailProfileSceneObjectFailure of;
        auto scene=NativeRetailProfileSceneObject::create(session,7,initial,of);
        auto* reference=session->live_scene_reference(sf);
        RetailLiveSceneFailure lf;
        check(scene && reference && reference->publish_from_2f1920(scene,lf)==
            RetailLiveScenePublishOutcome::published,"actual scene publication");

        // Before281060, first.ordinal is5. Role0 therefore selects SECOND
        // base+8, even though its role-indexed state comes from first+8.
        state(main,0,0);state(main,1,2);effects={};effects.scene=scene;
        check(transition_retail_profile_from_18b0d0(main,0,7,f) && state(main,0)==0 &&
            state(main,1)==1 && effects.tasks==1 && effects.publications==0 &&
            effects.lane_at_task==std::vector<std::uint32_t>{7},
            "fresh ordinal five selects second; final scene store precedes task delay effect");

        const auto controls=session->controls(sf);
        auto* domain=session->task_domain(sf);
        RetailTaskDomainFailure df;
        RetailGameControlsFailure gf;
        check(controls && domain && controls->store_selection_flags_from_source(0,gf),"real controls");
        auto pair=domain->begin_live_pair_from_28e2b0(0,0,df);
        check(pair && pair->partner_word==0 && pair->required_followup==RetailTaskLiveFollowup::none,
            "real source partner-zero producer without pending callback");
        check(controls->store_selection_flags_from_source(1,gf),"live selection bit zero set");
        auto other=std::make_shared<OtherScene>(session);
        check(reference->reset_from_2f1750(lf) && reference->publish_from_2f1920(other,lf)==
            RetailLiveScenePublishOutcome::published,"live non-menu scene for no-field gate arm");
        state(main,1,0);effects={};
        check(transition_retail_profile_from_18b0d0(main,0,9,f) && effects.tasks==0,
            "gate selected state zero exits without requiring menu subtype");
        state(main,1,2);
        check(transition_retail_profile_from_18b0d0(main,0,9,f) && state(main,1)==1 &&
            effects.tasks==1 && scene->observe(of)->values.lanes[0].word_10==7,
            "gate nonzero selected state writes one and task only, no scene store");
        check(reference->reset_from_2f1750(lf) && reference->publish_from_2f1920(scene,lf)==
            RetailLiveScenePublishOutcome::published,"restore actual menu object");
        check(controls->store_selection_flags_from_source(0,gf),"ordinary source gate");

        RetailInputRoleFailure rf;
        auto roles=NativeRetailInputRoles::create(rf);
        RetailInputAssignmentFailure assignment_failure;
        auto assignments=NativeRetailInputAssignments::create(roles,main.participants,assignment_failure);
        const auto first=main.participants->first_two(0,pf);
        check(assignments && assignments->bind_direct(first,RetailInputRole::role0,assignment_failure) &&
            first->observe(pf)->ordinal==0,"actual281060 path changes live first ordinal");
        state(main,0,1);state(main,1,3);
        check(scene->store_lane_word_from_18b0d0(0,63,of),"source-valid table index63");
        effects={};effects.scene=scene;
        check(transition_retail_profile_from_18b0d0(main,0,0,f) && effects.publications==1 &&
            effects.published_participant==first && effects.tasks==1 &&
            effects.lane_at_task==std::vector<std::uint32_t>{0},
            "key3 decision nonzero publishes role participant then stores argument before task");

        effects={};effects.scene=scene;
        check(transition_retail_profile_from_18b0d0(main,0,9,f) && effects.publications==0 &&
            effects.tasks==1 && effects.lane_at_task==std::vector<std::uint32_t>{0},
            "key48 decision zero skips publication/store but still calls task");
        effects={};effects.scene=scene;effects.first_task_sets_first_two=true;
        check(transition_retail_profile_from_18b0d0(main,0,9,f) && state(main,0)==1 &&
            effects.tasks==2 && effects.lane_at_task==std::vector<std::uint32_t>{0,9} &&
            effects.first_state_at_task==std::vector<std::uint32_t>{1,1},
            "live selected state reload observes callback two; final state and field precede second task");

        check(scene->store_lane_word_from_18b0d0(0,64,of),"retain explicit unproved index for refusal");
        effects={};
        check(!transition_retail_profile_from_18b0d0(main,0,0,f) &&
            f.kind==RetailProfileMenuError::profile_entry_index && effects.tasks==0 && effects.publications==0,
            "unproved table index refuses before child effects");
        check(scene->store_lane_word_from_18b0d0(0,63,of),"restore source-valid index");
        effects={};effects.publication_refuse=true;
        check(!transition_retail_profile_from_18b0d0(main,0,0,f) &&
            f.kind==RetailProfileMenuError::required_child && effects.tasks==0 &&
            scene->observe(of)->values.lanes[0].word_10==63,
            "publication failure preserves source-before-store ordering");
        state(main,1,1);effects={};effects.scene=scene;effects.task_refuse=true;
        check(!transition_retail_profile_from_18b0d0(main,1,9,f) &&
            f.kind==RetailProfileMenuError::required_child && effects.publications==1 &&
            scene->observe(of)->values.lanes[1].word_10==9,
            "task failure retains prior scene store");
        check(scene->store_lane_word_from_18b0d0(1,63,of),"repeat positive branch");
        effects={};effects.scene=scene;effects.task_transfer=true;
        bool transferred=false;
        try { (void)transition_retail_profile_from_18b0d0(main,1,9,f); }
        catch(const detail::NativeMainReentry& transfer) {
            transferred=transfer.owner==&main && transfer.value==43;
        }
        check(transferred && scene->observe(of)->values.lanes[1].word_10==9,
            "Main transfer propagates with preceding source store retained");
        check(scene->store_lane_word_from_18b0d0(0,30,of),"special key62 table row");
        effects={};effects.permission_refuse=true;
        check(!transition_retail_profile_from_18b0d0(main,0,0,f) && effects.permission_calls==1 &&
            f.kind==RetailProfileMenuError::required_child && effects.tasks==0 && effects.publications==0,
            "real1892D0 requires missing4EEE60 for key62");
        effects={};effects.permission_transfer=true;transferred=false;
        try { (void)transition_retail_profile_from_18b0d0(main,0,0,f); }
        catch(const detail::NativeMainReentry& transfer) {
            transferred=transfer.owner==&main && transfer.value==41;
        }
        check(transferred && scene->observe(of)->values.lanes[0].word_10==30,
            "decision child transfer occurs before source scene store");

        effects={};RetailProfileMenuFailure foreign_failure;bool foreign_result=true;
        std::thread foreign([&] {foreign_result=transition_retail_profile_from_18b0d0(main,0,0,foreign_failure);});
        foreign.join();
        check(!foreign_result && foreign_failure.kind==RetailProfileMenuError::main_lifetime && effects.tasks==0,
            "foreign Main thread cannot begin source effects");
        scene->cleanup_from_12fa00();
        check(transition_retail_profile_from_18b0d0(main,0,0,f) && effects.tasks==0,
            "logical stale scene returns before table/effect children");

        std::cout<<"KEYS";
        for(std::uint32_t i=0;i<64;++i) {
            const auto key=retail_profile_entry_key_from_5a2bc0(i,f);
            check(bool(key),"all original64 table records");std::cout<<' '<<*key;
        }
        check(!retail_profile_entry_key_from_5a2bc0(0xffffffffu,f) &&
            f.kind==RetailProfileMenuError::profile_entry_index,"no negative/wrapped table admission");
        std::cout<<"\nPASS actual18B0D0/1892D0, live base selection, source field/call order and native failures; producers open\n";
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"FAIL native profile transition child: %s\n",e.what());return 1;
    }
}
