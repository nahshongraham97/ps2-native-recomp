#include "mka/game_action_input.hpp"
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace mka::input;
namespace { void check(bool x){if(!x)std::abort();} }
int main(){
 ControllerInput host; GameActionFailure f;
 const std::map<std::uint32_t,std::uint32_t> explicit_mapping{{11,0x800},{6,0x40}};
 auto game=NativeGameActionInput::create({{0,explicit_mapping,true,false},{1,explicit_mapping,true,false}},false,f);check(bool(game));
 check(game->validate_logon_bindings(f));
 check(!game->poll(host,f)&&f.kind==GameActionError::missing_player);
 check(host.publish(0,digital_state(0,true)));check(host.publish(1,digital_state(0,true)));
 check(game->poll(host,f));check(!game->query_action(11,f)&&f.kind==GameActionError::none);
 check(host.publish(1,digital_state(0x800,true)));check(host.publish(0,digital_state(0x800,true)));
 check(game->set_ordinary_suspended(true,f));
 auto skip=game->poll_logon_skip(host,f);check(skip&&*skip&&game->intro_skip());
 check(game->query_action(11,f)==0);check(!game->query_ordinary_action(11,f)&&f.kind==GameActionError::none);
 skip=game->poll_logon_skip(host,f);check(skip&&!*skip&&game->intro_skip()); // same host sequence still polls
 check(host.publish(0,digital_state(0,true)));check(host.publish(1,digital_state(0,true)));check(game->poll(host,f));
 check(host.publish(0,digital_state(0x40,true)));check(game->poll(host,f));check(game->query_action(6,f)==0);
 check(game->remap(0,{{11,0},{6,0x20}},f));check(!game->query_action(6,f));
 check(host.publish(0,digital_state(0x20,true)));check(game->poll(host,f));check(game->query_action(6,f)==0);
 check(host.publish(0,digital_state(0,false)));check(game->poll(host,f));check(!game->query_action(6,f));
 auto state=game->snapshot(f);check(state&&(*state)[0].current==0x20&&!(*state)[0].active);
 check(host.publish(0,digital_state(0x20,true)));check(game->poll(host,f));check(!game->query_action(6,f));
 auto before=game->snapshot(f);
 ControllerInput incomplete;check(incomplete.publish(0,digital_state(0x800,true)));
 check(!game->poll(incomplete,f)&&f.kind==GameActionError::missing_player);
 auto after=game->snapshot(f);check(after&&before&&after->size()==before->size());
 for(std::size_t i=0;i<after->size();++i){check((*after)[i].previous==(*before)[i].previous&&(*after)[i].current==(*before)[i].current&&(*after)[i].rising==(*before)[i].rising&&(*after)[i].active==(*before)[i].active);}
 check(!game->remap(999,explicit_mapping,f)&&f.kind==GameActionError::missing_player);
 check(!game->query_action(100,f)&&f.kind==GameActionError::missing_mapping);
 auto missing=NativeGameActionInput::create({{0,{{6,1}},false,false}},false,f);check(bool(missing));check(!missing->validate_logon_bindings(f));
 check(!missing->poll_logon_skip(host,f)&&f.kind==GameActionError::missing_mapping);check(missing->snapshot(f)->front().current==0);
 auto owned=game->snapshot(f);game.reset();check(owned&&owned->size()==2&&(*owned)[0].player==0);
 auto dup=NativeGameActionInput::create({{1,explicit_mapping,true,false},{1,explicit_mapping,true,false}},false,f);check(!dup&&f.kind==GameActionError::invalid_configuration);
 auto ordered=NativeGameActionInput::create({{1,explicit_mapping,true,false},{0,explicit_mapping,true,false}},false,f);check(bool(ordered));
 check(host.publish(1,digital_state(0x800,true)));check(host.publish(0,digital_state(0x800,true)));check(ordered->poll(host,f));check(ordered->query_action(11,f)==1);
 bool thread_rejected=false;std::thread t([&]{GameActionFailure other;thread_rejected=!ordered->poll(host,other)&&other.kind==GameActionError::wrong_thread;});t.join();check(thread_rejected);
 check(ordered->reset_intro_skip(f)&&!ordered->intro_skip());
 ControllerInput extra_host;for(auto role:{0u,1u,3u})check(extra_host.publish(role,digital_state(role==3?0x800:0,true)));
 auto extended=NativeGameActionInput::create({{0,explicit_mapping,true,true},{1,explicit_mapping,false,true},{3,explicit_mapping,true,false}},false,f);check(bool(extended));
 check(!extended->validate_post_movie_targets(f));check(extended->add_unassigned_record(2,f));check(extended->validate_post_movie_targets(f));
 auto no_skip=extended->poll_logon_skip(extra_host,f);check(no_skip&&!*no_skip);check(extended->query_action(11,f)==3);
 GameInputTaskFailure tf;auto tasks=NativeGameInputTasks::create(tf);check(bool(tasks));auto task=tasks->create_task(GameInputTaskKind::kind2065,false,tf);check(bool(task));
 check(extra_host.publish(0,digital_state(0x40,true)));check(extended->poll(extra_host,f));check(!extended->query_ordinary_action(6,f));check(extended->set_dispatch_blocked(0,false,f));check(extended->query_ordinary_action(6,f)==0);
 check(extended->set_suspend_inhibit(true,f));check(extended->suspend_after_logon(*tasks,f));check(extended->snapshot(f)->front().current==0x40&&!tasks->get(*task,tf)->saved_restore.value());
 check(extended->set_suspend_inhibit(false,f));check(extended->suspend_after_logon(*tasks,f));check(tasks->get(*task,tf)->saved_restore.value());
 auto cleared=extended->snapshot(f);check(cleared&&(*cleared)[0].current==0&&(*cleared)[0].rising==0&&(*cleared)[2].current==0x800&&(*cleared)[3].current==0);
 check(extended->poll(extra_host,f));check(extended->snapshot(f)->front().rising==0x40);check(extended->suspend_after_logon(*tasks,f));check(extended->snapshot(f)->front().rising==0); // bit6 already set still repeats
 check(extended->set_dispatch_blocked(0,true,f));check(extended->resume_source_inputs(*tasks,f));auto resumed=extended->snapshot(f);check(!(*resumed)[0].dispatch_blocked&&(*resumed)[1].dispatch_blocked&&!tasks->get(*task,tf)->saved_restore.value());
 check(extended->set_game_assigned(1,true,f));check(extended->resume_source_inputs(*tasks,f));check(!extended->snapshot(f)->at(1).dispatch_blocked);
 std::unique_ptr<NativeGameInputTasks> wrong_tasks;std::thread maker([&]{GameInputTaskFailure why;wrong_tasks=NativeGameInputTasks::create(why);});maker.join();
 check(extra_host.publish(0,digital_state(0,true)));check(extra_host.publish(1,digital_state(0,true)));check(extended->poll(extra_host,f));
 check(extra_host.publish(0,digital_state(0x40,true)));check(extra_host.publish(1,digital_state(0x800,true)));check(extended->poll(extra_host,f));
 auto atomic=extended->snapshot(f);check(atomic&&(*atomic)[0].current==0x40&&(*atomic)[0].rising==0x40&&(*atomic)[1].current==0x800&&(*atomic)[1].rising==0x800);
 const auto task_before=tasks->get(*task,tf);check(bool(task_before));
 check(!extended->suspend_after_logon(*wrong_tasks,f)&&f.kind==GameActionError::task&&f.tasks.kind==GameInputTaskError::wrong_thread);
 auto unchanged=extended->snapshot(f);check(unchanged&&unchanged->size()==atomic->size());
 for(std::size_t i=0;i<atomic->size();++i){const auto& a=(*atomic)[i];const auto& b=(*unchanged)[i];check(a.player==b.player&&a.previous==b.previous&&a.current==b.current&&a.rising==b.rising&&a.active==b.active&&a.connected==b.connected&&a.game_assigned==b.game_assigned&&a.dispatch_blocked==b.dispatch_blocked);}
 const auto task_after=tasks->get(*task,tf);check(task_after&&task_after->saved_restore==task_before->saved_restore&&task_after->generation==task_before->generation&&task_after->live==task_before->live);
 check(extra_host.publish(1,digital_state(0,true)));check(extended->poll(extra_host,f));
 check(extended->set_game_assigned(1,false,f));check(extra_host.publish(0,digital_state(0,true)));check(extra_host.publish(1,digital_state(0x800,true)));check(extra_host.publish(3,digital_state(0,true)));check(extended->poll(extra_host,f));check(extended->query_action(11,f)==1&&!extended->query_ordinary_action(11,f));
 std::puts("PASS native game action input ordered mappings/poll edges/LOGON skip; same mask suspension/typed existing task restore; no task execution");
}
