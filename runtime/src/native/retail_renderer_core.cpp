#include "mka/retail_renderer_core.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <new>
#include <utility>
namespace mka::retail {
namespace {
bool fail(RetailRendererCoreFailure& f,RetailRendererCoreError k,const char* op)noexcept{f.kind=k;f.operation=op;return false;}
template<class T,class Create>std::shared_ptr<T> heap_object(
 const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,RetailRendererCoreFailure& f,Create create)noexcept{
 static_assert(alignof(T)<=16);
 auto allocation=heap->allocate(sizeof(T),f.allocator);
 if(!allocation){fail(f,RetailRendererCoreError::allocator,"native renderer typed object allocation");return {};}
 try {
  auto* object=create(allocation->data());
  return std::shared_ptr<T>(object,[heap,allocation=std::move(*allocation)](T* p)mutable{p->~T();});
 }catch(const std::bad_alloc&){fail(f,RetailRendererCoreError::allocation,"native renderer owner allocation");}
 catch(...){fail(f,RetailRendererCoreError::native_exception,"native renderer owner construction");}
 return {};
}
struct Busy {bool& flag;explicit Busy(bool& b):flag(b){flag=true;}~Busy(){flag=false;}};
}
struct NativeRetailRendererErrorDomain::State {
 std::weak_ptr<NativeRetailGameSession> session;
 RetailRendererErrorRecord record{}; // source BSS before19F410,not ready latch.
 std::atomic<std::uint32_t> count{};
 const std::thread::id thread{std::this_thread::get_id()};
 bool access(RetailRendererCoreFailure& f)const noexcept{
  f={};if(std::this_thread::get_id()!=thread)return fail(f,RetailRendererCoreError::wrong_thread,"native renderer error owner thread");
  const auto owner=session.lock();RetailGameSessionFailure sf;
  return (owner&&owner->validate_current_thread(sf))||fail(f,RetailRendererCoreError::owner,"native renderer error Session lifetime");
 }
 void store(RetailRendererErrorRecord input)noexcept{
  if(record.plugin==0&&record.code==0x80000000u)record={(input.code&0x80000000u)?0u:input.plugin,input.code};
 }
};
NativeRetailRendererErrorDomain::~NativeRetailRendererErrorDomain(){if(counted_)state_->count.fetch_sub(1,std::memory_order_relaxed);}
bool NativeRetailRendererErrorDomain::store(RetailRendererErrorRecord input,RetailRendererCoreFailure& f)noexcept{
 if(!state_->access(f))return false;state_->store(input);return true;
}
std::optional<RetailRendererErrorRecord> NativeRetailRendererErrorDomain::take(RetailRendererCoreFailure& f)noexcept{
 if(!state_->access(f))return {};const auto result=state_->record;state_->record={0,0x80000000u};return result;
}
NativeRetailRendererCore::NativeRetailRendererCore(std::weak_ptr<NativeRetailGameSession> session,
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator,content::NativeContentVolume&& content)
 :session_(std::move(session)),allocator_(std::move(allocator)),content_(std::move(content)){}
std::shared_ptr<NativeRetailRendererCore> NativeRetailRendererCore::for_session(
 std::shared_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
 const content::NativeContentVolume& content,RetailRendererCoreFailure& f)noexcept{
 f={};RetailGameSessionFailure sf;
 if(!session||!session->validate_current_thread(sf)||!allocator){fail(f,RetailRendererCoreError::owner,"1A6DD0 actual Session/allocator");return {};}
 if(session->renderer_core_){
  auto core=session->renderer_core_;
  if(core->allocator_identity()!=allocator->owner_identity()||!core->content_.same_volume(content)){fail(f,RetailRendererCoreError::owner,"1A6DD0 foreign actual native heap");return {};}
  return core;
 }
 auto retained=content.retain(f.content);if(!retained){fail(f,RetailRendererCoreError::content,"1A6DD0 retained native content root");return {};}
 auto core=heap_object<NativeRetailRendererCore>(allocator,f,[&](void* p){return new(p)NativeRetailRendererCore(session,allocator,std::move(*retained));});
 if(!core)return {};
 core->errors_=heap_object<NativeRetailRendererErrorDomain::State>(allocator,f,[](void* p){return new(p)NativeRetailRendererErrorDomain::State;});
 if(!core->errors_)return {};
 core->errors_->session=session;session->renderer_core_=core;return core;
}
bool NativeRetailRendererCore::access(RetailRendererCoreFailure& f)const noexcept{
 f={};if(std::this_thread::get_id()!=thread_)return fail(f,RetailRendererCoreError::wrong_thread,"native renderer registry thread");
 const auto owner=session_.lock();RetailGameSessionFailure sf;
 if(!owner||!owner->validate_current_thread(sf))return fail(f,RetailRendererCoreError::owner,"native renderer registry Session lifetime");
 if(after_effect_unknown_)return fail(f,RetailRendererCoreError::after_effect_unknown,"native renderer constructor partial effect");
 return true;
}
void NativeRetailRendererCore::store_error(RetailRendererErrorRecord r)noexcept{errors_->store(r);}
std::optional<std::shared_ptr<const NativeRetailRendererPluginRegistration>> NativeRetailRendererCore::register_factory(
 RetailRendererPluginSpec spec,RetailRendererPluginCallbacks callbacks,RetailRendererCoreFailure& f)noexcept{
 if(!access(f))return {};
 if(busy_){fail(f,RetailRendererCoreError::reentrant,"1A3190 callback registration");return {};}
 if(sealed_){store_error({1,0x80000017u});fail(f,RetailRendererCoreError::registration_closed,"1A3190 native registration closed");return {};}
 for(const auto& entry:entries_)if(entry->spec_.id==spec.id){store_error({1,0x80000017u});return entry;}
 if(!callbacks.construct||!callbacks.destroy){fail(f,RetailRendererCoreError::unbound_factory,"1A3190 required typed native factory");return {};}
 auto entry=heap_object<NativeRetailRendererPluginRegistration>(allocator_,f,[&](void* p){return new(p)NativeRetailRendererPluginRegistration(spec,callbacks);});
 if(!entry)return {};
 try{entries_.push_back(entry);return entry;}catch(const std::bad_alloc&){fail(f,RetailRendererCoreError::allocation,"1A3190 native ordered registry growth");}
 catch(...){fail(f,RetailRendererCoreError::native_exception,"1A3190 native ordered registry growth");}
 return {};
}
std::optional<std::vector<RetailRendererPluginSpec>> NativeRetailRendererCore::registered(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};try{std::vector<RetailRendererPluginSpec> result;for(const auto& e:entries_)result.push_back(e->spec_);return result;}
 catch(const std::bad_alloc&){fail(f,RetailRendererCoreError::allocation,"native renderer registry observation");}
 catch(...){fail(f,RetailRendererCoreError::native_exception,"native renderer registry observation");}return {};
}
std::shared_ptr<NativeRetailRendererErrorDomain> NativeRetailRendererCore::construct_error_domain(RetailRendererCoreFailure& f)noexcept{
 if(!access(f))return {};
 auto result=heap_object<NativeRetailRendererErrorDomain>(allocator_,f,[&](void* p){return new(p)NativeRetailRendererErrorDomain(errors_);});
 if(result){result->counted_=true;errors_->record={0,0x80000000u};errors_->count.fetch_add(1,std::memory_order_relaxed);}return result;
}
std::optional<std::uint32_t> NativeRetailRendererCore::error_domain_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return errors_->count.load(std::memory_order_relaxed);
}
std::optional<std::uint32_t> NativeRetailRendererCore::module_040d_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return module_040d_count_.load(std::memory_order_relaxed);
}
namespace renderer_required {
bool construct_core_plugin_040d(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!core.access(f))return false;core.module_040d_count_.fetch_add(1,std::memory_order_relaxed);return true;
}
void destroy_core_plugin_040d(NativeRetailRendererCore& core)noexcept{core.module_040d_count_.fetch_sub(1,std::memory_order_relaxed);}
}
bool construct_native_renderer_error(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){core.error_domain_=core.construct_error_domain(f);return bool(core.error_domain_);}
void destroy_native_renderer_error(NativeRetailRendererCore& core)noexcept{core.error_domain_.reset();}
bool NativeRetailRendererCore::register_core_plugins(RetailRendererCoreFailure& f)noexcept{
 using namespace renderer_required;
 struct Row{RetailRendererPluginSpec spec;RetailRendererPluginCallbacks callbacks;};
 const Row rows[]{
  {{0x40f,8},{construct_native_renderer_error,destroy_native_renderer_error}},
  {{0x0401,24},{construct_core_plugin_0401,destroy_core_plugin_0401}},
  {{0x040d,0},{construct_core_plugin_040d,destroy_core_plugin_040d}},
  {{0x0402,24},{construct_core_plugin_0402,destroy_core_plugin_0402}},
  {{0x0403,4},{construct_core_plugin_0403,destroy_core_plugin_0403}},
  {{0x0404,4},{construct_core_plugin_0404,destroy_core_plugin_0404}},
  {{0x0405,4},{construct_core_plugin_0405,destroy_core_plugin_0405}},
  {{0x0406,544},{construct_core_plugin_0406,destroy_core_plugin_0406}},
  {{0x0407,100},{construct_core_plugin_0407,destroy_core_plugin_0407}},
  {{0x0408,52},{construct_core_plugin_0408,destroy_core_plugin_0408}},
  {{0x0409,96},{construct_core_plugin_0409,destroy_core_plugin_0409}},
  {{0x0412,4},{construct_core_plugin_0412,destroy_core_plugin_0412}},
  {{0x040a,124},{construct_core_plugin_040a,destroy_core_plugin_040a}},
  {{0x040b,40},{construct_core_plugin_040b,destroy_core_plugin_040b}},
 };
 bool ok=true;RetailRendererCoreFailure first;
 //1A66A0 evaluates EVERY registration before OR/testing signed results.
 for(const auto& row:rows){RetailRendererCoreFailure detail;if(!register_factory(row.spec,row.callbacks,detail)){if(ok)first=detail;ok=false;}}
 if(!ok)f=first;return ok;
}
std::optional<bool> NativeRetailRendererCore::initialize_core(RetailRendererCoreFailure& f){
 if(!access(f))return {};
 if(busy_||initializing_){fail(f,RetailRendererCoreError::reentrant,"1A6DD0 native initialization active");return {};}
 if(initialized_)return false; //1A6E64..6C returns source s1=0.
 Busy initialization(initializing_);
 if(!register_core_plugins(f)){entries_.clear();return {};}
 // Entire1B72B0 remains a typed required producer; its object-family and
 // stream callbacks are not guessed from the core extension metadata.
 std::optional<bool> tail;
 object_extensions_prepared_=true;
 try{tail=renderer_required::register_object_extensions_1b72b0(*this,f);}
 catch(const detail::NativeMainReentry&){discard_object_extensions();entries_.clear();throw;}
 catch(const std::bad_alloc&){discard_object_extensions();entries_.clear();fail(f,RetailRendererCoreError::allocation,"1B72B0 required native extension registration");return {};}
 catch(...){discard_object_extensions();entries_.clear();fail(f,RetailRendererCoreError::native_exception,"1B72B0 required native extension registration");return {};}
 if(!tail||!*tail){discard_object_extensions();entries_.clear();return tail;}
 initialized_=true;return true;
}
bool NativeRetailRendererCore::construct_registered(RetailRendererCoreFailure& f){
 if(!access(f))return false;
 if(busy_||!constructed_.empty())return fail(f,RetailRendererCoreError::reentrant,"1A3480 native construction active");
 // Reserve before the first source callback,so publication has no later allocation.
 try{constructed_.reserve(entries_.size());}catch(const std::bad_alloc&){return fail(f,RetailRendererCoreError::allocation,"1A3480 native construction retention");}
 sealed_=true;Busy guard(busy_);
 for(const auto& entry:entries_){
  bool ok=false;
  try{ok=entry->callbacks_.construct(*this,f);}
  catch(const detail::NativeMainReentry&){throw;} // Legitimate transfer retains prefix,not an error latch.
  catch(...){after_effect_unknown_=true;return fail(f,RetailRendererCoreError::after_effect_unknown,"1A3480 native constructor exception");}
  if(!ok){
   while(!constructed_.empty()){auto previous=constructed_.back();constructed_.pop_back();previous->callbacks_.destroy(*this);}
   if(f.kind==RetailRendererCoreError::none)f.operation="1A3480 source constructor refusal";
   return false;
  }
  constructed_.push_back(entry);
 }
 return true;
}
bool NativeRetailRendererCore::destroy_constructed(RetailRendererCoreFailure& f)noexcept{
 if(!access(f))return false;if(busy_)return fail(f,RetailRendererCoreError::reentrant,"native renderer destruction active");
 Busy guard(busy_);while(!constructed_.empty()){auto entry=constructed_.back();constructed_.pop_back();entry->callbacks_.destroy(*this);}return true;
}
void NativeRetailRendererCore::discard_object_extensions()noexcept{
 if(object_extensions_prepared_){renderer_required::discard_object_extensions_after_core_init(*this);object_extensions_prepared_=false;}
}
NativeRetailRendererCore::~NativeRetailRendererCore(){
 while(!constructed_.empty()){auto entry=constructed_.back();constructed_.pop_back();entry->callbacks_.destroy(*this);}
 discard_object_extensions();
}
std::optional<content::NativeContentFile> NativeRetailRendererCore::open_resource(std::string_view key,RetailRendererCoreFailure& f)const{
 if(!access(f))return {};
 auto file=content_.open_resource(key,f.content);if(!file)fail(f,RetailRendererCoreError::content,"native renderer retained content file");return file;
}
}
