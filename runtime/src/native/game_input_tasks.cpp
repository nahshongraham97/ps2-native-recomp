#include "mka/game_input_tasks.hpp"
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace mka::input {
namespace {
bool fail(GameInputTaskFailure& f,GameInputTaskError k,const char* op) noexcept {f={k,op};return false;}
bool valid(GameInputTaskKind k) noexcept {return k==GameInputTaskKind::kind2065||k==GameInputTaskKind::kind2066||k==GameInputTaskKind::kind208b;}
}
NativeGameInputTasks::NativeGameInputTasks():identity_(std::make_shared<const unsigned char>(0)){}
std::unique_ptr<NativeGameInputTasks> NativeGameInputTasks::create(GameInputTaskFailure& f) noexcept {f={};try{return std::unique_ptr<NativeGameInputTasks>(new NativeGameInputTasks);}
 catch(const std::bad_alloc&){fail(f,GameInputTaskError::allocation,"game_tasks.create");return {};}}
bool NativeGameInputTasks::validate_access(GameInputTaskFailure& f) const noexcept {f={};return thread_==std::this_thread::get_id()||fail(f,GameInputTaskError::wrong_thread,"game_tasks.thread");}
std::optional<GameInputTaskToken> NativeGameInputTasks::create_task(GameInputTaskKind k,std::optional<bool> value,GameInputTaskFailure& f) noexcept {
 if(!validate_access(f))return {};if(!valid(k)){fail(f,GameInputTaskError::invalid_kind,"game_tasks.kind");return {};}
 if(generation_==std::numeric_limits<std::uint64_t>::max()){fail(f,GameInputTaskError::overflow,"game_tasks.generation");return {};}
 try{const auto next=generation_+1;records_.push_back({k,next,true,value});generation_=next;return GameInputTaskToken(identity_,next);}
 catch(const std::bad_alloc&){fail(f,GameInputTaskError::allocation,"game_tasks.record");}
 catch(const std::length_error&){fail(f,GameInputTaskError::allocation,"game_tasks.record");}return {};
}
std::optional<GameInputTaskState> NativeGameInputTasks::get(const GameInputTaskToken& t,GameInputTaskFailure& f) const noexcept {
 if(!validate_access(f))return {};if(t.owner_!=identity_){fail(f,GameInputTaskError::foreign_owner,"game_tasks.token");return {};}
 for(const auto& r:records_)if(r.generation==t.generation_&&r.live)return r;
 fail(f,GameInputTaskError::stale_task,"game_tasks.retired");return {};
}
bool NativeGameInputTasks::retire(const GameInputTaskToken& t,GameInputTaskFailure& f) noexcept {
 static_assert(std::is_nothrow_move_assignable_v<GameInputTaskState>);
 if(!get(t,f))return false;
 for(auto i=records_.begin();i!=records_.end();++i)if(i->generation==t.generation_){records_.erase(i);return true;}return false;
}
std::optional<GameInputTaskState> NativeGameInputTasks::first(GameInputTaskKind k,GameInputTaskFailure& f) const noexcept {
 if(!validate_access(f))return {};if(!valid(k)){fail(f,GameInputTaskError::invalid_kind,"game_tasks.kind");return {};}
 for(const auto& r:records_)if(r.live&&r.kind==k)return r;return {};
}
bool NativeGameInputTasks::write_existing_restore(bool value,GameInputTaskFailure& f) noexcept {
 if(!validate_access(f))return false;
 for(auto k:{GameInputTaskKind::kind2065,GameInputTaskKind::kind2066,GameInputTaskKind::kind208b})
  for(auto& r:records_)if(r.live&&r.kind==k){if(r.saved_restore)r.saved_restore=value;break;}
 return true;
}
}
