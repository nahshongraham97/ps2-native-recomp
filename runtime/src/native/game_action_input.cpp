#include "mka/game_action_input.hpp"
#include <algorithm>
#include <stdexcept>
namespace mka::input {
namespace { bool fail(GameActionFailure& f,GameActionError k,const char* op) noexcept { f={k,op};return false; } }
NativeGameActionInput::NativeGameActionInput(std::vector<GameActionParticipant> p,bool inhibit):participants_(std::move(p)),inhibit_(inhibit) {
 states_.reserve(participants_.size()); for(const auto& x:participants_)states_.push_back({x.player,0,0,0,false,false,x.game_assigned,x.dispatch_blocked});
 host_bound_.assign(participants_.size(),true);
}
bool NativeGameActionInput::access(GameActionFailure& f) const noexcept { f={};return owner_==std::this_thread::get_id()||fail(f,GameActionError::wrong_thread,"game_action.owner"); }
std::unique_ptr<NativeGameActionInput> NativeGameActionInput::create(std::vector<GameActionParticipant> p,bool inhibit,GameActionFailure& f) noexcept {
 f={}; if(p.empty()){fail(f,GameActionError::invalid_configuration,"game_action.participants");return {};}
 for(std::size_t i=0;i<p.size();++i){if(p[i].mappings.empty()){fail(f,GameActionError::missing_mapping,"game_action.mapping");return {};}
 for(std::size_t j=0;j<i;++j)if(p[i].player==p[j].player){fail(f,GameActionError::invalid_configuration,"game_action.duplicate");return {};}}
 try{return std::unique_ptr<NativeGameActionInput>(new NativeGameActionInput(std::move(p),inhibit));}
 catch(const std::bad_alloc&){fail(f,GameActionError::allocation,"game_action.create");}
 catch(const std::length_error&){fail(f,GameActionError::allocation,"game_action.create");}return {};
}
bool NativeGameActionInput::mappings_present(std::uint32_t a,GameActionFailure& f) const noexcept {
 for(std::size_t i=0;i<participants_.size();++i)if(host_bound_[i]&&!participants_[i].mappings.contains(a))return fail(f,GameActionError::missing_mapping,"game_action.action");return true;
}
bool NativeGameActionInput::validate_logon_bindings(GameActionFailure& f) const noexcept {
 if(!access(f))return false;for(auto role:{0u,1u}){bool found=false;for(std::size_t i=0;i<participants_.size();++i)if(participants_[i].player==role){found=true;if(!host_bound_[i]||!participants_[i].mappings.contains(11)||!participants_[i].mappings.contains(6))return fail(f,GameActionError::missing_mapping,"game_action.logon_mapping");}if(!found)return fail(f,GameActionError::missing_player,"game_action.logon_role");}return true;
}
bool NativeGameActionInput::poll(const ControllerInput& input,GameActionFailure& f) noexcept {
 if(!access(f))return false;
 try { auto staged=states_;
 for(std::size_t i=0;i<staged.size();++i){auto& s=staged[i];if(!host_bound_[i])continue;const auto sample=input.sample(s.player);if(!sample)return fail(f,GameActionError::missing_player,"game_action.sample");
 s.connected=sample->state.connected;s.active=s.connected;
 if(s.connected){s.previous=s.current;s.current=sample->state.buttons;s.rising=s.current&(s.current^s.previous);}}
 states_.swap(staged);return true;
 }catch(const std::bad_alloc&){return fail(f,GameActionError::allocation,"game_action.poll");}
 catch(const std::length_error&){return fail(f,GameActionError::allocation,"game_action.poll");}
}
std::optional<std::uint32_t> NativeGameActionInput::query_action(std::uint32_t a,GameActionFailure& f) const noexcept {
 if(!access(f)||!mappings_present(a,f))return {};
 for(std::size_t i=0;i<states_.size();++i)if(host_bound_[i]&&states_[i].active&&(states_[i].rising&participants_[i].mappings.find(a)->second))return states_[i].player;
 return {};
}
std::optional<std::uint32_t> NativeGameActionInput::query_ordinary_action(std::uint32_t a,GameActionFailure& f) const noexcept {
 if(!access(f)||!mappings_present(a,f))return {};if(suspended_)return {};
 for(std::size_t i=0;i<states_.size();++i)if(host_bound_[i]&&states_[i].active&&states_[i].game_assigned&&!states_[i].dispatch_blocked&&(states_[i].rising&participants_[i].mappings.find(a)->second))return states_[i].player;return {};
}
std::optional<bool> NativeGameActionInput::poll_logon_skip(const ControllerInput& input,GameActionFailure& f) noexcept {
 if(!validate_logon_bindings(f)||!poll(input,f))return {};
 bool pressed=false;for(auto action:{11u,6u})for(auto role:{0u,1u})for(std::size_t i=0;i<states_.size();++i)if(states_[i].player==role&&states_[i].active&&(states_[i].rising&participants_[i].mappings.find(action)->second))pressed=true;
 if(pressed)intro_skip_=true;return pressed;
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

bool NativeGameActionInput::add_unassigned_record(std::uint32_t role,GameActionFailure& f) noexcept {
 if(!access(f))return false;for(const auto& x:states_)if(x.player==role)return fail(f,GameActionError::invalid_configuration,"game_action.duplicate_role");
 try{auto p=participants_;auto s=states_;auto bound=host_bound_;p.emplace_back(role,std::map<std::uint32_t,std::uint32_t>{},false,false);s.push_back({role});bound.push_back(false);participants_.swap(p);states_.swap(s);host_bound_.swap(bound);return true;}
 catch(const std::bad_alloc&){return fail(f,GameActionError::allocation,"game_action.add_role");}
 catch(const std::length_error&){return fail(f,GameActionError::allocation,"game_action.add_role");}
}
bool NativeGameActionInput::set_game_assigned(std::uint32_t role,bool value,GameActionFailure& f) noexcept {
 if(!access(f))return false;for(auto& s:states_)if(s.player==role){s.game_assigned=value;return true;}return fail(f,GameActionError::missing_player,"game_action.assign");
}
bool NativeGameActionInput::set_dispatch_blocked(std::uint32_t role,bool value,GameActionFailure& f) noexcept {
 if(!access(f))return false;for(auto& s:states_)if(s.player==role){s.dispatch_blocked=value;return true;}return fail(f,GameActionError::missing_player,"game_action.dispatch");
}
bool NativeGameActionInput::set_suspend_inhibit(bool value,GameActionFailure& f) noexcept {if(!access(f))return false;inhibit_=value;return true;}
std::optional<bool> NativeGameActionInput::suspend_inhibited(GameActionFailure& f) const noexcept {if(!access(f))return {};return inhibit_;}
bool NativeGameActionInput::validate_post_movie_targets(GameActionFailure& f) const noexcept {
 if(!access(f))return false;for(auto role:{0u,1u,2u}){bool found=false;for(const auto& s:states_)if(s.player==role)found=true;if(!found)return fail(f,GameActionError::missing_player,"game_action.post_movie_role");}return true;
}
bool NativeGameActionInput::suspend_after_logon(NativeGameInputTasks& tasks,GameActionFailure& f) noexcept {
 if(!validate_post_movie_targets(f))return false;
 if(!tasks.validate_access(f.tasks)){f.kind=GameActionError::task;f.operation="game_action.tasks_owner";return false;}
 if(inhibit_)return true;
 for(auto& s:states_)if(s.player<=2){s.previous=0;s.current=0;s.rising=0;}suspended_=true;
 // No allocation/callback/other-thread mutation can intervene after preflight.
 tasks.write_existing_restore(true,f.tasks);return true;
}
bool NativeGameActionInput::resume_source_inputs(NativeGameInputTasks& tasks,GameActionFailure& f) noexcept {
 if(!validate_post_movie_targets(f))return false;
 if(!tasks.validate_access(f.tasks)){f.kind=GameActionError::task;f.operation="game_action.tasks_owner";return false;}
 suspended_=false;for(auto& s:states_)if(s.player<=2&&s.game_assigned)s.dispatch_blocked=false;
 tasks.write_existing_restore(false,f.tasks);return true;
}
} // namespace mka::input
