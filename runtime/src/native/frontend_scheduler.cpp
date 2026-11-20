#include "mka/frontend_scheduler.hpp"
#include <exception>
#include <iterator>
#include <limits>
#include <list>
#include <new>
#include <stdexcept>
#include <thread>
namespace mka::game {
namespace {
bool fail(FrontendSchedulerFailure& f,FrontendSchedulerError kind,const char* op) noexcept {f.kind=kind;f.operation=op;return false;}
struct Guard {bool& value;explicit Guard(bool& b):value(b){value=true;}~Guard(){value=false;}};
}
struct FrontendExecutionControl {
 NativeFrontendScheduler* owner=nullptr;
 const std::thread::id thread=std::this_thread::get_id();
 std::uint64_t generation=0;
 bool active=false;
};
struct NativeFrontendScheduler::Impl {
 struct Node {
  std::unique_ptr<NativeFrontendTask> task;
  std::int64_t priority;
  bool override_gate;
  std::uint64_t generation;
  Node(std::int64_t p,bool o,std::uint64_t g):priority(p),override_gate(o),generation(g){}
 };
 input::NativeGameActionInput& actions;
 std::list<Node> tasks;
 std::list<Node>::iterator cursor=tasks.end();
 std::shared_ptr<FrontendExecutionControl> control=std::make_shared<FrontendExecutionControl>();
 std::uint64_t suspension_count,generation=0,execution_generation=0;
 bool in_call=false,mutating=false,pass_active=false,blocked=false,transition_pending=false,failed=false;
 explicit Impl(input::NativeGameActionInput& a,std::uint64_t count):actions(a),suspension_count(count){}
};
NativeFrontendScheduler::NativeFrontendScheduler(std::unique_ptr<Impl> p):impl_(std::move(p)){impl_->control->owner=this;}
NativeFrontendScheduler::~NativeFrontendScheduler(){
 if(std::this_thread::get_id()!=impl_->control->thread||impl_->in_call)std::terminate();
 impl_->in_call=true;impl_->mutating=true;impl_->control->active=false;impl_->control->owner=nullptr;
 impl_->tasks.clear();
}
std::unique_ptr<NativeFrontendScheduler> NativeFrontendScheduler::create(input::NativeGameActionInput& actions,std::uint64_t count,FrontendSchedulerFailure& f) noexcept {
 f={};auto value=actions.suspend_inhibited(f.input);
 if(!value){fail(f,FrontendSchedulerError::input,"frontend scheduler input owner");return {};}
 try{return std::unique_ptr<NativeFrontendScheduler>(new NativeFrontendScheduler(std::make_unique<Impl>(actions,count)));}
 catch(const std::bad_alloc&){fail(f,FrontendSchedulerError::allocation,"frontend scheduler create");return {};}
}
bool NativeFrontendScheduler::access(FrontendSchedulerFailure& f) const noexcept {
 f={};if(std::this_thread::get_id()!=impl_->control->thread)return fail(f,FrontendSchedulerError::wrong_thread,"frontend scheduler owner thread");
 if(impl_->in_call||impl_->mutating)return fail(f,FrontendSchedulerError::invalid_state,"frontend scheduler reentry");return true;
}
std::optional<FrontendTaskToken> NativeFrontendScheduler::enqueue_owned(std::unique_ptr<NativeFrontendTask>& task,std::int64_t priority,bool override_gate,FrontendSchedulerFailure& f) noexcept {
 if(impl_->failed){fail(f,FrontendSchedulerError::invalid_state,"frontend scheduler terminal task error");return {};}
 if(!task){fail(f,FrontendSchedulerError::missing_task,"frontend scheduler missing owned task");return {};}
 if(impl_->generation==std::numeric_limits<std::uint64_t>::max()){fail(f,FrontendSchedulerError::overflow,"frontend scheduler generation exhausted");return {};}
 // Allocators may invoke application hooks. Keep the active execution capability
 // from recursively publishing another node/generation during this commit.
 Guard mutation(impl_->mutating);
 try {
  auto where=impl_->tasks.begin();while(where!=impl_->tasks.end()&&where->priority<=priority)++where;
  const auto next=impl_->generation+1;
  // Allocate the node before transferring the caller's task. Nothing after
  // allocation can throw; failure preserves both owners and generation.
  auto added=impl_->tasks.emplace(where,priority,override_gate,next);
  added->task=std::move(task);impl_->generation=next;
  return FrontendTaskToken(impl_->control,next);
 }catch(const std::bad_alloc&){fail(f,FrontendSchedulerError::allocation,"frontend scheduler enqueue allocation");}
 catch(const std::length_error&){fail(f,FrontendSchedulerError::allocation,"frontend scheduler enqueue extent");}return {};
}
std::optional<FrontendTaskToken> NativeFrontendScheduler::enqueue(std::unique_ptr<NativeFrontendTask>& task,std::int64_t priority,bool override_gate,FrontendSchedulerFailure& f) noexcept {
 if(!access(f))return {};Guard call(impl_->in_call);return enqueue_owned(task,priority,override_gate,f);
}
NativeFrontendExecution::NativeFrontendExecution(std::shared_ptr<FrontendExecutionControl> c,std::uint64_t g):control_(std::move(c)),generation_(g){}
NativeFrontendScheduler* NativeFrontendExecution::access(FrontendSchedulerFailure& f) const noexcept {
 f={};if(std::this_thread::get_id()!=control_->thread){fail(f,FrontendSchedulerError::wrong_thread,"frontend execution owner thread");return nullptr;}
 if(!control_->owner||!control_->active||generation_!=control_->generation){fail(f,FrontendSchedulerError::stale_execution,"frontend execution no longer active");return nullptr;}
 auto* owner=control_->owner;
 if(owner->impl_->mutating){fail(f,FrontendSchedulerError::invalid_state,"frontend execution mutation reentry");return nullptr;}
 return owner;
}
std::optional<FrontendTaskToken> NativeFrontendExecution::enqueue(std::unique_ptr<NativeFrontendTask>& task,std::int64_t priority,bool override_gate,FrontendSchedulerFailure& f) noexcept {
 auto* owner=access(f);if(!owner)return {};return owner->enqueue_owned(task,priority,override_gate,f);
}
bool NativeFrontendExecution::set_suspension_count(std::uint64_t count,FrontendSchedulerFailure& f) noexcept {
 auto* owner=access(f);if(!owner)return false;owner->impl_->suspension_count=count;return true;
}
std::optional<bool> NativeFrontendExecution::transition_pending(FrontendSchedulerFailure& f) const noexcept {
 auto* owner=access(f);if(!owner)return {};return owner->impl_->transition_pending;
}
bool NativeFrontendExecution::validate_transition_task(const FrontendTaskToken& token,std::int64_t priority,FrontendSchedulerFailure& f) const noexcept {
 auto* owner=access(f);if(!owner)return false;
 const auto& state=*owner->impl_;
 if(!state.transition_pending)return fail(f,FrontendSchedulerError::invalid_state,"frontend transition task requires clear");
 if(token.owner_!=state.control)return fail(f,FrontendSchedulerError::foreign_owner,"frontend transition foreign task");
 for(auto it=state.tasks.begin();it!=state.tasks.end();++it)if(it->generation==token.generation_){
  if(it==state.cursor)return fail(f,FrontendSchedulerError::invalid_state,"frontend transition executing task is not replacement");
  if(it->priority!=priority)return fail(f,FrontendSchedulerError::invalid_state,"frontend transition task priority mismatch");
  return true;
 }
 return fail(f,FrontendSchedulerError::stale_task,"frontend transition task no longer owned");
}
bool NativeFrontendScheduler::clear_transition_owned(FrontendSchedulerFailure& f) noexcept {
 if(impl_->transition_pending)return fail(f,FrontendSchedulerError::invalid_state,"frontend transition already requested");
 impl_->transition_pending=true;
 Guard mutation(impl_->mutating);
 for(auto it=impl_->tasks.begin();it!=impl_->tasks.end();){if(it==impl_->cursor)++it;else it=impl_->tasks.erase(it);}
 return true;
}
bool NativeFrontendExecution::clear_for_transition(FrontendSchedulerFailure& f) noexcept {
 auto* owner=access(f);if(!owner)return false;return owner->clear_transition_owned(f);
}
bool NativeFrontendScheduler::set_suspension_count(std::uint64_t count,FrontendSchedulerFailure& f) noexcept {
 if(!access(f))return false;Guard call(impl_->in_call);
 if(impl_->failed)return fail(f,FrontendSchedulerError::invalid_state,"frontend scheduler terminal task error");
 impl_->suspension_count=count;return true;
}
FrontendDispatchResult NativeFrontendScheduler::dispatch(FrontendSchedulerFailure& f) noexcept {
 if(!access(f))return {};
 Guard call(impl_->in_call);
 if(impl_->failed){fail(f,FrontendSchedulerError::invalid_state,"frontend scheduler terminal task error");return {};}
 if(!impl_->pass_active){impl_->pass_active=true;impl_->cursor=impl_->tasks.begin();}
 bool resuming=impl_->blocked;impl_->blocked=false;
 std::uint64_t updates=0;
 auto terminal=[&](){impl_->failed=true;impl_->control->active=false;impl_->blocked=false;return FrontendDispatchResult{FrontendDispatchProgress::error,updates};};
 while(impl_->cursor!=impl_->tasks.end()){
  if(!resuming){
   auto inhibited=impl_->actions.suspend_inhibited(f.input);
   if(!inhibited){fail(f,FrontendSchedulerError::input,"frontend scheduler input gate");return terminal();}
   if((*inhibited||impl_->suspension_count!=0)&&!impl_->cursor->override_gate){++impl_->cursor;continue;}
  }
  if(impl_->execution_generation==std::numeric_limits<std::uint64_t>::max()||updates==std::numeric_limits<std::uint64_t>::max()){
   fail(f,FrontendSchedulerError::overflow,"frontend scheduler dispatch counter exhausted");return terminal();}
  impl_->control->generation=++impl_->execution_generation;impl_->control->active=true;
  NativeFrontendExecution execution(impl_->control,impl_->execution_generation);
  impl_->transition_pending=false;TaskUpdate result=TaskUpdate::error;
  try{result=impl_->cursor->task->update(execution,f);}
  catch(const std::bad_alloc&){if(f.kind==FrontendSchedulerError::none)fail(f,FrontendSchedulerError::allocation,"frontend task allocation exception");}
  catch(...){if(f.kind==FrontendSchedulerError::none)fail(f,FrontendSchedulerError::task_exception,"frontend task exception");}
  impl_->control->active=false;++updates;resuming=false;
  // Preserve a concrete task's failure/cause even if its result is malformed.
  if(f.kind!=FrontendSchedulerError::none)return terminal();
  if(result==TaskUpdate::error){fail(f,FrontendSchedulerError::invalid_result,"frontend task error without named failure");return terminal();}
  if(result!=TaskUpdate::blocked&&result!=TaskUpdate::yield&&result!=TaskUpdate::finished&&result!=TaskUpdate::transition){
   fail(f,FrontendSchedulerError::invalid_result,"frontend task unknown result");return terminal();}
  if(impl_->transition_pending!=(result==TaskUpdate::transition)){
   fail(f,FrontendSchedulerError::invalid_result,"frontend task transition requires clear and matching result");return terminal();}
  if(result==TaskUpdate::blocked){impl_->blocked=true;return {FrontendDispatchProgress::blocked,updates};}
  if(result==TaskUpdate::transition){
   {Guard mutation(impl_->mutating);impl_->cursor=impl_->tasks.erase(impl_->cursor);}
   impl_->cursor=impl_->tasks.end();impl_->pass_active=false;impl_->blocked=false;impl_->transition_pending=false;
   return {FrontendDispatchProgress::transition,updates};
  }
  // Read live next AFTER the callback. Insertions after the current node are
  // eligible in this pass; insertions before it wait for a new source pass.
  auto next=std::next(impl_->cursor);
  if(result==TaskUpdate::finished){Guard mutation(impl_->mutating);impl_->tasks.erase(impl_->cursor);}
  impl_->cursor=next;
 }
 impl_->pass_active=false;impl_->blocked=false;
 return {FrontendDispatchProgress::pass_complete,updates};
}
bool NativeFrontendScheduler::cancel(const FrontendTaskToken& token,FrontendSchedulerFailure& f) noexcept {
 if(!access(f))return false;Guard call(impl_->in_call);
 if(token.owner_!=impl_->control)return fail(f,FrontendSchedulerError::foreign_owner,"frontend scheduler foreign task");
 for(auto it=impl_->tasks.begin();it!=impl_->tasks.end();++it)if(it->generation==token.generation_){
  if(impl_->pass_active&&it==impl_->cursor){impl_->cursor=impl_->tasks.end();impl_->pass_active=false;impl_->blocked=false;impl_->transition_pending=false;}
  Guard mutation(impl_->mutating);impl_->tasks.erase(it);return true;
 }
 return fail(f,FrontendSchedulerError::stale_task,"frontend scheduler retired task");
}
std::optional<FrontendSchedulerSnapshot> NativeFrontendScheduler::snapshot(FrontendSchedulerFailure& f) const noexcept {
 if(!access(f))return {};Guard call(impl_->in_call);
 try{
  FrontendSchedulerSnapshot s;s.suspension_count=impl_->suspension_count;s.pass_active=impl_->pass_active;s.blocked=impl_->blocked;s.transition_pending=impl_->transition_pending;s.failed=impl_->failed;
  s.tasks.reserve(impl_->tasks.size());for(auto it=impl_->tasks.begin();it!=impl_->tasks.end();++it)
   s.tasks.push_back({FrontendTaskToken(impl_->control,it->generation),it->priority,it->override_gate,impl_->pass_active&&it==impl_->cursor});
  return s;
 }catch(const std::bad_alloc&){fail(f,FrontendSchedulerError::allocation,"frontend scheduler snapshot allocation");}
 catch(const std::length_error&){fail(f,FrontendSchedulerError::allocation,"frontend scheduler snapshot extent");}return {};
}
} // namespace mka::game
