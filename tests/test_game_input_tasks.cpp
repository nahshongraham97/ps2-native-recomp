#include "mka/game_input_tasks.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
namespace {bool fail_next=false;void check(bool b){if(!b)std::abort();}}
void* operator new(std::size_t n){if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
using namespace mka::input;
int main(){GameInputTaskFailure f;
 fail_next=true;auto failed=NativeGameInputTasks::create(f);check(!failed&&f.kind==GameInputTaskError::allocation);
 auto owner=NativeGameInputTasks::create(f);check(bool(owner));check(!owner->first(GameInputTaskKind::kind2065,f)&&f.kind==GameInputTaskError::none);
 fail_next=true;auto rejected=owner->create_task(GameInputTaskKind::kind2065,false,f);check(!rejected&&f.kind==GameInputTaskError::allocation);check(!owner->first(GameInputTaskKind::kind2065,f));
 auto first=owner->create_task(GameInputTaskKind::kind2065,std::nullopt,f);auto second=owner->create_task(GameInputTaskKind::kind2065,false,f);check(first&&second);
 check(owner->write_existing_restore(true,f));check(!owner->get(*first,f)->saved_restore&&owner->get(*second,f)->saved_restore==false); // first task wins even absent userdata
 check(owner->retire(*first,f));check(!owner->get(*first,f)&&f.kind==GameInputTaskError::stale_task);check(owner->write_existing_restore(true,f));check(owner->get(*second,f)->saved_restore==true);
 auto other=NativeGameInputTasks::create(f);check(bool(other));check(!other->retire(*second,f)&&f.kind==GameInputTaskError::foreign_owner);
 auto retained=owner->get(*second,f);owner.reset();check(retained&&retained->live&&retained->saved_restore==true);check(!other->get(*second,f)&&f.kind==GameInputTaskError::foreign_owner);
 auto next=other->create_task(GameInputTaskKind::kind208b,false,f);check(next&&other->write_existing_restore(true,f));check(other->get(*next,f)->saved_restore==true);
 bool thread_rejected=false;std::thread thread([&]{GameInputTaskFailure why;thread_rejected=!other->write_existing_restore(false,why)&&why.kind==GameInputTaskError::wrong_thread;});thread.join();check(thread_rejected&&other->get(*next,f)->saved_restore==true);
 check(other->retire(*next,f));auto fresh=other->create_task(GameInputTaskKind::kind208b,false,f);check(fresh&&other->get(*fresh,f)->generation>retained->generation-1);check(!other->get(*next,f)&&f.kind==GameInputTaskError::stale_task);
 auto last_generation=other->get(*fresh,f)->generation;check(other->retire(*fresh,f));
 for(unsigned i=0;i<4096;++i){auto cycle=other->create_task(GameInputTaskKind::kind2066,false,f);check(bool(cycle));auto live=other->get(*cycle,f);check(live&&live->generation>last_generation);last_generation=live->generation;check(other->retire(*cycle,f));check(!other->get(*cycle,f)&&f.kind==GameInputTaskError::stale_task);check(!other->first(GameInputTaskKind::kind2066,f)&&f.kind==GameInputTaskError::none);}
 check(!other->create_task(static_cast<GameInputTaskKind>(0),false,f)&&f.kind==GameInputTaskError::invalid_kind);
 std::puts("PASS native owned game input task restore/lifetime/first match/allocation; no task body execution");
}
