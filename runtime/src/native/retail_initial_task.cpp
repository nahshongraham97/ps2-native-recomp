#include "mka/retail_initial_task.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_participants.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <array>
#include <new>
#include <utility>

namespace mka::retail {
namespace {
[[noreturn]] void fail(RetailInitialTaskError kind,const char* operation) {
    throw RetailInitialTaskException(kind,operation);
}
using MenuFactory=std::shared_ptr<NativeRetailInitialChild>(*)(
    NativeRetailMainState&,NativeRetailGameSession&);
// Exact source table 5A3410, in original slot order. Factories are strong
// unresolved production child symbols, never default-success callbacks.
constexpr std::array<MenuFactory,22> menu_factories{
    initial_task_required::begin_menu_18d450,
    initial_task_required::begin_menu_18d870,
    initial_task_required::begin_menu_18d8e0,
    initial_task_required::begin_menu_18d700,
    initial_task_required::begin_menu_18d800,
    initial_task_required::begin_menu_18d660,
    initial_task_required::begin_menu_18d770,
    initial_task_required::begin_menu_18d8e0,
    initial_task_required::begin_menu_18d700,
    initial_task_required::begin_menu_18d800,
    initial_task_required::begin_menu_18d660,
    initial_task_required::begin_menu_18d770,
    initial_task_required::begin_menu_18d8e0,
    initial_task_required::begin_menu_18d700,
    initial_task_required::begin_menu_18d800,
    initial_task_required::begin_menu_18d660,
    initial_task_required::begin_menu_18d770,
    initial_task_required::begin_menu_18d8e0,
    initial_task_required::begin_menu_18d700,
    initial_task_required::begin_menu_18d800,
    initial_task_required::begin_menu_18d660,
    initial_task_required::begin_menu_18d770
};
NativeRetailInitialTaskState& state_for(
    NativeRetailMainState& main,NativeRetailGameSession& session) {
    RetailGameSessionFailure gf;
    if(!session.validate_current_thread(gf))
        fail(RetailInitialTaskError::wrong_thread,
             "initial task game session owner thread");
    if(main.startup.game_session.get()!=&session)
        fail(RetailInitialTaskError::foreign_session,"initial task Main session identity");
    auto* state=session.initial_task_state(gf);
    if(!state)
        fail(gf.kind==RetailGameSessionError::wrong_thread
            ? RetailInitialTaskError::wrong_thread
            : RetailInitialTaskError::missing_source_owner,
            "initial task persistent source words");
    return *state;
}
void require_state(bool result,const RetailInitialTaskStateFailure& f) {
    if(result) return;
    fail(f.kind==RetailInitialTaskStateError::wrong_thread
        ? RetailInitialTaskError::wrong_thread
        : f.kind==RetailInitialTaskStateError::invalid_menu_index
            ? RetailInitialTaskError::invalid_menu_index
            : RetailInitialTaskError::after_effect_unknown,
        f.operation);
}
}

RetailInitialTaskException::RetailInitialTaskException(
    RetailInitialTaskError kind,const char* operation)
    : std::runtime_error(operation),kind_(kind) {}

NativeRetailInitialTaskBody::NativeRetailInitialTaskBody(
    NativeRetailMainState& main,std::shared_ptr<NativeRetailGameSession> session) noexcept
    : main_(main),main_lifetime_(main.lifetime),session_(session) {}

std::shared_ptr<NativeRetailInitialTaskBody> NativeRetailInitialTaskBody::create(
    NativeRetailMainState& main,std::shared_ptr<NativeRetailGameSession> session,
    RetailInitialTaskFailure& f) noexcept {
    f={};
    if(!session) {
        f={RetailInitialTaskError::missing_session,"initial task game session missing"};
        return {};
    }
    RetailGameSessionFailure sf;
    if(!session->validate_current_thread(sf)) {
        f={RetailInitialTaskError::wrong_thread,"initial task game session owner thread"};
        return {};
    }
    if(!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        f={RetailInitialTaskError::retired_main,"initial task Main lifetime retired"};
        return {};
    }
    if(main.startup.game_session!=session) {
        f={RetailInitialTaskError::foreign_session,"initial task Main session identity"};
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailInitialTaskBody>(
            new NativeRetailInitialTaskBody(main,std::move(session)));
    } catch(const std::bad_alloc&) {
        f={RetailInitialTaskError::allocation,"initial task body allocation"};
    } catch(...) {
        f={RetailInitialTaskError::native_exception,"initial task body construction"};
    }
    return {};
}

NativeRetailInitialTaskState& NativeRetailInitialTaskBody::checked_state() const {
    const auto session=session_.lock();
    if(!session)
        fail(RetailInitialTaskError::missing_session,"initial task game session missing");
    RetailGameSessionFailure sf;
    if(!session->validate_current_thread(sf))
        fail(RetailInitialTaskError::wrong_thread,
             "initial task game session owner thread");
    const auto witness=main_lifetime_.lock();
    if(!witness || !witness->active_on_owner_thread())
        fail(RetailInitialTaskError::retired_main,"initial task Main lifetime retired");
    return state_for(main_,*session);
}

NativeTaskStep NativeRetailInitialTaskBody::run() {
    const auto session=session_.lock();
    if(!session)
        fail(RetailInitialTaskError::missing_session,"initial task game session missing");
    (void)checked_state(); // Admission before reading or changing body state.
    if(after_effect_unknown_)
        fail(RetailInitialTaskError::after_effect_unknown,"initial task prior partial effect");
    try {
        for(;;) switch(phase_) {
        case Phase::begin_reset:
            child_=initial_task_required::begin_reset_18d970(main_,*session);
            if(!child_) fail(RetailInitialTaskError::missing_child,"18D970 retained reset procedure owner");
            phase_=Phase::advance_reset;
            break;
        case Phase::advance_reset: {
            const auto step=child_->advance();
            if(step.kind==RetailInitialChildStep::Kind::wait) {
                if(step.duration!=1.0f)
                    fail(RetailInitialTaskError::invalid_child_step,"12ABD0 source reset wait 1");
                return NativeTaskStep::wait(shared_from_this(),step.duration);
            }
            if(step.kind!=RetailInitialChildStep::Kind::completed)
                fail(RetailInitialTaskError::invalid_child_step,"18D970 reset progress");
            child_.reset();
            phase_=Phase::start_words;
            break;
        }
        case Phase::start_words: {
            RetailInitialTaskStateFailure sf;
            require_state(checked_state().store_start_words_from_18e100(sf),sf);
            phase_=Phase::choose_first_run;
            break;
        }
        case Phase::choose_first_run: {
            RetailInitialTaskStateFailure sf;
            auto words=checked_state().observe(sf);
            require_state(words.has_value(),sf);
            phase_=words->first_run==0 ? Phase::begin_intro : Phase::yield_menu;
            break;
        }
        case Phase::begin_intro:
            child_=initial_task_required::begin_intro_18deb0(main_,*session);
            if(!child_) fail(RetailInitialTaskError::missing_child,"18DEB0 intro procedure owner");
            phase_=Phase::advance_intro;
            break;
        case Phase::advance_intro: {
            const auto step=child_->advance();
            if(step.kind==RetailInitialChildStep::Kind::wait) {
                if(step.duration!=300.0f)
                    fail(RetailInitialTaskError::invalid_child_step,"18DEB0 source wait 300");
                return NativeTaskStep::wait(shared_from_this(),step.duration);
            }
            if(step.kind!=RetailInitialChildStep::Kind::completed)
                fail(RetailInitialTaskError::invalid_child_step,"18DEB0 child progress");
            child_.reset();
            phase_=Phase::begin_prompt;
            break;
        }
        case Phase::begin_prompt:
            child_=initial_task_required::begin_prompt_18dc10(main_,*session);
            if(!child_) fail(RetailInitialTaskError::missing_child,"18DC10 prompt procedure owner");
            phase_=Phase::advance_prompt;
            break;
        case Phase::advance_prompt: {
            const auto step=child_->advance();
            if(step.kind==RetailInitialChildStep::Kind::wait) {
                if(step.duration!=1.0f)
                    fail(RetailInitialTaskError::invalid_child_step,"18DC10 source wait 1");
                return NativeTaskStep::wait(shared_from_this(),step.duration);
            }
            if(step.kind!=RetailInitialChildStep::Kind::completed)
                fail(RetailInitialTaskError::invalid_child_step,"18DC10 child progress");
            child_.reset();
            phase_=Phase::mark_first_run;
            break;
        }
        case Phase::mark_first_run: {
            RetailInitialTaskStateFailure sf;
            require_state(checked_state().store_first_run_from_18e100(sf),sf);
            phase_=Phase::transition_three;
            break;
        }
        case Phase::transition_three:
            initial_task_required::transition_1225d0(
                main_,3,RetailInitialTransitionBody::source_245930);
            phase_=Phase::yield_menu;
            break;
        case Phase::yield_menu: {
            menu_=std::make_shared<NativeRetailMenuContinuation>(main_,session);
            phase_=Phase::yielded;
            return NativeTaskStep::yield(menu_,0.0f);
        }
        case Phase::yielded:
            fail(RetailInitialTaskError::after_effect_unknown,
                "18E100 already replaced by 18DB60 continuation");
        }
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(...) {
        after_effect_unknown_=true;
        throw;
    }
}

void NativeRetailInitialTaskBody::cleanup(bool release_allowed) {
    if(!release_allowed) return; // Source 12EE70 current/flag2 guard.
    (void)checked_state();
    if(child_ || (menu_&&menu_->has_pending_child()))
        fail(RetailInitialTaskError::after_effect_unknown,
            "source child resource owner still active at task retirement");
    menu_.reset(); // Retire completed native continuation/frame ownership.
    // Work27's 12EE70 owner has already invalidated the cookie and retires
    // the native task body/continuation after this callback. No separate
    // 18E100 scene-destroy call exists in the original source.
}

NativeRetailMenuContinuation::NativeRetailMenuContinuation(
    NativeRetailMainState& main,std::shared_ptr<NativeRetailGameSession> session) noexcept
    : main_(main),main_lifetime_(main.lifetime),session_(session) {}
NativeRetailInitialTaskState& NativeRetailMenuContinuation::checked_state() const {
    const auto session=session_.lock();
    if(!session)
        fail(RetailInitialTaskError::missing_session,"menu continuation game session missing");
    RetailGameSessionFailure sf;
    if(!session->validate_current_thread(sf))
        fail(RetailInitialTaskError::wrong_thread,
             "menu continuation game session owner thread");
    const auto witness=main_lifetime_.lock();
    if(!witness || !witness->active_on_owner_thread())
        fail(RetailInitialTaskError::retired_main,"menu continuation Main lifetime retired");
    return state_for(main_,*session);
}
bool NativeRetailMenuContinuation::has_pending_child() const noexcept {
    return bool(child_);
}
NativeTaskStep NativeRetailMenuContinuation::run() {
    const auto session=session_.lock();
    if(!session)
        fail(RetailInitialTaskError::missing_session,"menu continuation game session missing");
    (void)checked_state(); // Admission before reading or changing procedure state.
    if(after_effect_unknown_)
        fail(RetailInitialTaskError::after_effect_unknown,"menu continuation prior partial effect");
    try {
        for(;;) switch(phase_) {
        case Phase::detach_first: {
            if(!main_.participants || !main_.input_assignments || !main_.mode_history)
                fail(RetailInitialTaskError::missing_source_owner,
                    "18DB60 participants, assignment and mode owners");
            RetailParticipantFailure pf;
            first_=main_.participants->first_two(0,pf);
            second_=main_.participants->first_two(1,pf);
            if(!first_ || !second_)
                fail(RetailInitialTaskError::missing_source_owner,"18DB60 first two participants");
            RetailInputAssignmentFailure af;
            if(!main_.input_assignments->detach(first_,af))
                fail(RetailInitialTaskError::after_effect_unknown,"18DB60 first detach");
            phase_=Phase::detach_second;
            break;
        }
        case Phase::detach_second: {
            RetailInputAssignmentFailure af;
            if(!main_.input_assignments->detach(second_,af))
                fail(RetailInitialTaskError::after_effect_unknown,"18DB60 second detach");
            phase_=Phase::clear_first_state;
            break;
        }
        case Phase::clear_first_state: {
            RetailParticipantFailure pf;
            if(!first_->set_game_state(0,pf))
                fail(RetailInitialTaskError::after_effect_unknown,"18DB60 first game state");
            phase_=Phase::clear_second_state;
            break;
        }
        case Phase::clear_second_state: {
            RetailParticipantFailure pf;
            if(!second_->set_game_state(0,pf))
                fail(RetailInitialTaskError::after_effect_unknown,"18DB60 second game state");
            phase_=Phase::push_history;
            break;
        }
        case Phase::push_history: {
            RetailModeFailure mf;
            if(!main_.mode_history || !main_.mode_history->push(3,mf))
                fail(RetailInitialTaskError::after_effect_unknown,"18DB60 push mode history 3");
            phase_=Phase::select_body;
            break;
        }
        case Phase::select_body: {
            RetailInitialTaskStateFailure sf;
            auto selected=checked_state().select_menu_body_from_18db60(sf);
            require_state(selected.has_value(),sf);
            selected_index_=*selected;
            phase_=Phase::begin_body;
            break;
        }
        case Phase::begin_body:
            child_=menu_factories[selected_index_](main_,*session);
            if(!child_) fail(RetailInitialTaskError::missing_child,"18DB60 indexed child owner");
            phase_=Phase::advance_body;
            break;
        case Phase::advance_body: {
            const auto step=child_->advance();
            if(step.kind==RetailInitialChildStep::Kind::wait)
                return NativeTaskStep::wait(shared_from_this(),step.duration);
            if(step.kind!=RetailInitialChildStep::Kind::completed)
                fail(RetailInitialTaskError::invalid_child_step,"18DB60 indexed child progress");
            child_.reset();
            phase_=Phase::transition_zero;
            break;
        }
        case Phase::transition_zero:
            initial_task_required::transition_1225d0(
                main_,0,RetailInitialTransitionBody::menu_18db60);
            phase_=Phase::returned;
            return NativeTaskStep::returned(-1.0f);
        case Phase::returned:
            fail(RetailInitialTaskError::after_effect_unknown,
                "18DB60 already returned through source negative cleanup");
        }
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(...) {
        after_effect_unknown_=true;
        throw;
    }
}
} // namespace mka::retail
