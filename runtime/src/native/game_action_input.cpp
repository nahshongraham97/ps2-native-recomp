#include "mka/game_action_input.hpp"
#include <algorithm>
#include <stdexcept>
namespace mka::input {
namespace { bool fail(GameActionFailure& f,GameActionError k,const char* op) noexcept { f={k,op};return false; } }
NativeGameActionInput::NativeGameActionInput(std::vector<GameActionParticipant> p):participants_(std::move(p)) {
 states_.reserve(participants_.size()); for(const auto& x:participants_)states_.push_back({x.player});
}
bool NativeGameActionInput::access(GameActionFailure& f) const noexcept { f={};return owner_==std::this_thread::get_id()||fail(f,GameActionError::wrong_thread,"game_action.owner"); }
std::unique_ptr<NativeGameActionInput> NativeGameActionInput::create(std::vector<GameActionParticipant> p,GameActionFailure& f) noexcept {
 f={}; if(p.empty()){fail(f,GameActionError::invalid_configuration,"game_action.participants");return {};}
 for(std::size_t i=0;i<p.size();++i){if(p[i].mappings.empty()){fail(f,GameActionError::missing_mapping,"game_action.mapping");return {};}
 for(std::size_t j=0;j<i;++j)if(p[i].player==p[j].player){fail(f,GameActionError::invalid_configuration,"game_action.duplicate");return {};}}
 try{return std::unique_ptr<NativeGameActionInput>(new NativeGameActionInput(std::move(p)));}
 catch(const std::bad_alloc&){fail(f,GameActionError::allocation,"game_action.create");}
 catch(const std::length_error&){fail(f,GameActionError::allocation,"game_action.create");}return {};
}
bool NativeGameActionInput::mappings_present(std::uint32_t a,GameActionFailure& f) const noexcept {
 for(const auto& p:participants_)if(!p.mappings.contains(a))return fail(f,GameActionError::missing_mapping,"game_action.action");return true;
}
bool NativeGameActionInput::validate_logon_bindings(GameActionFailure& f) const noexcept {return access(f)&&mappings_present(11,f)&&mappings_present(6,f);}
bool NativeGameActionInput::poll(const ControllerInput& input,GameActionFailure& f) noexcept {
 if(!access(f))return false;
 try { auto staged=states_;
 for(auto& s:staged){const auto sample=input.sample(s.player);if(!sample)return fail(f,GameActionError::missing_player,"game_action.sample");
 s.connected=sample->state.connected;s.active=s.connected;
 if(s.connected){s.previous=s.current;s.current=sample->state.buttons;s.rising=s.current&(s.current^s.previous);}}
 states_.swap(staged);return true;
 }catch(const std::bad_alloc&){return fail(f,GameActionError::allocation,"game_action.poll");}
 catch(const std::length_error&){return fail(f,GameActionError::allocation,"game_action.poll");}
}
std::optional<std::uint32_t> NativeGameActionInput::query_action(std::uint32_t a,GameActionFailure& f) const noexcept {
 if(!access(f)||!mappings_present(a,f))return {};
 for(std::size_t i=0;i<states_.size();++i)if(states_[i].active&&(states_[i].rising&participants_[i].mappings.find(a)->second))return states_[i].player;
 return {};
}
std::optional<std::uint32_t> NativeGameActionInput::query_ordinary_action(std::uint32_t a,GameActionFailure& f) const noexcept {
 if(!access(f)||!mappings_present(a,f))return {};if(suspended_)return {};return query_action(a,f);
}
std::optional<bool> NativeGameActionInput::poll_logon_skip(const ControllerInput& input,GameActionFailure& f) noexcept {
 if(!access(f)||!mappings_present(11,f)||!mappings_present(6,f)||!poll(input,f))return {};
 const auto first=query_action(11,f);const auto second=query_action(6,f);
 const bool pressed=first.has_value()||second.has_value();if(pressed)intro_skip_=true;return pressed;
}
bool NativeGameActionInput::remap(std::uint32_t p,std::map<std::uint32_t,std::uint32_t> m,GameActionFailure& f) noexcept {
 if(!access(f))return false;if(m.empty())return fail(f,GameActionError::missing_mapping,"game_action.remap");
 for(auto& x:participants_)if(x.player==p){x.mappings.swap(m);return true;}return fail(f,GameActionError::missing_player,"game_action.remap");
}
bool NativeGameActionInput::set_ordinary_suspended(bool b,GameActionFailure& f) noexcept {if(!access(f))return false;suspended_=b;return true;}
bool NativeGameActionInput::reset_intro_skip(GameActionFailure& f) noexcept {if(!access(f))return false;intro_skip_=false;return true;}
std::optional<std::vector<GameActionState>> NativeGameActionInput::snapshot(GameActionFailure& f) const noexcept {
 if(!access(f))return {};try{return states_;}catch(const std::bad_alloc&){fail(f,GameActionError::allocation,"game_action.snapshot");}
 catch(const std::length_error&){fail(f,GameActionError::allocation,"game_action.snapshot");}return {};
}
} // namespace mka::input
