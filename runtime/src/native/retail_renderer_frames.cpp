#include "mka/retail_renderer_frames.hpp"
#include "mka/retail_game_session.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
bool fail(RetailRendererFrameFailure& f,RetailRendererFrameError kind,const char* op)noexcept{
 f.kind=kind;f.operation=op;return false;
}
RetailRendererMatrixValue identity_matrix()noexcept{
 const auto one=std::bit_cast<float>(std::uint32_t{0x3f800000});
 return {{{one,0,0},{0,one,0},{0,0,one},{0,0,0}},0x20003u};
}
template<class T,class Create>std::shared_ptr<T> heap_object(
 const std::shared_ptr<NativeRetailRendererAllocatorService>& allocator,
 RetailRendererFrameFailure& f,Create create)noexcept{
 static_assert(alignof(T)<=16);
 auto storage=allocator->allocate(sizeof(T),f.allocator);
 if(!storage){fail(f,RetailRendererFrameError::allocator,"0403 native typed allocation");return {};}
 try{
  auto* object=create(storage->data());
  return std::shared_ptr<T>(object,[heap=std::shared_ptr<NativeRetailRendererAllocatorService>(allocator),storage=std::move(*storage)](T* p)mutable{
   p->~T();storage=memory::NativeAllocation{};heap.reset();
  });
 }catch(const std::bad_alloc&){fail(f,RetailRendererFrameError::allocation,"0403 native allocation control ownership");}
 catch(...){fail(f,RetailRendererFrameError::native_exception,"0403 native typed construction");}
 return {};
}
struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}};
}
struct NativeRetailRendererFrameRecord {
 RetailRendererMatrixValue modelling{identity_matrix()},world{identity_matrix()};
 std::uint8_t flags{};
 bool alive{true},queued{};
 NativeRetailRendererFrameRecord* parent{};
 NativeRetailRendererFrameRecord* first_child{};
 NativeRetailRendererFrameRecord* next_sibling{};
 NativeRetailRendererFrameRecord* root{this};
 NativeRetailRendererFrameRecord* dirty_previous{};
 NativeRetailRendererFrameRecord* dirty_next{};
 std::weak_ptr<NativeRetailRendererFrameRecord> self;
 std::weak_ptr<NativeRetailRendererFrames> owner;
};
NativeRetailRendererFrames::NativeRetailRendererFrames(std::weak_ptr<NativeRetailGameSession> session,
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator)noexcept
 :session_(std::move(session)),allocator_(std::move(allocator)){}
NativeRetailRendererFrames::~NativeRetailRendererFrames(){retire();}
std::shared_ptr<NativeRetailRendererFrames> NativeRetailRendererFrames::create(
 std::weak_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
 RetailRendererFrameFailure& f)noexcept{
 f={};const auto lease=session.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)||!allocator){fail(f,RetailRendererFrameError::owner,"0403 actual Session/heap required");return {};}
 // A real growing host resource, not the source176-byte/50-entry free list.
 return heap_object<NativeRetailRendererFrames>(allocator,f,[&](void* p){
  return new(p)NativeRetailRendererFrames(session,allocator);
 });
}
std::shared_ptr<NativeRetailGameSession> NativeRetailRendererFrames::admit(RetailRendererFrameFailure& f)const noexcept{
 f={};
 if(std::this_thread::get_id()!=thread_){fail(f,RetailRendererFrameError::wrong_thread,"0403 owner thread");return {};}
 if(!active_.load(std::memory_order_acquire)){fail(f,RetailRendererFrameError::retired,"0403 native resource retired");return {};}
 auto lease=session_.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)){fail(f,RetailRendererFrameError::owner,"0403 actual Session lifetime");return {};}
 if(busy_){fail(f,RetailRendererFrameError::reentrant,"0403 native resource mutation active");return {};}
 return lease;
}
std::shared_ptr<NativeRetailRendererFrameRecord> NativeRetailRendererFrames::record(
 const NativeRetailRendererFrameHandle& frame,RetailRendererFrameFailure& f)const noexcept{
 if(frame.empty()){fail(f,RetailRendererFrameError::invalid_handle,"0403 empty base-frame handle");return {};}
 if(frame.owner_.lock().get()!=this){fail(f,RetailRendererFrameError::foreign_owner,"0403 foreign native frame resource");return {};}
 auto result=frame.record_.lock();
 if(!result||!result->alive){fail(f,RetailRendererFrameError::retired_frame,"0403 base frame retired");return {};}
 return result;
}
NativeRetailRendererFrameHandle NativeRetailRendererFrames::handle(NativeRetailRendererFrameRecord* node)const noexcept{
 return node?NativeRetailRendererFrameHandle(node->owner,node->self):NativeRetailRendererFrameHandle{};
}
std::optional<NativeRetailRendererFrameHandle> NativeRetailRendererFrames::create_base_frame(
 RetailRendererFrameFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return {};
 Busy guard(busy_);
 auto node=heap_object<NativeRetailRendererFrameRecord>(allocator_,f,[](void* p){
  return new(p)NativeRetailRendererFrameRecord;
 });
 if(!node)return {};
 try{
  node->self=node;node->owner=weak_from_this();
  if(records_.size()==records_.max_size()){
   fail(f,RetailRendererFrameError::allocation,"0403 native host frame ownership capacity");return {};
  }
  if(records_.size()==records_.capacity()){
   const auto maximum=records_.max_size();
   records_.reserve(records_.empty()?1:records_.size()<=maximum/2?records_.size()*2:maximum);
  } // Complete geometrically growing host ownership before publication.
  if(!active_.load(std::memory_order_acquire)){fail(f,RetailRendererFrameError::retired,"0403 retired during native allocation");return {};}
  records_.push_back(node);
  return handle(node.get());
 }catch(const std::bad_alloc&){fail(f,RetailRendererFrameError::allocation,"0403 native frame ownership growth");}
 catch(const std::length_error&){fail(f,RetailRendererFrameError::allocation,"0403 native host frame ownership capacity");}
 catch(...){fail(f,RetailRendererFrameError::native_exception,"0403 native base-frame admission");}
 return {};
}
std::optional<RetailRendererFrameObservation> NativeRetailRendererFrames::observe(
 const NativeRetailRendererFrameHandle& frame,RetailRendererFrameFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto node=record(frame,f);if(!node)return {};
 return RetailRendererFrameObservation{node->modelling,node->world,node->flags,
  handle(node->parent),handle(node->first_child),handle(node->next_sibling),handle(node->root)};
}
std::optional<RetailRendererFrameModuleObservation> NativeRetailRendererFrames::observe_module(
 RetailRendererFrameFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};
 std::size_t dirty{};for(auto* node=dirty_first_;node;node=node->dirty_next)++dirty;
 return RetailRendererFrameModuleObservation{records_.size(),dirty};
}
std::optional<std::vector<NativeRetailRendererFrameHandle>> NativeRetailRendererFrames::dirty_roots(
 RetailRendererFrameFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};
 try{
  std::vector<NativeRetailRendererFrameHandle> result;
  for(auto* node=dirty_first_;node;node=node->dirty_next)result.push_back(handle(node));
  return result;
 }catch(const std::bad_alloc&){fail(f,RetailRendererFrameError::allocation,"0403 native dirty-root observation");}
 catch(...){fail(f,RetailRendererFrameError::native_exception,"0403 dirty-root snapshot");}
 return {};
}
void NativeRetailRendererFrames::mark_record(NativeRetailRendererFrameRecord* node)noexcept{
 auto* root=node->root;
 if(!(root->flags&3u)){
  root->dirty_previous=nullptr;root->dirty_next=dirty_first_;root->queued=true;
  if(dirty_first_)dirty_first_->dirty_previous=root;else dirty_last_=root;
  dirty_first_=root;
 }
 root->flags=static_cast<std::uint8_t>(root->flags|3u);
 node->flags=static_cast<std::uint8_t>(node->flags|0x0cu);
}
void NativeRetailRendererFrames::unlink_dirty(NativeRetailRendererFrameRecord* node)noexcept{
 if(!node->queued)return;
 if(node->dirty_previous)node->dirty_previous->dirty_next=node->dirty_next;else dirty_first_=node->dirty_next;
 if(node->dirty_next)node->dirty_next->dirty_previous=node->dirty_previous;else dirty_last_=node->dirty_previous;
 node->dirty_previous=node->dirty_next=nullptr;node->queued=false;
}
bool NativeRetailRendererFrames::mark_dirty(const NativeRetailRendererFrameHandle& frame,RetailRendererFrameFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto node=record(frame,f);if(!node)return false;
 mark_record(node.get());return true;
}
void NativeRetailRendererFrames::rewrite_roots(NativeRetailRendererFrameRecord* first,NativeRetailRendererFrameRecord* root)noexcept{
 auto* node=first;
 for(;;){
  node->root=root;
  if(node->first_child){node=node->first_child;continue;}
  while(node!=first&&!node->next_sibling)node=node->parent;
  if(node==first)break;
  node=node->next_sibling;
 }
}
void NativeRetailRendererFrames::detach_record(NativeRetailRendererFrameRecord* node)noexcept{
 auto** link=&node->parent->first_child;
 while(*link!=node)link=&(*link)->next_sibling;
 *link=node->next_sibling;node->parent=nullptr;node->next_sibling=nullptr;
 rewrite_roots(node,node);mark_record(node);
}
bool NativeRetailRendererFrames::add_child(const NativeRetailRendererFrameHandle& parent,
 const NativeRetailRendererFrameHandle& child,RetailRendererFrameFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;
 const auto p=record(parent,f);if(!p)return false;const auto c=record(child,f);if(!c)return false;
 for(auto* ancestor=p.get();ancestor;ancestor=ancestor->parent)
  if(ancestor==c.get())return fail(f,RetailRendererFrameError::cycle,"0403 native hierarchy cycle refused");
 if(c->parent)detach_record(c.get());
 c->next_sibling=p->first_child;p->first_child=c.get();c->parent=p.get();
 rewrite_roots(c.get(),p->root);
 if(c->flags&3u){unlink_dirty(c.get());c->flags=static_cast<std::uint8_t>(c->flags&0xfcu);}
 mark_record(c.get());return true;
}
bool NativeRetailRendererFrames::detach(const NativeRetailRendererFrameHandle& frame,RetailRendererFrameFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto node=record(frame,f);if(!node)return false;
 if(!node->parent)return fail(f,RetailRendererFrameError::parent_required,"0403 detach requires an actual parent");
 detach_record(node.get());return true;
}
void NativeRetailRendererFrames::release_record(NativeRetailRendererFrameRecord* node)noexcept{
 unlink_dirty(node);node->alive=false;
 const auto found=std::find_if(records_.begin(),records_.end(),[node](const auto& owned){return owned.get()==node;});
 records_.erase(found);
}
bool NativeRetailRendererFrames::destroy_leaf(const NativeRetailRendererFrameHandle& frame,RetailRendererFrameFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto node=record(frame,f);if(!node)return false;
 if(node->first_child)return fail(f,RetailRendererFrameError::nonleaf_single_delete_required,"0403 single-delete with children requires its remaining native child-root contract");
 if(node->parent)detach_record(node.get());
 release_record(node.get());return true;
}
bool NativeRetailRendererFrames::destroy_root_hierarchy(const NativeRetailRendererFrameHandle& frame,RetailRendererFrameFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;
 if(frame.empty())return true; // Original2BF230 NULL input is an actual empty case.
 const auto retained=record(frame,f);if(!retained)return false;auto* node=retained.get();
 if(node->parent)return fail(f,RetailRendererFrameError::attached_hierarchy_required,"0403 attached hierarchy deletion requires verified caller detach");
 for(;;){
  if(node->first_child){node=node->first_child;continue;}
  auto* parent=node->parent;auto* next=node->next_sibling;
  if(parent)parent->first_child=next;
  release_record(node);
  if(!parent)break;
  node=next?next:parent;
 }
 return true;
}
bool NativeRetailRendererFrames::require_fully_synchronized(const NativeRetailRendererFrameHandle& frame,
 RetailRendererFrameFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto node=record(frame,f);if(!node)return false;
 if((node->flags&0x0fu)||(node->root->flags&3u))
  return fail(f,RetailRendererFrameError::synchronization_required,"0403 actual game frame/object synchronization required; dirty work not skipped");
 return true;
}
void NativeRetailRendererFrames::retire()noexcept{
 active_.store(false,std::memory_order_release);dirty_first_=dirty_last_=nullptr;
 for(const auto& node:records_)node->alive=false;
 records_.clear();
}
std::shared_ptr<NativeRetailRendererFrames> NativeRetailRendererCore::frames(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};
 if(!frames_){f.kind=RetailRendererCoreError::unbound_factory;f.operation="0403 native frame resource not constructed";return {};}
 return frames_;
}
std::optional<std::uint32_t> NativeRetailRendererCore::module_0403_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return module_0403_count_.load(std::memory_order_relaxed);
}
namespace renderer_required {
bool construct_core_plugin_0403(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!core.access(f))return false;
 if(core.frames_){f.kind=RetailRendererCoreError::reentrant;f.operation="0403 actual native resource already constructed";return false;}
 RetailRendererFrameFailure detail;auto resource=NativeRetailRendererFrames::create(core.session_,core.allocator_,detail);
 if(!resource){
  f.kind=detail.kind==RetailRendererFrameError::allocation||detail.allocator.heap.kind==memory::NativeHeapError::allocation_failed?
   RetailRendererCoreError::allocation:detail.kind==RetailRendererFrameError::native_exception?
   RetailRendererCoreError::native_exception:RetailRendererCoreError::allocator;
  f.operation=detail.operation;f.allocator=detail.allocator;return false;
 }
 core.frames_=std::move(resource);core.module_0403_count_.fetch_add(1,std::memory_order_relaxed);return true;
}
void destroy_core_plugin_0403(NativeRetailRendererCore& core)noexcept{
 if(core.frames_){core.frames_->retire();core.frames_.reset();core.module_0403_count_.fetch_sub(1,std::memory_order_relaxed);}
}
}
}
