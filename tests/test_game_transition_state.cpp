#include "mka/game_transition_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <thread>
using namespace mka::game;
namespace {bool fail_next=false,reentry_rejected=false;NativeGameTransitionState* allocation_probe=nullptr;void check(bool b){if(!b)std::abort();}}
void* operator new(std::size_t n){if(allocation_probe){auto* p=allocation_probe;allocation_probe=nullptr;GameTransitionFailure f;reentry_rejected=!p->current_context(f)&&f.kind==GameTransitionError::reentry;}if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
int main(){GameTransitionFailure f;
 GameTransitionConfiguration config({},7,9,-42);fail_next=true;auto absent=NativeGameTransitionState::create(std::move(config),f);check(!absent&&f.kind==GameTransitionError::allocation);
 auto state=NativeGameTransitionState::create({{},7,9,-42},f);check(bool(state));check(state->current_context(f)==0&&state->current_past21(f)==0);check(state->pop_context(f));
 fail_next=true;check(!state->push_context(3,f)&&f.kind==GameTransitionError::allocation);check(state->current_context(f)==0);
 allocation_probe=state.get();check(state->push_context(3,f)&&reentry_rejected);check(state->push_context(3,f));check(state->snapshot(f)->contexts.size()==1);
 check(state->push_context(21,f));check(state->current_past21(f)==3);check(state->push_context(-9,f));check(state->current_past21(f)==-9);check(state->pop_context(f)&&state->current_context(f)==21);check(state->pop_context(f)&&state->current_context(f)==3);
 check(state->push_context(std::numeric_limits<std::int32_t>::min(),f));check(state->push_context(std::numeric_limits<std::int32_t>::max(),f));
 for(std::int32_t i=0;i<256;++i)check(state->push_context(i,f));check(state->snapshot(f)->contexts==std::vector<std::int32_t>({3,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max(),0,1,2,3,4,255}));
 auto bounded=NativeGameTransitionState::create({{},0,1,0},f);check(bool(bounded));
 for(auto code:{10,20,30,40,50,60,70,80,90})check(bounded->push_context(code,f));
 check(bounded->snapshot(f)->contexts==std::vector<std::int32_t>({10,20,30,40,50,60,70,80,90}));
 check(bounded->push_context(100,f));check(bounded->snapshot(f)->contexts==std::vector<std::int32_t>({10,20,30,40,50,60,70,80,100}));
 check(bounded->pop_context(f)&&bounded->current_context(f)==80);check(bounded->push_context(80,f));check(bounded->snapshot(f)->contexts==std::vector<std::int32_t>({10,20,30,40,50,60,70,80}));
 check(bounded->push_context(110,f)&&bounded->push_context(120,f)&&bounded->push_context(120,f));check(bounded->snapshot(f)->contexts==std::vector<std::int32_t>({10,20,30,40,50,60,70,80,120}));check(bounded->pop_context(f)&&bounded->current_context(f)==80);
 auto deep=NativeGameTransitionState::create({{1,2,3,4,5,6,7,8,9,10,11},0,1,0},f);check(deep&&deep->push_context(11,f)&&deep->snapshot(f)->contexts.size()==11);check(deep->push_context(12,f));check(deep->snapshot(f)->contexts==std::vector<std::int32_t>({1,2,3,4,5,6,7,8,12}));
 const auto before=state->snapshot(f);fail_next=true;check(!state->enter_transition(f)&&f.kind==GameTransitionError::allocation);check(state->snapshot(f)->contexts==before->contexts);
 check(state->enter_transition(f));auto transition=state->snapshot(f);check(transition&&transition->contexts==std::vector<std::int32_t>{1}&&transition->main_mode==7&&transition->active_render_view_count==9&&transition->transition_selector==-42);
 check(state->apply_post_world_transition(f)&&state->snapshot(f)->active_render_view_count==9);
 check(state->set_main_mode(3,f)&&state->set_transition_selector(0,f));check(state->apply_post_world_transition(f));transition=state->snapshot(f);check(transition&&transition->main_mode==3&&transition->contexts==std::vector<std::int32_t>{1}&&transition->active_render_view_count==1&&transition->transition_selector==0);
 check(state->set_active_render_view_count(std::numeric_limits<std::uint64_t>::max(),f));check(state->snapshot(f)->active_render_view_count==std::numeric_limits<std::uint64_t>::max());
 auto all21=NativeGameTransitionState::create({{21,21},0,0,1},f);check(all21&&all21->current_past21(f)==21);check(all21->pop_context(f)&&all21->current_past21(f)==21);check(all21->pop_context(f)&&all21->current_past21(f)==0);check(all21->pop_context(f));
 bool wrong=false;std::thread t([&]{GameTransitionFailure other;wrong=!state->reset_context(other)&&other.kind==GameTransitionError::wrong_thread;});t.join();check(wrong&&state->current_context(f)==1);
 fail_next=true;check(!state->snapshot(f)&&f.kind==GameTransitionError::allocation);check(state->current_context(f)==1);
 auto owned=state->snapshot(f);state.reset();check(owned&&owned->contexts==std::vector<std::int32_t>{1});check(all21->push_context(-1,f));check(all21->reset_context(f)&&all21->current_context(f)==0);
 std::puts("PASS native game transition context/state/allocation/ownership; no world teardown or scheduler execution");
}
