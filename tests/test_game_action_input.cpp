#include "mka/game_action_input.hpp"
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace mka::input;
namespace { void check(bool x){if(!x)std::abort();} }
int main(){
 ControllerInput host; GameActionFailure f;
 const std::map<std::uint32_t,std::uint32_t> explicit_mapping{{11,0x800},{6,0x40}};
 auto game=NativeGameActionInput::create({{7,explicit_mapping},{2,explicit_mapping}},f);check(bool(game));
 check(game->validate_logon_bindings(f));
 check(!game->poll(host,f)&&f.kind==GameActionError::missing_player);
 check(host.publish(7,digital_state(0,true)));check(host.publish(2,digital_state(0,true)));
 check(game->poll(host,f));check(!game->query_action(11,f)&&f.kind==GameActionError::none);
 check(host.publish(2,digital_state(0x800,true)));check(host.publish(7,digital_state(0x800,true)));
 check(game->set_ordinary_suspended(true,f));
 auto skip=game->poll_logon_skip(host,f);check(skip&&*skip&&game->intro_skip());
 check(game->query_action(11,f)==7);check(!game->query_ordinary_action(11,f)&&f.kind==GameActionError::none);
 skip=game->poll_logon_skip(host,f);check(skip&&!*skip&&game->intro_skip()); // same host sequence still polls
 check(host.publish(7,digital_state(0,true)));check(host.publish(2,digital_state(0,true)));check(game->poll(host,f));
 check(host.publish(7,digital_state(0x40,true)));check(game->poll(host,f));check(game->query_action(6,f)==7);
 check(game->remap(7,{{11,0},{6,0x20}},f));check(!game->query_action(6,f));
 check(host.publish(7,digital_state(0x20,true)));check(game->poll(host,f));check(game->query_action(6,f)==7);
 check(host.publish(7,digital_state(0,false)));check(game->poll(host,f));check(!game->query_action(6,f));
 auto state=game->snapshot(f);check(state&&(*state)[0].current==0x20&&!(*state)[0].active);
 check(host.publish(7,digital_state(0x20,true)));check(game->poll(host,f));check(!game->query_action(6,f));
 auto before=game->snapshot(f);
 ControllerInput incomplete;check(incomplete.publish(7,digital_state(0x800,true)));
 check(!game->poll(incomplete,f)&&f.kind==GameActionError::missing_player);
 auto after=game->snapshot(f);check(after&&before&&after->size()==before->size());
 for(std::size_t i=0;i<after->size();++i){check((*after)[i].previous==(*before)[i].previous&&(*after)[i].current==(*before)[i].current&&(*after)[i].rising==(*before)[i].rising&&(*after)[i].active==(*before)[i].active);}
 check(!game->remap(999,explicit_mapping,f)&&f.kind==GameActionError::missing_player);
 check(!game->query_action(100,f)&&f.kind==GameActionError::missing_mapping);
 auto missing=NativeGameActionInput::create({{7,{{6,1}}}},f);check(bool(missing));check(!missing->validate_logon_bindings(f));
 check(!missing->poll_logon_skip(host,f)&&f.kind==GameActionError::missing_mapping);check(missing->snapshot(f)->front().current==0);
 auto owned=game->snapshot(f);game.reset();check(owned&&owned->size()==2&&(*owned)[0].player==7);
 auto dup=NativeGameActionInput::create({{2,explicit_mapping},{2,explicit_mapping}},f);check(!dup&&f.kind==GameActionError::invalid_configuration);
 auto ordered=NativeGameActionInput::create({{2,explicit_mapping},{7,explicit_mapping}},f);check(bool(ordered));
 check(host.publish(2,digital_state(0x800,true)));check(host.publish(7,digital_state(0x800,true)));check(ordered->poll(host,f));check(ordered->query_action(11,f)==2);
 bool thread_rejected=false;std::thread t([&]{GameActionFailure other;thread_rejected=!ordered->poll(host,other)&&other.kind==GameActionError::wrong_thread;});t.join();check(thread_rejected);
 check(ordered->reset_intro_skip(f)&&!ordered->intro_skip());
 std::puts("PASS native game action input ordered mappings/poll edges/LOGON skip; no guest ABI or task suspension notifications");
}
