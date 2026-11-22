#include "mka/retail_renderer_streams.hpp"
#include "mka/retail_game_session.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
bool fail(RetailRendererStreamFailure& f,RetailRendererStreamError kind,const char* operation)noexcept{
 f.kind=kind;f.operation=operation;return false;
}
struct Busy {bool& flag;explicit Busy(bool& b):flag(b){flag=true;}~Busy(){flag=false;}};
template<class T,class Create>std::shared_ptr<T> heap_object(
 const std::shared_ptr<NativeRetailRendererAllocatorService>& allocator,RetailRendererStreamFailure& f,Create create)noexcept{
 static_assert(alignof(T)<=16);auto storage=allocator->allocate(sizeof(T),f.allocator);
 if(!storage){fail(f,RetailRendererStreamError::allocator,"0404 actual native heap allocation");return {};}
 try{
  auto* object=create(storage->data());
  return std::shared_ptr<T>(object,[heap=std::shared_ptr<NativeRetailRendererAllocatorService>(allocator),
      storage=std::move(*storage)](T* p)mutable{p->~T();storage=memory::NativeAllocation{};heap.reset();});
 }catch(const std::bad_alloc&){fail(f,RetailRendererStreamError::allocation,"0404 native control ownership");}
 catch(...){fail(f,RetailRendererStreamError::native_exception,"0404 native resource construction");}
 return {};
}
}
struct NativeRetailRendererStreamRecord {
 RetailRendererStreamKind kind{RetailRendererStreamKind::memory_read};bool alive{true};
 std::shared_ptr<const std::vector<std::uint8_t>> memory;
 std::uint64_t memory_position{};
 std::shared_ptr<content::NativeContentFile> borrowed;
 std::optional<content::NativeContentFile> owned;
 std::weak_ptr<NativeRetailRendererStreams> owner;
 std::weak_ptr<NativeRetailRendererStreamRecord> self;
 content::NativeContentFile* file()noexcept{return owned?&*owned:borrowed.get();}
};
NativeRetailRendererStreams::NativeRetailRendererStreams(std::weak_ptr<NativeRetailGameSession> session,
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator,content::NativeContentVolume&& content,
 std::shared_ptr<NativeRetailRendererErrorDomain> errors)noexcept
 :session_(std::move(session)),allocator_(std::move(allocator)),content_(std::move(content)),errors_(std::move(errors)){}
NativeRetailRendererStreams::~NativeRetailRendererStreams(){(void)retire();}
std::shared_ptr<NativeRetailRendererStreams> NativeRetailRendererStreams::create(
 std::weak_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
 const content::NativeContentVolume& content,std::shared_ptr<NativeRetailRendererErrorDomain> errors,
 RetailRendererStreamFailure& f)noexcept{
 f={};const auto lease=session.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)||!allocator){fail(f,RetailRendererStreamError::owner,"0404 actual Session/heap required");return {};}
 if(!errors){fail(f,RetailRendererStreamError::error_domain,"0404 actual prior40F error domain required");return {};}
 auto retained=content.retain(f.content);
 if(!retained){fail(f,RetailRendererStreamError::content,"0404 actual retained native content volume");return {};}
 return heap_object<NativeRetailRendererStreams>(allocator,f,[&](void* p){
  return new(p)NativeRetailRendererStreams(session,allocator,std::move(*retained),std::move(errors));
 });
}
std::shared_ptr<NativeRetailGameSession> NativeRetailRendererStreams::admit(
 RetailRendererStreamFailure& f,bool cleanup)const noexcept{
 f={};
 if(std::this_thread::get_id()!=thread_){fail(f,RetailRendererStreamError::wrong_thread,"0404 stream owner thread");return {};}
 if(!active_.load(std::memory_order_acquire)){fail(f,RetailRendererStreamError::retired,"0404 actual stream resource retired");return {};}
 auto lease=session_.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)){fail(f,RetailRendererStreamError::owner,"0404 actual Session lifetime");return {};}
 if(busy_){fail(f,RetailRendererStreamError::reentrant,"0404 stream operation active");return {};}
 if(after_effect_failure_&&!cleanup){fail(f,RetailRendererStreamError::after_effect_failure,"0404 prior report failure after I/O; no replay");return {};}
 return lease;
}
std::shared_ptr<NativeRetailRendererStreamRecord> NativeRetailRendererStreams::record(
 const NativeRetailRendererStreamHandle& h,RetailRendererStreamFailure& f)const noexcept{
 if(h.empty()){fail(f,RetailRendererStreamError::invalid_handle,"0404 empty native stream handle");return {};}
 if(h.owner_.lock().get()!=this){fail(f,RetailRendererStreamError::foreign_owner,"0404 foreign stream resource");return {};}
 auto r=h.record_.lock();
 if(!r||!r->alive){fail(f,RetailRendererStreamError::retired_stream,"0404 stream already consumed");return {};}
 return r;
}
std::shared_ptr<NativeRetailRendererStreamRecord> NativeRetailRendererStreams::prepare_record(RetailRendererStreamFailure& f)noexcept{
 auto r=heap_object<NativeRetailRendererStreamRecord>(allocator_,f,[](void* p){return new(p)NativeRetailRendererStreamRecord;});
 if(!r)return {};
 try{
  if(records_.size()==records_.max_size()){fail(f,RetailRendererStreamError::allocation,"0404 host stream ownership capacity");return {};}
  if(records_.size()==records_.capacity()){
   const auto maximum=records_.max_size();
   records_.reserve(records_.empty()?1:records_.size()<=maximum/2?records_.size()*2:maximum);
  }
  r->owner=weak_from_this();r->self=r;return r;
 }catch(const std::bad_alloc&){fail(f,RetailRendererStreamError::allocation,"0404 native record retention");}
 catch(...){fail(f,RetailRendererStreamError::native_exception,"0404 native record admission");}
 return {};
}
NativeRetailRendererStreamHandle NativeRetailRendererStreams::publish(std::shared_ptr<NativeRetailRendererStreamRecord> r)noexcept{
 const NativeRetailRendererStreamHandle h(r->owner,r->self);records_.push_back(std::move(r));return h;
}
bool NativeRetailRendererStreams::report(std::uint32_t code,RetailRendererStreamFailure& f)noexcept{
 if(errors_->store({1,code},f.error_domain))return true;
 after_effect_failure_=true;return fail(f,RetailRendererStreamError::error_domain,"0404 actual error report failed after effect");
}
std::optional<NativeRetailRendererStreamHandle> NativeRetailRendererStreams::open_memory_read(
 std::shared_ptr<const std::vector<std::uint8_t>> bytes,RetailRendererStreamFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return {};
 if(!bytes){fail(f,RetailRendererStreamError::invalid_backing,"0404 explicit immutable memory owner required");return {};}
 Busy guard(busy_);auto r=prepare_record(f);if(!r)return {};
 r->memory=std::move(bytes);return publish(std::move(r));
}
std::optional<NativeRetailRendererStreamHandle> NativeRetailRendererStreams::open_resource_read(
 std::string_view key,RetailRendererStreamFailure& f){
 const auto lease=admit(f);if(!lease)return {};Busy guard(busy_);
 auto r=prepare_record(f);if(!r)return {};
 // All host record/control ownership is secured BEFORE opening the file.
 auto file=content_.open_resource(key,f.content);
 if(!file){fail(f,RetailRendererStreamError::content,"0404 native resource read open");(void)report(0x80000002u,f);return {};}
 r->kind=RetailRendererStreamKind::owned_file_read;r->owned=std::move(*file);return publish(std::move(r));
}
std::optional<NativeRetailRendererStreamHandle> NativeRetailRendererStreams::adopt_borrowed_file_read(
 std::shared_ptr<content::NativeContentFile> file,RetailRendererStreamFailure& f){
 const auto lease=admit(f);if(!lease)return {};
 if(!file){fail(f,RetailRendererStreamError::invalid_backing,"0404 actual borrowed native file owner required");return {};}
 Busy guard(busy_);auto r=prepare_record(f);if(!r)return {};
 if(!file->position(f.content)){fail(f,RetailRendererStreamError::content,"0404 actual borrowed file position validation");return {};}
 r->kind=RetailRendererStreamKind::borrowed_file_read;r->borrowed=std::move(file);return publish(std::move(r));
}
std::optional<RetailRendererStreamRead> NativeRetailRendererStreams::read(
 const NativeRetailRendererStreamHandle& h,std::span<std::uint8_t> out,RetailRendererStreamFailure& f){
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 Busy guard(busy_);RetailRendererStreamRead result;
 if(r->kind==RetailRendererStreamKind::memory_read){
  const auto remaining=r->memory->size()-r->memory_position;
  const auto count=static_cast<std::size_t>(std::min<std::uint64_t>(remaining,out.size()));
  if(count<out.size()){result.source_diagnostic=5;result.eof=true;if(!report(5,f))return {};}
  if(count)std::memcpy(out.data(),r->memory->data()+r->memory_position,count);
  r->memory_position+=count;result.bytes=count;return result;
 }
 const auto actual=r->file()->read(out.data(),out.size(),f.content);
 result.bytes=actual.bytes;result.eof=actual.eof;result.host_error=actual.error;
 if(actual.error)fail(f,RetailRendererStreamError::content,"0404 actual native file read");
 if(actual.bytes!=out.size()){
  result.source_diagnostic=actual.eof?5u:0x8000001au;
  if(!report(*result.source_diagnostic,f))return {};
 }
 return result;
}
bool NativeRetailRendererStreams::skip_forward(
 const NativeRetailRendererStreamHandle& h,std::uint64_t bytes,RetailRendererStreamFailure& f){
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(!bytes)return true; // Source early return, no backing dispatch.
 Busy guard(busy_);
 if(r->kind==RetailRendererStreamKind::memory_read){
  const auto remaining=r->memory->size()-r->memory_position;
  if(bytes>remaining){
   constexpr auto word_max=std::numeric_limits<std::uint32_t>::max();
   if(bytes<=word_max&&r->memory_position<=word_max&&bytes>word_max-r->memory_position)
    return fail(f,RetailRendererStreamError::source_wrapping_skip_unbound,"0404 source32 wrapping skip outside backing remains unadopted");
   r->memory_position=r->memory->size();
   if(!report(5,f))return false;
   return false; // Original overshoot clamps to end and reports code5.
  }
  r->memory_position+=bytes;return true;
 }
 if(bytes>std::uint64_t(std::numeric_limits<std::int64_t>::max()))
  return fail(f,RetailRendererStreamError::native_range,"0404 skip exceeds native signed64 file API");
 if(!r->file()->seek(static_cast<std::int64_t>(bytes),content::SeekOrigin::current,f.content)){
  fail(f,RetailRendererStreamError::content,"0404 actual native forward seek");
  content::ContentVolumeFailure status;const auto eof=r->file()->end_of_file(status);
  if(eof&&*eof)(void)report(5,f);
  return false;
 }
 return true;
}
void NativeRetailRendererStreams::release_record(NativeRetailRendererStreamRecord* r)noexcept{
 r->alive=false;const auto i=std::find_if(records_.begin(),records_.end(),[r](const auto& x){return x.get()==r;});records_.erase(i);
}
bool NativeRetailRendererStreams::close(const NativeRetailRendererStreamHandle& h,RetailRendererStreamFailure& f)noexcept{
 const auto lease=admit(f,true);if(!lease)return false;const auto r=record(h,f);if(!r)return false;Busy guard(busy_);
 bool ok=true;
 if(r->kind==RetailRendererStreamKind::owned_file_read&&!r->owned->close_checked(f.content))
  ok=fail(f,RetailRendererStreamError::content,"0404 checked native file close failed; stream consumed");
 release_record(r.get());return ok;
}
std::optional<RetailRendererStreamObservation> NativeRetailRendererStreams::observe(
 const NativeRetailRendererStreamHandle& h,RetailRendererStreamFailure& f){
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 if(r->kind==RetailRendererStreamKind::memory_read)return RetailRendererStreamObservation{r->kind,r->memory_position};
 const auto position=r->file()->position(f.content);
 if(!position){fail(f,RetailRendererStreamError::content,"0404 actual file cursor observation");return {};}
 return RetailRendererStreamObservation{r->kind,*position};
}
std::optional<std::size_t> NativeRetailRendererStreams::live_streams(RetailRendererStreamFailure& f)const noexcept{
 const auto lease=admit(f,true);if(!lease)return {};return records_.size();
}
std::optional<RetailRendererErrorRecord> NativeRetailRendererStreams::take_source_error(RetailRendererStreamFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return {};
 auto result=errors_->take(f.error_domain);
 if(!result)fail(f,RetailRendererStreamError::error_domain,"0404 actual40F error observation");
 return result;
}
content::ContentVolumeFailure NativeRetailRendererStreams::retire()noexcept{
 active_.store(false,std::memory_order_release);content::ContentVolumeFailure first;
 for(const auto& r:records_){
  if(r->kind==RetailRendererStreamKind::owned_file_read){
   content::ContentVolumeFailure detail;
   if(!r->owned->close_checked(detail)&&first.kind==content::ContentVolumeError::none)first=detail;
  }
  r->alive=false;
 }
 records_.clear();errors_.reset();return first;
}
std::shared_ptr<NativeRetailRendererStreams> NativeRetailRendererCore::streams(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};
 if(!streams_){f.kind=RetailRendererCoreError::unbound_factory;f.operation="0404 native stream resource not constructed";return {};}
 return streams_;
}
std::optional<std::uint32_t> NativeRetailRendererCore::module_0404_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return module_0404_count_.load(std::memory_order_relaxed);
}
namespace renderer_required {
bool construct_core_plugin_0404(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!core.access(f))return false;
 if(core.streams_){f.kind=RetailRendererCoreError::reentrant;f.operation="0404 actual resource already constructed";return false;}
 RetailRendererStreamFailure detail;auto resource=NativeRetailRendererStreams::create(core.session_,core.allocator_,core.content_,core.error_domain_,detail);
 if(!resource){
  f.kind=detail.kind==RetailRendererStreamError::content?RetailRendererCoreError::content:
   detail.kind==RetailRendererStreamError::allocation||detail.allocator.heap.kind==memory::NativeHeapError::allocation_failed?RetailRendererCoreError::allocation:
   detail.kind==RetailRendererStreamError::native_exception?RetailRendererCoreError::native_exception:
   detail.kind==RetailRendererStreamError::error_domain?RetailRendererCoreError::construction:RetailRendererCoreError::allocator;
  f.operation=detail.operation;f.content=detail.content;f.allocator=detail.allocator;return false;
 }
 core.streams_=std::move(resource);core.module_0404_count_.fetch_add(1,std::memory_order_relaxed);return true;
}
void destroy_core_plugin_0404(NativeRetailRendererCore& core)noexcept{
 if(!core.streams_)return;
 const auto failure=core.streams_->retire();
 if(failure.kind!=content::ContentVolumeError::none){
  if(core.cleanup_content_failure_.kind==content::ContentVolumeError::none)core.cleanup_content_failure_=failure;
  core.after_effect_unknown_=true;
 }
 core.streams_.reset();core.module_0404_count_.fetch_sub(1,std::memory_order_relaxed);
}
}
}
