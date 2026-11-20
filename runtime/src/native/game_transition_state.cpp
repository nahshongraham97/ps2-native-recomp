#include "mka/game_transition_state.hpp"
#include <stdexcept>
#include <exception>
namespace mka::game {
namespace {bool fail(GameTransitionFailure& f,GameTransitionError k,const char* op) noexcept {f={k,op};return false;}}
NativeGameTransitionState::NativeGameTransitionState(GameTransitionConfiguration c):contexts_(std::move(c.initial_contexts)),main_mode_(c.main_mode),selector_(c.transition_selector),views_(c.active_render_view_count){}
std::unique_ptr<NativeGameTransitionState> NativeGameTransitionState::create(GameTransitionConfiguration c,GameTransitionFailure& f) noexcept {
 f={};try{return std::unique_ptr<NativeGameTransitionState>(new NativeGameTransitionState(std::move(c)));}
 catch(const std::bad_alloc&){fail(f,GameTransitionError::allocation,"transition.create");return {};}
}
NativeGameTransitionState::~NativeGameTransitionState(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();}
bool NativeGameTransitionState::access(GameTransitionFailure& f) const noexcept {f={};if(owner_!=std::this_thread::get_id())return fail(f,GameTransitionError::wrong_thread,"transition.owner");if(busy_)return fail(f,GameTransitionError::reentry,"transition.reentry");return true;}
bool NativeGameTransitionState::reset_context(GameTransitionFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);contexts_.clear();return true;}
bool NativeGameTransitionState::push_context(std::int32_t value,GameTransitionFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);if(!contexts_.empty()&&contexts_.back()==value)return true;
 // Source business history policy: compare old top before clamping.
 // A ninth slot is overwritten on later distinct pushes; host capacity is native.
 if(contexts_.size()>8)contexts_.resize(8);
 try{contexts_.push_back(value);return true;}catch(const std::bad_alloc&){return fail(f,GameTransitionError::allocation,"transition.push");}catch(const std::length_error&){return fail(f,GameTransitionError::allocation,"transition.push");}
}
bool NativeGameTransitionState::pop_context(GameTransitionFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);if(!contexts_.empty())contexts_.pop_back();return true;}
std::optional<std::int32_t> NativeGameTransitionState::current_context(GameTransitionFailure& f) const noexcept {if(!access(f))return {};Guard guard(busy_);return contexts_.empty()?0:contexts_.back();}
std::optional<std::int32_t> NativeGameTransitionState::current_past21(GameTransitionFailure& f) const noexcept {
 if(!access(f))return {};Guard guard(busy_);if(contexts_.empty())return 0;
 for(auto i=contexts_.rbegin();i!=contexts_.rend();++i)if(*i!=21)return *i;
 return 21;
}
bool NativeGameTransitionState::enter_transition(GameTransitionFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);
 try{std::vector<std::int32_t> staged{1};contexts_.swap(staged);return true;}
 catch(const std::bad_alloc&){return fail(f,GameTransitionError::allocation,"transition.reset_push1");}
 catch(const std::length_error&){return fail(f,GameTransitionError::allocation,"transition.reset_push1");}
}
bool NativeGameTransitionState::apply_post_world_transition(GameTransitionFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);if(main_mode_!=7)views_=1;return true;}
bool NativeGameTransitionState::set_main_mode(std::int32_t v,GameTransitionFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);main_mode_=v;return true;}
bool NativeGameTransitionState::set_active_render_view_count(std::uint64_t v,GameTransitionFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);views_=v;return true;}
bool NativeGameTransitionState::set_transition_selector(std::int32_t v,GameTransitionFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);selector_=v;return true;}
std::optional<GameTransitionSnapshot> NativeGameTransitionState::snapshot(GameTransitionFailure& f) const noexcept {
 if(!access(f))return {};Guard guard(busy_);try{return GameTransitionSnapshot{contexts_,main_mode_,views_,selector_};}
 catch(const std::bad_alloc&){fail(f,GameTransitionError::allocation,"transition.snapshot");}
 catch(const std::length_error&){fail(f,GameTransitionError::allocation,"transition.snapshot");}return {};
}
}
