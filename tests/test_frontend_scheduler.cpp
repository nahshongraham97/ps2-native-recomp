#include "mka/frontend_scheduler.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::game;
namespace {bool fail_next=false;void (*allocation_hook)()=nullptr;void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}}
void* operator new(std::size_t n){if(allocation_hook){auto hook=allocation_hook;allocation_hook=nullptr;hook();}if(fail_next){fail_next=false;throw std::bad_alloc();}if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
struct Trace {std::vector<int> log;std::array<unsigned,64> destroyed{};Trace(){log.reserve(256);}};
using Body=std::function<TaskUpdate(NativeFrontendExecution&,FrontendSchedulerFailure&)>;
struct Task final:NativeFrontendTask {
 std::shared_ptr<Trace> trace;int id;Body body;bool log_destruction;
 Task(std::shared_ptr<Trace> t,int n,Body b,bool d):trace(std::move(t)),id(n),body(std::move(b)),log_destruction(d){}
 ~Task(){++trace->destroyed[static_cast<std::size_t>(id)];if(log_destruction)trace->log.push_back(1000+id);}
 TaskUpdate update(NativeFrontendExecution& e,FrontendSchedulerFailure& f)override{trace->log.push_back(id);return body(e,f);}
};
std::unique_ptr<NativeFrontendTask> job(std::shared_ptr<Trace> t,int id,Body b={},bool destruction=false){
 if(!b)b=[](NativeFrontendExecution&,FrontendSchedulerFailure&){return TaskUpdate::finished;};
 return std::make_unique<Task>(std::move(t),id,std::move(b),destruction);
}
std::unique_ptr<mka::input::NativeGameActionInput> actions(){
 mka::input::GameActionFailure f;
 auto owner=mka::input::NativeGameActionInput::create({{0,{{6,1},{11,2}},false,false},{1,{{6,1},{11,2}},false,false}},false,f);
 check(bool(owner),"actual native action owner");return owner;
}
std::unique_ptr<NativeFrontendScheduler> queue(mka::input::NativeGameActionInput& a,std::uint64_t count=0){
 FrontendSchedulerFailure f;auto q=NativeFrontendScheduler::create(a,count,f);check(bool(q),"actual queue creation");return q;
}
FrontendTaskToken put(NativeFrontendScheduler& q,std::unique_ptr<NativeFrontendTask> task,std::int64_t p,bool override_gate=false){
 FrontendSchedulerFailure f;auto token=q.enqueue(task,p,override_gate,f);check(token&&!task&&f.kind==FrontendSchedulerError::none,"successful exclusive transfer");return *token;
}
void live_insertion(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);FrontendSchedulerFailure f;
 put(*q,job(trace,1,[trace](NativeFrontendExecution& e,FrontendSchedulerFailure& f){
  for(auto [id,p]:{std::pair{2,16},std::pair{3,32},std::pair{4,48}}){auto task=job(trace,id);check(bool(e.enqueue(task,p,false,f))&&!task,"callback native owned enqueue");}
  return TaskUpdate::finished;
 }),32);
 put(*q,job(trace,6),32);put(*q,job(trace,5),64);
 auto r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::pass_complete&&r.updates==5,"same pass live insert count");
 check(trace->log==std::vector<int>({1,6,3,4,5}),"equal stable and after-current same pass; lower waits");
 auto remaining=q->snapshot(f);check(remaining&&remaining->tasks.size()==1&&remaining->tasks[0].priority==16,"before-current remains owned");
 r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::pass_complete&&r.updates==1&&trace->log==std::vector<int>({1,6,3,4,5,2}),"new source pass begins from actual head");
}
void yields_and_fresh_gates(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);FrontendSchedulerFailure f;mka::input::GameActionFailure af;auto calls=std::make_shared<unsigned>(0);
 put(*q,job(trace,1,[calls](NativeFrontendExecution&,FrontendSchedulerFailure&){return ++*calls==1?TaskUpdate::yield:TaskUpdate::finished;}),1);
 put(*q,job(trace,2),2);
 auto r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::pass_complete&&trace->log==std::vector<int>({1,2})&&trace->destroyed[1]==0,"yield retains task for another source pass");
 check(a->set_suspend_inhibit(true,af),"same actual input gate changes");r=q->dispatch(f);check(r.updates==0&&*calls==1,"gated pass does not consume eligible yield");
 check(a->set_suspend_inhibit(false,af),"actual input ungate");r=q->dispatch(f);check(r.updates==1&&trace->destroyed[1]==1&&*calls==2,"yield1 resumes next eligible source pass");
 auto fresh=std::make_shared<Trace>();
 put(*q,job(fresh,3,[&](NativeFrontendExecution&,FrontendSchedulerFailure&){check(a->set_suspend_inhibit(true,af),"gate during earlier callback");return TaskUpdate::finished;}),1);
 put(*q,job(fresh,4),2);
 put(*q,job(fresh,5,[&](NativeFrontendExecution&,FrontendSchedulerFailure&){check(a->set_suspend_inhibit(false,af),"override ungates later jobs");return TaskUpdate::finished;}),3,true);
 put(*q,job(fresh,6),4);
 r=q->dispatch(f);check(fresh->log==std::vector<int>({3,5,6})&&r.updates==3,"fresh source gate per newly selected job");
 r=q->dispatch(f);check(fresh->log==std::vector<int>({3,5,6,4})&&r.updates==1,"earlier skipped task waits next source pass");
 check(q->set_suspension_count(9,f),"explicit native count ingress");put(*q,job(fresh,7),1);put(*q,job(fresh,8),2,true);
 r=q->dispatch(f);check(r.updates==1&&fresh->log.back()==8,"explicit override bypasses nonzero count");
 check(q->set_suspension_count(0,f),"count releases");r=q->dispatch(f);check(r.updates==1&&fresh->log.back()==7,"normal queued owner survives gated count");
}
void blocked_call(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);auto calls=std::make_shared<unsigned>(0);FrontendSchedulerFailure f;mka::input::GameActionFailure af;
 put(*q,job(trace,1,[calls](NativeFrontendExecution&,FrontendSchedulerFailure&){return ++*calls==1?TaskUpdate::blocked:TaskUpdate::finished;}),32);
 put(*q,job(trace,2),64);auto r=q->dispatch(f);
 check(r.progress==FrontendDispatchProgress::blocked&&r.updates==1&&trace->log==std::vector<int>({1}),"blocked logical call runs no other game job");
 auto state=q->snapshot(f);check(state&&state->blocked&&state->pass_active&&state->tasks[0].executing,"blocked owns same current logical pass");
 put(*q,job(trace,3),16);put(*q,job(trace,4),48);
 check(a->set_suspend_inhibit(true,af)&&q->set_suspension_count(5,f),"gate changes while actual call blocked");
 r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::pass_complete&&r.updates==1&&*calls==2&&trace->log==std::vector<int>({1,1}),"already admitted blocked call resumes without new-job gate; tail remains gated");
 check(a->set_suspend_inhibit(false,af)&&q->set_suspension_count(0,f),"next source pass releases gates");
 r=q->dispatch(f);check(r.updates==3&&trace->log==std::vector<int>({1,1,3,4,2}),"blocked cursor preserves before/after insertion ordering");
}
void transition_lifetime(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);FrontendSchedulerFailure f;std::optional<NativeFrontendExecution> retained;
 put(*q,job(trace,10,[&](NativeFrontendExecution& e,FrontendSchedulerFailure& f){
  retained=e;auto flag=e.transition_pending(f);check(flag&&!*flag,"transition initially absent");
  check(e.clear_for_transition(f),"actual pending source resources retired");check(trace->destroyed[20]==1&&trace->destroyed[10]==0,"pending destroyed immediately; executing payload retained");trace->log.push_back(11);
  for(auto [id,p]:{std::pair{16,16},std::pair{32,32}}){auto next=job(trace,id);check(bool(e.enqueue(next,p,false,f)),"actual replacement task owns source-order position");}
  flag=e.transition_pending(f);check(flag&&*flag,"actual clear observed, not guessed handoff");
  return TaskUpdate::transition;
 },true),32);
 put(*q,job(trace,20,{},true),64);
 auto r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::transition&&r.updates==1&&trace->log==std::vector<int>({10,1020,11,1010}),"handoff aborts pass and current destructor follows callback return");
 auto state=q->snapshot(f);check(state&&!state->pass_active&&!state->blocked&&state->tasks.size()==2&&state->tasks[0].priority==16&&state->tasks[1].priority==32,"new16/32 remain owned, never recursively run");
 check(!retained->clear_for_transition(f)&&f.kind==FrontendSchedulerError::stale_execution,"callback capability expires after return");
 r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::pass_complete&&r.updates==2&&trace->log==std::vector<int>({10,1020,11,1010,16,32}),"explicit next source pass executes replacements in priority order");
 q.reset();check(!retained->transition_pending(f)&&f.kind==FrontendSchedulerError::stale_execution,"retained capability rejects safely after owner destruction");
}
void transition_receipts(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);auto other=queue(*a);FrontendSchedulerFailure f;
 auto foreign=put(*other,job(trace,9),16);
 auto stale=put(*q,job(trace,8),16);check(q->cancel(stale,f),"retired receipt setup");
 std::optional<FrontendTaskToken> current;std::optional<NativeFrontendExecution> held;
 current=put(*q,job(trace,1,[&](NativeFrontendExecution& e,FrontendSchedulerFailure& f){
  held=e;FrontendSchedulerFailure local;
  check(!e.validate_transition_task(*current,16,local)&&local.kind==FrontendSchedulerError::invalid_state,"receipt requires actual transition clear");
  check(e.clear_for_transition(f),"receipt actual clear");
  check(!e.validate_transition_task(*current,16,local)&&local.kind==FrontendSchedulerError::invalid_state,"executing task is not replacement maintenance");
  check(!e.validate_transition_task(foreign,16,local)&&local.kind==FrontendSchedulerError::foreign_owner,"foreign pending task cannot satisfy world registration");
  check(!e.validate_transition_task(stale,16,local)&&local.kind==FrontendSchedulerError::stale_task,"retired generation cannot satisfy world registration");
  auto wrong=job(trace,2);auto wrong_token=e.enqueue(wrong,32,false,f);check(bool(wrong_token),"wrong priority actual owned setup");
  check(!e.validate_transition_task(*wrong_token,16,local)&&local.kind==FrontendSchedulerError::invalid_state,"actual owned wrong priority rejected");
  auto maintenance=job(trace,3);auto token=e.enqueue(maintenance,16,false,f);check(bool(token),"actual maintenance registration receipt");
  fail_next=true;const bool valid=e.validate_transition_task(*token,16,local);const bool no_allocation=fail_next;fail_next=false;
  check(valid&&no_allocation&&local.kind==FrontendSchedulerError::none,"authentic pending16 receipt validation allocates nothing");
  bool thread_rejected=false;std::thread thread([&]{FrontendSchedulerFailure why;thread_rejected=!e.validate_transition_task(*token,16,why)&&why.kind==FrontendSchedulerError::wrong_thread;});thread.join();check(thread_rejected,"receipt capability creating thread enforced");
  return TaskUpdate::transition;
 }),16);
 auto result=q->dispatch(f);check(result.progress==FrontendDispatchProgress::transition&&result.updates==1,"handled receipt negatives preserve successful transition");
 check(!held->validate_transition_task(foreign,16,f)&&f.kind==FrontendSchedulerError::stale_execution,"receipt validation cannot outlive callback capability");
 auto state=q->snapshot(f);check(state&&state->tasks.size()==2&&state->tasks[0].priority==16&&state->tasks[1].priority==32,"receipt checks do not consume or rearrange pending tasks");
}
void errors_and_ownership(){
 auto a=actions();FrontendSchedulerFailure f;
 for(int mode=0;mode<5;++mode){auto trace=std::make_shared<Trace>();auto q=queue(*a);
  put(*q,job(trace,1,[mode](NativeFrontendExecution& e,FrontendSchedulerFailure& f){
   if(mode==0){check(e.clear_for_transition(f),"clear before bad return");return TaskUpdate::yield;}
   if(mode==1)return TaskUpdate::transition;
   if(mode==2){f.kind=FrontendSchedulerError::invalid_result;f.operation="actual task source failure";f.task_code=72;return TaskUpdate::error;}
   if(mode==3)return static_cast<TaskUpdate>(99);
   throw std::runtime_error("actual authored task exception");
  }),1);
  auto r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::error,"malformed/error/exception cannot continue source pass");
  if(mode==2)check(f.kind==FrontendSchedulerError::invalid_result&&f.task_code==72&&std::string_view(f.operation)=="actual task source failure","original task error preserved exactly");
  else check(f.kind==(mode==4?FrontendSchedulerError::task_exception:FrontendSchedulerError::invalid_result),"named malformed task result");
  auto failed_state=q->snapshot(f);check(failed_state&&failed_state->failed,"terminal error state observed without retry");r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::error&&f.kind==FrontendSchedulerError::invalid_state,"failed task never retried");
 }
 auto trace=std::make_shared<Trace>();auto q=queue(*a);auto token=put(*q,job(trace,2),1);auto other=queue(*a);
 check(!other->cancel(token,f)&&f.kind==FrontendSchedulerError::foreign_owner,"native token belongs to actual owner");
 check(q->cancel(token,f)&&trace->destroyed[2]==1,"cancel releases actual resource");
 check(!q->cancel(token,f)&&f.kind==FrontendSchedulerError::stale_task,"retired token cannot act again");
 auto replacement=put(*q,job(trace,3),1);check(!q->cancel(token,f)&&f.kind==FrontendSchedulerError::stale_task,"generation never aliases replacement");
 bool rejected=false;std::thread t([&]{FrontendSchedulerFailure why;rejected=q->dispatch(why).progress==FrontendDispatchProgress::error&&why.kind==FrontendSchedulerError::wrong_thread;});t.join();check(rejected,"foreign thread rejected without task execution");
 check(q->cancel(replacement,f),"owner intact after foreign rejection");
 auto held=put(*q,job(trace,4),1);auto snapshot=q->snapshot(f);q.reset();check(snapshot&&snapshot->tasks.size()==1&&trace->destroyed[4]==1,"owned metadata and token survive job/queue destruction");
 check(!other->cancel(held,f)&&f.kind==FrontendSchedulerError::foreign_owner,"destroyed owner token cannot target another owner");
}
void dynamic_owned_count(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);FrontendSchedulerFailure f;
 for(unsigned i=0;i<512;++i)put(*q,job(trace,9),static_cast<std::int64_t>(i)-256);
 auto held=q->snapshot(f);check(held&&held->tasks.size()==512,"dynamic owned jobs have no inherited slot cap");
 auto result=q->dispatch(f);check(result.progress==FrontendDispatchProgress::pass_complete&&result.updates==512&&trace->log.size()==512&&trace->destroyed[9]==512,"all dynamic owned tasks execute and retire");
}
NativeFrontendExecution* allocation_execution=nullptr;
std::unique_ptr<NativeFrontendTask>* allocation_candidate=nullptr;
bool allocation_rejected=false;
void probe_allocator_reentry(){
 FrontendSchedulerFailure failure;
 auto token=allocation_execution->enqueue(*allocation_candidate,64,false,failure);
 allocation_rejected=!token&&bool(*allocation_candidate)&&failure.kind==FrontendSchedulerError::invalid_state;
}
void allocator_capability_reentry(){
 auto a=actions();auto trace=std::make_shared<Trace>();auto q=queue(*a);FrontendSchedulerFailure f;
 auto nested=job(trace,3);std::optional<FrontendTaskToken> outer_token,nested_token;
 auto running=put(*q,job(trace,1,[&](NativeFrontendExecution& e,FrontendSchedulerFailure& f){
  auto outer=job(trace,2);
  allocation_execution=&e;allocation_candidate=&nested;allocation_rejected=false;
  allocation_hook=&probe_allocator_reentry;
  outer_token=e.enqueue(outer,32,false,f);
  allocation_hook=nullptr;allocation_execution=nullptr;allocation_candidate=nullptr;
  check(outer_token&&!outer&&allocation_rejected&&nested&&trace->destroyed[3]==0,"allocator hook cannot steal nested candidate or publish duplicate generation");
  nested_token=e.enqueue(nested,64,false,f);
  check(nested_token&&!nested,"outer mutation scope closes before next legitimate enqueue");
  return TaskUpdate::blocked;
 }),16);
 auto result=q->dispatch(f);check(result.progress==FrontendDispatchProgress::blocked&&result.updates==1&&f.kind==FrontendSchedulerError::none,"rejected allocator reentry does not poison actual outer commit");
 auto state=q->snapshot(f);check(state&&state->tasks.size()==3,"exact three distinct actual owned tasks after guarded commit");
 check(q->cancel(*outer_token,f)&&trace->destroyed[2]==1&&trace->destroyed[3]==0,"outer token retires ONLY its resource");
 check(!q->cancel(*outer_token,f)&&f.kind==FrontendSchedulerError::stale_task&&trace->destroyed[3]==0,"retired outer generation cannot alias next task");
 check(q->cancel(*nested_token,f)&&trace->destroyed[3]==1,"next legitimate generation independently owns its resource");
 check(q->cancel(running,f)&&trace->destroyed[1]==1,"blocked executing owner remains valid for cancellation");
}
void reentry_and_allocation(){
 auto a=actions();FrontendSchedulerFailure f;fail_next=true;auto failed=NativeFrontendScheduler::create(*a,0,f);check(!failed&&f.kind==FrontendSchedulerError::allocation,"real queue allocation failure");
 auto trace=std::make_shared<Trace>();auto q=queue(*a);std::unique_ptr<NativeFrontendTask> missing;check(!q->enqueue(missing,1,false,f)&&f.kind==FrontendSchedulerError::missing_task,"missing task is not a successful no-op");auto candidate=job(trace,1);fail_next=true;auto absent=q->enqueue(candidate,1,false,f);check(!absent&&candidate&&trace->destroyed[1]==0&&f.kind==FrontendSchedulerError::allocation,"failed enqueue retains caller resource");
 auto empty=q->snapshot(f);check(empty&&empty->tasks.empty(),"failed insertion publishes no task");auto token=q->enqueue(candidate,1,false,f);check(token&&!candidate,"actual owner can accept after recoverable allocation error");
 fail_next=true;auto no_snapshot=q->snapshot(f);check(!no_snapshot&&f.kind==FrontendSchedulerError::allocation,"owned snapshot allocation failure is named");
 auto preserved=q->snapshot(f);check(preserved&&preserved->tasks.size()==1&&q->cancel(*token,f),"snapshot failure changes no task owner");
 std::optional<NativeFrontendExecution> retained;
 put(*q,job(trace,2,[&](NativeFrontendExecution& e,FrontendSchedulerFailure&){
  retained=e;FrontendSchedulerFailure local;check(q->dispatch(local).progress==FrontendDispatchProgress::error&&local.kind==FrontendSchedulerError::invalid_state,"callback cannot recursively dispatch");
  auto extra=job(trace,3,[&](NativeFrontendExecution&,FrontendSchedulerFailure&){FrontendSchedulerFailure old;check(!retained->set_suspension_count(99,old)&&old.kind==FrontendSchedulerError::stale_execution,"old capability cannot act during a different live callback");return TaskUpdate::finished;});check(!q->enqueue(extra,2,false,local)&&extra&&local.kind==FrontendSchedulerError::invalid_state,"public reentry retains supplied task");
  check(bool(e.enqueue(extra,2,false,local))&&!extra,"issued execution capability allows intended mutation");return TaskUpdate::finished;
 }),1);
 auto r=q->dispatch(f);check(r.progress==FrontendDispatchProgress::pass_complete&&r.updates==2,"handled reentry rejection does not poison outer task");
 auto extra=job(trace,4);check(!retained->enqueue(extra,1,false,f)&&extra&&f.kind==FrontendSchedulerError::stale_execution,"stale callback cannot steal caller task");
}
}
int main(){try{
 live_insertion();yields_and_fresh_gates();blocked_call();transition_lifetime();transition_receipts();errors_and_ownership();dynamic_owned_count();allocator_capability_reentry();reentry_and_allocation();
 check(std::puts("PASS native frontend owned queue/live ordering/blocked handoff/lifetime; no game scheduler bootstrap or hardware tick")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){fail_next=false;allocation_hook=nullptr;std::fprintf(stderr,"FAIL native frontend scheduler %s\n",e.what());return 1;}}
