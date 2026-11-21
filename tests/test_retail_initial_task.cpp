#include "mka/retail_initial_task.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_participants.hpp"
#include "mka/retail_task_execution.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
void check(bool good,const char* message) {
    if(!good) throw std::runtime_error(message);
}
struct Fixture {
    std::vector<std::string> calls;
    NativeRetailGameSession* session{};
    bool transition_zero_transfers{};
    bool empty_intro_result{};
    unsigned resets{},intro_begins{},prompt_begins{},menu_begins{};
    std::weak_ptr<NativeRetailInitialChild> intro_lifetime,prompt_lifetime,menu_lifetime;
};
Fixture* fixture{};
struct Child final : NativeRetailInitialChild {
    Child(const char* name,float wait,bool must_wait,bool return_empty=false)
        : name(name),wait(wait),must_wait(must_wait),return_empty(return_empty) {}
    RetailInitialChildStep advance() override {
        fixture->calls.emplace_back(std::string(name)+".advance");
        if(return_empty) { return_empty=false; return {}; }
        if(must_wait&&!waited) {
            waited=true;
            return RetailInitialChildStep::wait(wait);
        }
        return RetailInitialChildStep::completed();
    }
    const char* name;
    float wait{};
    bool must_wait{},waited{},return_empty{};
};
std::shared_ptr<NativeRetailInitialChild> menu(const char* name,bool wait=false) {
    ++fixture->menu_begins;
    fixture->calls.emplace_back(std::string(name)+".begin");
    RetailGameSessionFailure gf;
    RetailInitialTaskStateFailure sf;
    auto* state=fixture->session->initial_task_state(gf);
    auto words=state->observe(sf);
    check(words&&words->menu_index>=1,"menu index is stored before selected child starts");
    auto child=std::make_shared<Child>(name,1.0f,wait);
    fixture->menu_lifetime=child;
    return child;
}
std::size_t call_index(const std::vector<std::string>& calls,const char* name) {
    const auto it=std::find(calls.begin(),calls.end(),name);
    check(it!=calls.end(),"required child call missing");
    return static_cast<std::size_t>(it-calls.begin());
}
void install_known_owners(NativeRetailMainState& main) {
    RetailParticipantFailure pf;
    RetailInputRoleFailure rf;
    RetailInputAssignmentFailure af;
    main.participants=NativeRetailParticipants::create_first_two(pf);
    main.input_roles=NativeRetailInputRoles::create(rf);
    check(main.participants&&main.input_roles,"real first two participants and roles");
    main.input_assignments=NativeRetailInputAssignments::create(
        main.input_roles,main.participants,af);
    check(bool(main.input_assignments),"real shared participant/role assignment owner");
    main.mode_history.emplace();
}
}

// Explicit fixture-only child implementations. The production source has no
// definitions for these symbols and therefore cannot link a fake game path.
namespace mka::retail::initial_task_required {
void run_reset_18d970(NativeRetailMainState&,NativeRetailGameSession&) {
    ++fixture->resets;fixture->calls.emplace_back("reset");
}
std::shared_ptr<NativeRetailInitialChild> begin_intro_18deb0(
    NativeRetailMainState&,NativeRetailGameSession&) {
    ++fixture->intro_begins;fixture->calls.emplace_back("intro.begin");
    auto child=std::make_shared<Child>("intro",300.0f,true,fixture->empty_intro_result);
    fixture->intro_lifetime=child;
    return child;
}
std::shared_ptr<NativeRetailInitialChild> begin_prompt_18dc10(
    NativeRetailMainState&,NativeRetailGameSession&) {
    ++fixture->prompt_begins;fixture->calls.emplace_back("prompt.begin");
    auto child=std::make_shared<Child>("prompt",1.0f,true);
    fixture->prompt_lifetime=child;
    return child;
}
void transition_1225d0(NativeRetailMainState& main,std::uint32_t mode,
    RetailInitialTransitionBody next) {
    fixture->calls.emplace_back(mode==3?"transition.3":"transition.0");
    check((mode==3&&next==RetailInitialTransitionBody::source_245930)||
          (mode==0&&next==RetailInitialTransitionBody::menu_18db60),
          "typed transition body identity");
    if(mode==0&&fixture->transition_zero_transfers)
        throw detail::NativeMainReentry{&main,7};
}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d450(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D450",true);}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d870(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D870");}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d8e0(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D8E0");}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d700(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D700");}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d800(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D800");}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d660(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D660");}
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d770(
    NativeRetailMainState&,NativeRetailGameSession&) {return menu("18D770");}
} // namespace mka::retail::initial_task_required

int main(int argc,char** argv) {try {
    check(argc==2,"fixture requires disposable content directory");
    mka::content::ContentVolumeFailure vf;
    auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),vf);
    check(volume.has_value(),"actual CPU/file content root opened");
    NativeRetailStartupState startup(std::move(*volume),{});
    RetailGameSessionFailure gf;
    startup.game_session=NativeRetailGameSession::create(gf);
    check(bool(startup.game_session),"actual persistent session created");
    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup,samples);
    install_known_owners(main);
    Fixture first;
    first.session=startup.game_session.get();fixture=&first;
    RetailInitialTaskFailure tf;
    auto body=NativeRetailInitialTaskBody::create(main,startup.game_session,tf);
    check(body&&tf.kind==RetailInitialTaskError::none,"same Main/session initial body factory");
    auto foreign_session=NativeRetailGameSession::create(gf);
    check(!NativeRetailInitialTaskBody::create(main,foreign_session,tf)&&
          tf.kind==RetailInitialTaskError::foreign_session,
          "creator refuses a separate persistent word owner");
    bool wrong_thread=false;
    std::thread other([&] {
        RetailInitialTaskFailure other_failure;
        wrong_thread=!NativeRetailInitialTaskBody::create(
            main,startup.game_session,other_failure)&&
            other_failure.kind==RetailInitialTaskError::wrong_thread;
    });
    other.join();
    check(wrong_thread,"creator refuses foreign thread before publication");
    bool direct_run_refused=false;
    std::thread direct_other([&] {
        try { (void)body->run(); }
        catch(const RetailInitialTaskException& e) {
            direct_run_refused=e.kind()==RetailInitialTaskError::wrong_thread;
        }
    });
    direct_other.join();
    check(direct_run_refused&&first.calls.empty(),
          "direct parent run refuses wrong thread before Main read or child effects");
    NativeRetailMenuContinuation null_menu(main,{});
    bool missing_menu_session=false;
    try { (void)null_menu.run(); }
    catch(const RetailInitialTaskException& e) {
        missing_menu_session=e.kind()==RetailInitialTaskError::missing_session;
    }
    check(missing_menu_session&&first.calls.empty(),
          "public menu continuation refuses absent session before dereference or effects");
    auto saved_session=startup.game_session;
    startup.game_session.reset();
    body->cleanup(false);
    startup.game_session=std::move(saved_session);
    check(first.calls.empty(),
          "guarded source cleanup performs no identity validation or game effects");
    RetailTaskRegistryFailure rf;
    auto registry=NativeRetailTaskRegistry::create(rf);
    auto task=registry->publish_task(0x2001,0x20,17,body,rf); // CPU-only source-cookie input.
    check(bool(task),"real typed task published with explicit fixture cookie");
    NativeRetailTaskExecution execution(*registry);
    RetailTaskExecutionFailure ef;
    RetailInitialTaskStateFailure sf;
    auto* state=startup.game_session->initial_task_state(gf);
    auto one=execution.invoke_virtual_10_initial(task,ef);
    auto words=state->observe(sf);
    check(one&&one->kind==NativeTaskRunKind::retained_wait&&one->stored_wait==300.0f&&
          first.resets==1&&first.intro_begins==1&&first.prompt_begins==0&&
          words&&words->game_word_660094==0x38&&words->cycle_word_6602ec==0&&
          words->first_run==0&&!first.intro_lifetime.expired(),
          "valid owner still runs after foreign refusal; intro locals survive first +14 wait");
    auto two=execution.invoke_virtual_10_initial(task,ef);
    check(two&&two->kind==NativeTaskRunKind::retained_wait&&two->stored_wait==1.0f&&
          first.resets==1&&first.intro_begins==1&&first.prompt_begins==1&&
          first.intro_lifetime.expired()&&!first.prompt_lifetime.expired()&&
          state->observe(sf)->first_run==0,
          "resumed intro completes before prompt begins; prompt locals retain across wait");
    auto three=execution.invoke_virtual_10_initial(task,ef);
    check(three&&three->kind==NativeTaskRunKind::retained_wait&&
          three->stored_wait==1.0f&&three->immediate_calls==2&&
          first.prompt_lifetime.expired()&&!first.menu_lifetime.expired()&&
          state->observe(sf)->first_run==1&&state->observe(sf)->menu_index==1&&
          first.menu_begins==1,
          "first-run mark and mode3 transition precede immediate 18DB60 and indexed child wait");
    check(call_index(first.calls,"transition.3")<call_index(first.calls,"18D450.begin")&&
          call_index(first.calls,"intro.advance")<call_index(first.calls,"prompt.begin"),
          "original parent, nested child, and immediate continuation ordering");
    auto four=execution.invoke_virtual_10_initial(task,ef);
    check(four&&four->kind==NativeTaskRunKind::cleaned_negative&&
          first.menu_lifetime.expired()&&
          call_index(first.calls,"18D450.advance")<call_index(first.calls,"transition.0")&&
          main.mode_history->top()==3,
          "indexed child resumes same locals, transition normally returns -1 for source cleanup");
    auto found=registry->find_first_live(0x2001,rf);
    check(found&&!*found&&
          std::count(first.calls.begin(),first.calls.end(),"transition.0")==1,
          "later stale-link recycle does not replay task effects");

    Fixture later;
    later.session=startup.game_session.get();fixture=&later;
    check(state->store_menu_index_from_source(22,sf),"source menu index writer");
    auto later_body=NativeRetailInitialTaskBody::create(main,startup.game_session,tf);
    auto later_registry=NativeRetailTaskRegistry::create(rf);
    auto later_task=later_registry->publish_task(0x2001,0x20,19,later_body,rf);
    NativeRetailTaskExecution later_execution(*later_registry);
    auto later_result=later_execution.invoke_virtual_10_initial(later_task,ef);
    auto later_words=state->observe(sf);
    check(later_result&&later_result->kind==NativeTaskRunKind::cleaned_negative&&
          later.resets==1&&later.intro_begins==0&&later.prompt_begins==0&&
          later.menu_begins==1&&later_words&&later_words->first_run==1&&
          later_words->cycle_word_6602ec==1&&later_words->menu_index==2&&
          call_index(later.calls,"18D870.begin")<call_index(later.calls,"transition.0"),
          "persistent first-run skip and signed >=22 wrap choose exact slot1 before child call");

    Fixture transfer;
    transfer.session=startup.game_session.get();transfer.transition_zero_transfers=true;
    fixture=&transfer;
    auto transfer_body=NativeRetailInitialTaskBody::create(main,startup.game_session,tf);
    auto transfer_registry=NativeRetailTaskRegistry::create(rf);
    auto transfer_task=transfer_registry->publish_task(0x2001,0x20,21,transfer_body,rf);
    NativeRetailTaskExecution transfer_execution(*transfer_registry);
    bool propagated=false;
    try { (void)transfer_execution.invoke_virtual_10_initial(transfer_task,ef); }
    catch(const detail::NativeMainReentry& event) {
        propagated=event.owner==&main&&event.value==7;
    }
    check(propagated&&
          transfer_registry->find_first_live(0x2001,rf).has_value()&&
          state->observe(sf)->menu_index==3,
          "mode transition Main transfer propagates after indexed body and prepublished index");
    auto another_body=NativeRetailInitialTaskBody::create(main,startup.game_session,tf);
    auto another_task=transfer_registry->publish_task(0x2001,0x20,23,another_body,rf);
    check(another_task&&rf.kind==RetailTaskRegistryError::none,
          "typed Main transfer does not poison unrelated task publication");

    auto original_session=startup.game_session;
    startup.game_session=NativeRetailGameSession::create(gf);
    check(bool(startup.game_session),"fresh actual session for malformed child result");
    Fixture malformed;
    malformed.session=startup.game_session.get();
    malformed.empty_intro_result=true;
    fixture=&malformed;
    auto malformed_body=NativeRetailInitialTaskBody::create(main,startup.game_session,tf);
    auto malformed_registry=NativeRetailTaskRegistry::create(rf);
    auto malformed_task=malformed_registry->publish_task(0x2001,0x20,25,malformed_body,rf);
    NativeRetailTaskExecution malformed_execution(*malformed_registry);
    auto malformed_result=malformed_execution.invoke_virtual_10_initial(malformed_task,ef);
    RetailInitialTaskStateFailure malformed_failure;
    auto malformed_words=startup.game_session->initial_task_state(gf)->observe(malformed_failure);
    check(!malformed_result&&ef.kind==RetailTaskExecutionError::native_exception&&
          malformed_words&&malformed_words->first_run==0&&
          std::find(malformed.calls.begin(),malformed.calls.end(),"transition.3")==malformed.calls.end(),
          "uninitialized child result fails by name before first-run mark or transition");
    const auto calls_before_retry=malformed.calls.size();
    bool latched=false;
    try { (void)malformed_body->run(); }
    catch(const RetailInitialTaskException& e) {
        latched=e.kind()==RetailInitialTaskError::after_effect_unknown;
    }
    check(latched&&malformed.calls.size()==calls_before_retry&&malformed.resets==1,
          "after-effect failure cannot replay reset or malformed child");
    startup.game_session=std::move(original_session);
    const auto calls_before_retirement=malformed.calls.size();
    main.lifetime->retire();
    bool retired_run=false, retired_cleanup=false;
    try { (void)body->run(); }
    catch(const RetailInitialTaskException& e) {
        retired_run=e.kind()==RetailInitialTaskError::retired_main;
    }
    try { body->cleanup(true); }
    catch(const RetailInitialTaskException& e) {
        retired_cleanup=e.kind()==RetailInitialTaskError::retired_main;
    }
    body->cleanup(false);
    check(retired_run&&retired_cleanup&&
          malformed.calls.size()==calls_before_retirement,
          "retired Main witness refuses body access before borrowed Main read; guarded cleanup remains immediate");
    std::printf("PASS native initial task parent and menu continuation; test-only child effects, full Main/game open\n");
    return 0;
} catch(const std::exception& e) {
    std::fprintf(stderr,"FAIL native initial task: %s\n",e.what());return 1;
}}
