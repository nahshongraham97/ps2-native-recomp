#include "mka/retail_middleware_work.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <list>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
namespace mka::retail {
struct NativeRetailMiddlewareSubmission::State {std::shared_ptr<const void> group,parent;};
NativeRetailMiddlewareSubmission::NativeRetailMiddlewareSubmission(std::shared_ptr<State> s):state_(std::move(s)){}

struct NativeRetailMiddlewareDispatcher::State {
    mutable std::recursive_mutex mutex;const std::thread::id thread=std::this_thread::get_id();
    bool pumping=false,closed=false,engaged=false;std::shared_ptr<const void> identity=std::make_shared<int>(0);
    std::vector<std::shared_ptr<NativeRetailMiddlewareWorkGroup>> groups;
    struct Completion {std::shared_ptr<NativeRetailMiddlewareChild> child;std::shared_ptr<NativeRetailMiddlewareSubmission::State> ticket;RetailMiddlewareCompletionPhase phase=RetailMiddlewareCompletionPhase::primary_pending;RetailMiddlewareWorkFailure failure;};
    std::list<Completion> completed;
};
struct NativeRetailMiddlewareWorkGroup::State {
    mutable std::recursive_mutex mutex;const std::thread::id thread=std::this_thread::get_id();
    bool enabled=false,polling=false,closed=false;
    std::shared_ptr<const void> identity=std::make_shared<int>(0);
    struct Entry {std::shared_ptr<NativeRetailMiddlewareChild> child;std::shared_ptr<NativeRetailMiddlewareSubmission::State> ticket;};
    std::vector<Entry> children;
    std::weak_ptr<NativeRetailMiddlewareDispatcher::State> parent;
};
namespace {
bool thread_ok(std::thread::id expected,RetailMiddlewareWorkFailure& f){if(std::this_thread::get_id()==expected)return true;f.kind=RetailMiddlewareWorkError::wrong_thread;return false;}
void child_failure(RetailMiddlewareWorkFailure& f){if(f.kind==RetailMiddlewareWorkError::none)f.kind=RetailMiddlewareWorkError::child_failed;}
}
NativeRetailMiddlewareWorkGroup::NativeRetailMiddlewareWorkGroup(std::unique_ptr<State> s):state_(std::move(s)){}
NativeRetailMiddlewareWorkGroup::~NativeRetailMiddlewareWorkGroup(){
 if(!state_->closed&&(state_->enabled||state_->polling||!state_->children.empty()||!state_->parent.expired())){
  std::fputs("retail middleware group destroyed with live ownership\n",stderr);std::terminate();
 }
}
bool NativeRetailMiddlewareWorkGroup::validate_current_thread(RetailMiddlewareWorkFailure& f) const noexcept {
 f={};return thread_ok(state_->thread,f);
}
std::shared_ptr<NativeRetailMiddlewareWorkGroup> NativeRetailMiddlewareWorkGroup::create(RetailMiddlewareWorkFailure& f)noexcept{
 f={};try{return std::shared_ptr<NativeRetailMiddlewareWorkGroup>(new NativeRetailMiddlewareWorkGroup(std::make_unique<State>()));}catch(const std::bad_alloc&){f.kind=RetailMiddlewareWorkError::allocation;}catch(...){f.kind=RetailMiddlewareWorkError::native_exception;}return {};
}
bool NativeRetailMiddlewareWorkGroup::configure(std::size_t hint,RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return false;std::lock_guard lock(state_->mutex);
 if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return false;}if(state_->polling){f.kind=RetailMiddlewareWorkError::active;return false;}
 try{state_->children.reserve(hint);state_->enabled=true;return true;}catch(const std::bad_alloc&){f.kind=RetailMiddlewareWorkError::allocation;}catch(const std::length_error&){f.kind=RetailMiddlewareWorkError::host_capacity;}catch(...){f.kind=RetailMiddlewareWorkError::native_exception;}return false;
}
std::optional<NativeRetailMiddlewareSubmission> NativeRetailMiddlewareWorkGroup::append(std::shared_ptr<NativeRetailMiddlewareChild> child,RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return {};if(!child){f.kind=RetailMiddlewareWorkError::invalid_child;return {};}
 try{auto ticket=std::make_shared<NativeRetailMiddlewareSubmission::State>();ticket->group=state_->identity;
  std::lock_guard lock(state_->mutex);if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return {};}
  if(auto parent=state_->parent.lock())ticket->parent=parent->identity;
  state_->children.push_back({std::move(child),ticket});return NativeRetailMiddlewareSubmission(std::move(ticket));
 }catch(const std::bad_alloc&){f.kind=RetailMiddlewareWorkError::allocation;}catch(const std::length_error&){f.kind=RetailMiddlewareWorkError::host_capacity;}catch(...){f.kind=RetailMiddlewareWorkError::native_exception;}return {};
}
std::optional<std::size_t> NativeRetailMiddlewareWorkGroup::poll(RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return {};std::lock_guard lock(state_->mutex);
 if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return {};}if(state_->polling)return 0;
 auto parent=state_->parent.lock();if(!parent){f.kind=RetailMiddlewareWorkError::unbound_parent;return {};}
 state_->polling=true;struct Reset {bool& value;~Reset(){value=false;}} reset{state_->polling};std::size_t transferred=0,index=0,end=state_->children.size();
 try{while(index<end){
   auto entry=state_->children[index];auto child=entry.child;
   // Allocate a completion node BEFORE the child can produce an irreversible
   // status2. Splicing that exact owner to parent is nonallocating.
   std::list<NativeRetailMiddlewareDispatcher::State::Completion> reserved;reserved.push_back({child,entry.ticket});
   auto status=child->poll(f);if(!status){child_failure(f);return {};}
   if(*status==RetailMiddlewarePollStatus::source_zero||*status==RetailMiddlewarePollStatus::source_one){++index;continue;}
   if(*status!=RetailMiddlewarePollStatus::transfer_to_parent){f.kind=RetailMiddlewareWorkError::invalid_status;return {};}
   {std::lock_guard parent_lock(parent->mutex);if(parent->closed){f.kind=RetailMiddlewareWorkError::closed;return {};}
    parent->completed.splice(parent->completed.end(),reserved);}
   state_->children.erase(state_->children.begin()+index);end=state_->children.size();++transferred;
  }return transferred;
 }catch(const std::bad_alloc&){f.kind=RetailMiddlewareWorkError::allocation;}catch(...){f.kind=RetailMiddlewareWorkError::native_exception;}return {};
}
RetailMiddlewareGroupObservation NativeRetailMiddlewareWorkGroup::observe()const noexcept{std::lock_guard lock(state_->mutex);return {state_->enabled,state_->polling,state_->closed,!state_->parent.expired(),state_->children.size()};}
bool NativeRetailMiddlewareWorkGroup::shutdown(RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return false;std::lock_guard lock(state_->mutex);if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return false;}
 if(state_->polling||!state_->children.empty()||!state_->parent.expired()){f.kind=RetailMiddlewareWorkError::active;return false;}state_->closed=true;return true;
}
NativeRetailMiddlewareDispatcher::NativeRetailMiddlewareDispatcher(std::shared_ptr<State> s):state_(std::move(s)){}
NativeRetailMiddlewareDispatcher::~NativeRetailMiddlewareDispatcher(){
 if(!state_->closed&&(state_->engaged||state_->pumping||!state_->groups.empty()||!state_->completed.empty())){
  std::fputs("retail middleware dispatcher destroyed with live ownership\n",stderr);std::terminate();
 }
}
bool NativeRetailMiddlewareDispatcher::validate_current_thread(RetailMiddlewareWorkFailure& f) const noexcept {
 f={};return thread_ok(state_->thread,f);
}
std::shared_ptr<NativeRetailMiddlewareDispatcher> NativeRetailMiddlewareDispatcher::create(RetailMiddlewareWorkFailure& f)noexcept{
 f={};try{return std::shared_ptr<NativeRetailMiddlewareDispatcher>(new NativeRetailMiddlewareDispatcher(std::make_shared<State>()));}catch(const std::bad_alloc&){f.kind=RetailMiddlewareWorkError::allocation;}catch(...){f.kind=RetailMiddlewareWorkError::native_exception;}return {};
}
bool NativeRetailMiddlewareDispatcher::attach(std::shared_ptr<NativeRetailMiddlewareWorkGroup> group,RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return false;if(!group){f.kind=RetailMiddlewareWorkError::invalid_child;return false;}if(!thread_ok(group->state_->thread,f))return false;
 try{std::lock_guard lock(state_->mutex);std::lock_guard group_lock(group->state_->mutex);
  if(state_->closed||group->state_->closed){f.kind=RetailMiddlewareWorkError::closed;return false;}if(state_->pumping||group->state_->polling){f.kind=RetailMiddlewareWorkError::active;return false;}
  if(auto previous=group->state_->parent.lock();previous&&previous!=state_){f.kind=RetailMiddlewareWorkError::foreign_parent;return false;}
  if(std::find(state_->groups.begin(),state_->groups.end(),group)!=state_->groups.end())return true;
  state_->groups.push_back(group);group->state_->parent=state_;for(auto& entry:group->state_->children)entry.ticket->parent=state_->identity;state_->engaged=true;return true;
 }catch(const std::bad_alloc&){f.kind=RetailMiddlewareWorkError::allocation;}catch(const std::length_error&){f.kind=RetailMiddlewareWorkError::host_capacity;}catch(...){f.kind=RetailMiddlewareWorkError::native_exception;}return false;
}
bool NativeRetailMiddlewareDispatcher::validate_attachment(
 const std::shared_ptr<NativeRetailMiddlewareWorkGroup>& group,RetailMiddlewareWorkFailure& f) const noexcept {
 f={};if(!thread_ok(state_->thread,f))return false;
 if(!group){f.kind=RetailMiddlewareWorkError::invalid_child;return false;}
 if(!thread_ok(group->state_->thread,f))return false;
 std::lock_guard lock(state_->mutex);std::lock_guard group_lock(group->state_->mutex);
 if(state_->closed||group->state_->closed){f.kind=RetailMiddlewareWorkError::closed;return false;}
 if(state_->pumping||group->state_->polling){f.kind=RetailMiddlewareWorkError::active;return false;}
 if(group->state_->parent.lock()!=state_||
    std::find(state_->groups.begin(),state_->groups.end(),group)==state_->groups.end()){
  f.kind=RetailMiddlewareWorkError::foreign_parent;return false;
 }
 return true;
}
bool NativeRetailMiddlewareDispatcher::detach(const std::shared_ptr<NativeRetailMiddlewareWorkGroup>& group,RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return false;if(!group){f.kind=RetailMiddlewareWorkError::invalid_child;return false;}if(!thread_ok(group->state_->thread,f))return false;
 std::shared_ptr<NativeRetailMiddlewareWorkGroup> retained;
 {std::lock_guard lock(state_->mutex);std::lock_guard group_lock(group->state_->mutex);if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return false;}if(state_->pumping||group->state_->polling||!group->state_->children.empty()||!state_->completed.empty()){f.kind=RetailMiddlewareWorkError::active;return false;}
  auto found=std::find(state_->groups.begin(),state_->groups.end(),group);if(found==state_->groups.end()){f.kind=RetailMiddlewareWorkError::foreign_parent;return false;}retained=std::move(*found);state_->groups.erase(found);group->state_->parent.reset();}return true;
}
std::optional<std::size_t> NativeRetailMiddlewareDispatcher::pump(RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return {};std::lock_guard lock(state_->mutex);if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return {};}if(state_->pumping){f.kind=RetailMiddlewareWorkError::active;return {};}
 // Direct group.poll can call out before dispatcher.pumping is set. Refuse
 // this nested route before polling any other group or draining completion.
 for(const auto& group:state_->groups){std::lock_guard group_lock(group->state_->mutex);if(group->state_->polling){f.kind=RetailMiddlewareWorkError::active;return {};}}
 state_->engaged=true;state_->pumping=true;struct Reset {bool& value;~Reset(){value=false;}} reset{state_->pumping};
 try{for(const auto& group:state_->groups)if(!group->poll(f))return {};
  std::size_t completed=0;while(!state_->completed.empty()){
   auto& item=state_->completed.front();
   if(item.phase==RetailMiddlewareCompletionPhase::failed_uncertain){f=item.failure;return {};}
   if(item.phase==RetailMiddlewareCompletionPhase::primary_pending){
    // Latch before arbitrary code: an exception/failed primary cannot replay.
    item.phase=RetailMiddlewareCompletionPhase::failed_uncertain;
    item.failure.kind=RetailMiddlewareWorkError::completion_uncertain;
    if(!item.child->primary_complete(f)){child_failure(f);item.failure=f;return {};}
    item.phase=RetailMiddlewareCompletionPhase::primary_done;
   }
   if(item.phase==RetailMiddlewareCompletionPhase::primary_done){
    // This is a const source-flag query after primary, not another callback.
    auto required=item.child->release_required(f);if(!required){child_failure(f);return {};}
    if(!*required){auto retired=std::move(item.child);state_->completed.pop_front();++completed;retired.reset();continue;}
    item.phase=RetailMiddlewareCompletionPhase::release_pending;
   }
   if(item.phase==RetailMiddlewareCompletionPhase::release_pending){
    item.phase=RetailMiddlewareCompletionPhase::failed_uncertain;
    item.failure.kind=RetailMiddlewareWorkError::completion_uncertain;
    auto release=item.child->release(f);if(!release){child_failure(f);item.failure=f;return {};}
    if(*release==RetailMiddlewareReleaseResult::retained){item.phase=RetailMiddlewareCompletionPhase::release_pending;f.kind=RetailMiddlewareWorkError::release_pending;return {};}
    if(*release!=RetailMiddlewareReleaseResult::released){item.failure.kind=RetailMiddlewareWorkError::invalid_status;f=item.failure;return {};}
    auto retired=std::move(item.child);state_->completed.pop_front();++completed;retired.reset();
   }
  }return completed;
 }catch(...){f.kind=RetailMiddlewareWorkError::native_exception;return {};}
}
std::optional<RetailMiddlewareCompletionPhase> NativeRetailMiddlewareDispatcher::completion_phase(const NativeRetailMiddlewareSubmission& submission,RetailMiddlewareWorkFailure& f)const noexcept{
 f={};if(!thread_ok(state_->thread,f))return {};
 std::lock_guard lock(state_->mutex);
 if(!submission.state_){f.kind=RetailMiddlewareWorkError::invalid_submission;return {};}
 if(submission.state_->parent!=state_->identity){f.kind=RetailMiddlewareWorkError::foreign_submission;return {};}
 if(!state_->closed){
  for(const auto& item:state_->completed)if(item.ticket==submission.state_)return item.phase;
  for(const auto& group:state_->groups){std::lock_guard group_lock(group->state_->mutex);
   if(group->state_->identity==submission.state_->group)for(const auto& entry:group->state_->children)if(entry.ticket==submission.state_)return RetailMiddlewareCompletionPhase::queued;
  }
 }
 f.kind=RetailMiddlewareWorkError::stale_submission;return {};
}
RetailMiddlewareDispatcherObservation NativeRetailMiddlewareDispatcher::observe()const noexcept{std::lock_guard lock(state_->mutex);return {state_->pumping,state_->closed,state_->groups.size(),state_->completed.size()};}
bool NativeRetailMiddlewareDispatcher::shutdown(RetailMiddlewareWorkFailure& f)noexcept{
 f={};if(!thread_ok(state_->thread,f))return false;std::lock_guard lock(state_->mutex);if(state_->closed){f.kind=RetailMiddlewareWorkError::closed;return false;}if(state_->pumping||!state_->groups.empty()||!state_->completed.empty()){f.kind=RetailMiddlewareWorkError::active;return false;}state_->closed=true;return true;
}
namespace main_binding {
bool construct_retail_middleware_work_group(NativeRetailMainState& main,StartupFailure& f){
 f={};if(main.middleware_work_group){f.kind=StartupError::already_initialized;f.operation="retail middleware work group exists";return false;}
 RetailMiddlewareWorkFailure failure;auto group=NativeRetailMiddlewareWorkGroup::create(failure);
 if(!group){f.kind=failure.kind==RetailMiddlewareWorkError::allocation?StartupError::allocation:StartupError::native_exception;f.operation="retail middleware work group construction";return false;}
 main.middleware_work_group=std::move(group);return true;
}
} // namespace main_binding
} // namespace mka::retail
