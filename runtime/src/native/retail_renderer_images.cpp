#include "mka/retail_renderer_images.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <new>
namespace mka::retail {
namespace {
template<class T,class Create>std::shared_ptr<T> heap_object(
 const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,RetailRendererImageFailure& f,Create create)noexcept{
 static_assert(alignof(T)<=16);auto allocation=heap->allocate(sizeof(T),f.allocator);
 if(!allocation){f.kind=RetailRendererImageError::allocator;f.operation="0406 actual typed Permanent allocation";return {};}
 try{
  auto* object=create(allocation->data());
  return std::shared_ptr<T>(object,[heap,allocation=std::move(*allocation)](T* p)mutable{p->~T();allocation=memory::NativeAllocation{};});
 }catch(const std::bad_alloc&){f.kind=RetailRendererImageError::allocation;f.operation="0406 actual native ownership";}
 catch(...){f.kind=RetailRendererImageError::native_exception;f.operation="0406 native construction";}
 return {};
}
struct Layout {std::size_t row,stride,pixels,palette;};
std::optional<Layout> layout(RetailRendererImageDescription d)noexcept{
 if(d.width<0||d.height<0||d.depth<=0)return {};
 const auto maximum=std::numeric_limits<std::size_t>::max();
 const auto row=((std::uint64_t(d.depth)+7)/8)*std::uint64_t(d.width);
 if(row>maximum-3)return {};
 const auto stride=(std::size_t(row)+3)&~std::size_t(3);
 if(stride&&std::size_t(d.height)>maximum/stride)return {};
 const auto pixels=stride*std::size_t(d.height);
 const std::size_t palette=d.depth==4?64:d.depth==8?1024:0;
 if(pixels>maximum-palette)return {};
 return Layout{std::size_t(row),stride,pixels,palette};
}
struct OwnedBytes final:NativeRetailImageBytes {
 memory::NativeAllocation allocation;
 explicit OwnedBytes(memory::NativeAllocation&& a):allocation(std::move(a)){}
 std::span<std::byte> bytes()noexcept override{return {static_cast<std::byte*>(allocation.data()),allocation.requested_size()};}
};
std::string extension_key(std::string_view input,bool upper){
 std::string out(".");out.append(input.substr(0,18));
 for(auto& c:out)if(upper?(c>='a'&&c<='z'):(c>='A'&&c<='Z'))c=char(c+(upper?-32:32));
 return out;
}
}
struct NativeRetailImageRecord {
 RetailRendererImageDescription description;std::optional<std::size_t> stride;
 std::shared_ptr<NativeRetailImageBytes> pixels,palette;std::size_t pixel_length{},palette_offset{},palette_length{};
 std::vector<std::shared_ptr<NativeRetailImageExtensionState>> extensions;std::uint32_t flags{};
 bool alive{true},constructing{},extensions_complete{},destroying{},callout_active{};std::size_t destroy_cursor{};
 explicit NativeRetailImageRecord(RetailRendererImageDescription d):description(d){}
};
struct NativeRetailRendererImages::Format {
 std::string lower,upper;RetailRendererImageReader reader;RetailRendererImageWriter writer;
 Format(std::string l,std::string u):lower(std::move(l)),upper(std::move(u)){}
};
NativeRetailRendererImages::NativeRetailRendererImages(std::weak_ptr<NativeRetailGameSession> session,
 std::shared_ptr<NativeRetailRendererAllocatorService> heap,content::NativeContentVolume&& content,
 std::weak_ptr<NativeRetailRendererCore> core,std::vector<std::shared_ptr<const RetailRendererImageExtension>> extensions)
 :session_(std::move(session)),allocator_(std::move(heap)),content_(std::move(content)),core_(std::move(core)),extensions_(std::move(extensions)){}
NativeRetailRendererImages::~NativeRetailRendererImages(){retire();}
bool NativeRetailRendererImages::reject(RetailRendererImageFailure& f,RetailRendererImageError k,const char* op,std::uint32_t code)const noexcept{
 f.kind=k;f.operation=op;f.source_code=code;if(code)if(const auto core=core_.lock())core->store_error({1,code});return false;
}
std::shared_ptr<NativeRetailRendererImages> NativeRetailRendererImages::create(NativeRetailRendererCore& core,RetailRendererImageFailure& f)noexcept{
 f={};auto content=core.content_.retain(f.content);
 if(!content){f.kind=RetailRendererImageError::content;f.operation="0406 retained native content identity";return {};}
 RetailRendererCoreFailure cf;auto session=core.session_.lock();
 auto owner=NativeRetailRendererCore::for_session(session,core.allocator_,core.content_,cf);
 if(!owner){f.kind=RetailRendererImageError::owner;f.operation="0406 actual Core identity";return {};}
 std::shared_ptr<NativeRetailRendererImages> result;
 try{
  auto extensions=core.image_extensions_;
  result=heap_object<NativeRetailRendererImages>(core.allocator_,f,[&](void* p){
   return new(p)NativeRetailRendererImages(session,core.allocator_,std::move(*content),owner,std::move(extensions));
  });
 }catch(...){f.kind=RetailRendererImageError::allocation;f.operation="0406 extension ownership snapshot";return {};}
 if(!result)return {};
 // Initial capacity is a native detail,not a256-byte path/filename cap.
 auto path=core.allocator_->allocate(1,f.allocator);
 if(!path){result->reject(f,RetailRendererImageError::allocator,"0406 initial native path");return {};}
 result->path_=std::move(*path);*static_cast<char*>(result->path_.data())=0;
 if(!result->set_gamma(1.0f,f))return {};
 auto scratch=core.allocator_->allocate(1,f.allocator);
 if(!scratch){result->reject(f,RetailRendererImageError::allocator,"0406 initial native filename scratch");return {};}
 result->scratch_=std::move(*scratch);return result;
}
std::shared_ptr<NativeRetailGameSession> NativeRetailRendererImages::admit(RetailRendererImageFailure& f)const noexcept{
 f={};if(std::this_thread::get_id()!=thread_){reject(f,RetailRendererImageError::wrong_thread,"0406 owner thread");return {};}
 if(!active_){reject(f,RetailRendererImageError::retired,"0406 image service retired");return {};}
 auto lease=session_.lock();RetailGameSessionFailure sf;
 if(!lease||!lease->validate_current_thread(sf)){reject(f,RetailRendererImageError::owner,"0406 actual Session lifetime");return {};}
 if(after_effect_unknown_){reject(f,RetailRendererImageError::after_effect_unknown,"0406 prior callback effect cannot replay");return {};}
 return lease;
}
std::shared_ptr<NativeRetailImageRecord> NativeRetailRendererImages::record(const NativeRetailImageHandle& h,RetailRendererImageFailure& f)const noexcept{
 if(h.empty()){reject(f,RetailRendererImageError::invalid_handle,"0406 empty native image");return {};}
 if(h.owner_.lock().get()!=this){reject(f,RetailRendererImageError::foreign_owner,"0406 foreign image owner");return {};}
 auto r=h.record_.lock();if(!r||!r->alive){reject(f,RetailRendererImageError::retired_image,"0406 image allocation retired");return {};}return r;
}
std::optional<NativeRetailImageHandle> NativeRetailRendererImages::create_image(RetailRendererImageDescription d,RetailRendererImageFailure& f){
 const auto lease=admit(f);if(!lease)return {};
 auto r=heap_object<NativeRetailImageRecord>(allocator_,f,[&](void* p){return new(p)NativeRetailImageRecord(d);});if(!r)return {};
 try{r->extensions.resize(extensions_.size());records_.push_back(r);}
 catch(...){reject(f,RetailRendererImageError::allocation,"0406 image ownership publication");return {};}
 const NativeRetailImageHandle h(weak_from_this(),r);r->constructing=true;
 try{
  std::size_t done=0;
  for(;done<extensions_.size();++done)if(!extensions_[done]->construct(*this,h,r->extensions[done],f))break;
  r->extensions_complete=done==extensions_.size();
  if(!r->extensions_complete)while(done){--done;extensions_[done]->destroy(*this,h,r->extensions[done]);}
  r->constructing=false;f={};return h; // Source image caller ignores registry-null return.
 }catch(const detail::NativeMainReentry&){r->constructing=false;throw;}
 catch(...){r->constructing=false;after_effect_unknown_=true;reject(f,RetailRendererImageError::after_effect_unknown,"0406 constructor/rollback callback effect");return {};}
}
std::optional<RetailRendererImageObservation> NativeRetailRendererImages::observe(const NativeRetailImageHandle& h,RetailRendererImageFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};const auto r=record(h,f);if(!r)return {};
 return RetailRendererImageObservation{r->description,r->stride,r->pixel_length,r->palette_length,r->flags,r->extensions_complete,r->destroying};
}
std::optional<std::size_t> NativeRetailRendererImages::live_images(RetailRendererImageFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};return records_.size();
}
std::span<std::byte> NativeRetailRendererImages::region(const NativeRetailImageRecord& r,bool palette)const noexcept{
 const auto& owner=palette?r.palette:r.pixels;if(!owner)return {};
 auto bytes=owner->bytes();const auto offset=palette?r.palette_offset:0;const auto n=palette?r.palette_length:r.pixel_length;
 if(offset>bytes.size()||n>bytes.size()-offset)return {};return bytes.subspan(offset,n);
}
bool NativeRetailRendererImages::allocate_pixels(const NativeRetailImageHandle& h,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->destroying)return reject(f,RetailRendererImageError::reentrant,"0406 image destruction active");
 if(r->pixels)return reject(f,RetailRendererImageError::already_backed,"0406 existing backing;replacement contract required");
 const auto size=layout(r->description);if(!size)return reject(f,RetailRendererImageError::extent,"0406 unverified/overflowed image extent");
 r->stride=size->stride;r->pixel_length=0;
 auto allocation=allocator_->allocate(size->pixels+size->palette,f.allocator);
 if(!allocation||!allocation->data())return reject(f,RetailRendererImageError::allocator,"0406 actual pixel allocation refusal",0x80000013u);
 try{
  auto backing=std::make_shared<OwnedBytes>(std::move(*allocation));
  r->pixels=backing;r->pixel_length=size->pixels;r->palette=size->palette?backing:nullptr;
  r->palette_offset=size->pixels;r->palette_length=size->palette;r->flags|=1;return true;
 }catch(...){return reject(f,RetailRendererImageError::allocation,"0406 native pixel ownership",0x80000013u);}
}
bool NativeRetailRendererImages::bind_borrowed(const NativeRetailImageHandle& h,std::size_t stride,
 std::shared_ptr<NativeRetailImageBytes> pixels,std::shared_ptr<NativeRetailImageBytes> palette,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->destroying)return reject(f,RetailRendererImageError::reentrant,"0406 image destruction active");
 if(r->pixels)return reject(f,RetailRendererImageError::already_backed,"0406 prior image backing retained");
 const auto size=layout(r->description);
 if(!size||!pixels||stride<size->row||(stride&&std::size_t(r->description.height)>std::numeric_limits<std::size_t>::max()/stride))
  return reject(f,RetailRendererImageError::extent,"0406 borrowed image extent/owner");
 const auto n=stride*std::size_t(r->description.height);
 if(pixels->bytes().size()<n||(palette&&palette->bytes().size()<size->palette))return reject(f,RetailRendererImageError::extent,"0406 borrowed backing range");
 r->stride=stride;r->pixels=std::move(pixels);r->pixel_length=n;r->palette=std::move(palette);
 r->palette_offset=0;r->palette_length=r->palette?size->palette:0;return true;
}
bool NativeRetailRendererImages::write_bytes(const NativeRetailImageHandle& h,bool palette,std::size_t offset,
 std::span<const std::byte> input,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->destroying)return reject(f,RetailRendererImageError::reentrant,"0406 image destruction active");
 auto bytes=region(*r,palette);if(offset>bytes.size()||input.size()>bytes.size()-offset)return reject(f,RetailRendererImageError::extent,"0406 image write range");
 if(!input.empty())std::memmove(bytes.data()+offset,input.data(),input.size());return true;
}
bool NativeRetailRendererImages::read_bytes(const NativeRetailImageHandle& h,bool palette,std::size_t offset,
 std::span<std::byte> output,RetailRendererImageFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 auto bytes=region(*r,palette);if(offset>bytes.size()||output.size()>bytes.size()-offset)return reject(f,RetailRendererImageError::extent,"0406 image read range");
 if(!output.empty())std::memmove(output.data(),bytes.data()+offset,output.size());return true;
}
bool NativeRetailRendererImages::set_gamma(float input,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;
 if(std::bit_cast<std::uint32_t>(input)!=0x3f800000u)return reject(f,RetailRendererImageError::unverified_gamma_domain,"0406 only source-proved gamma1 is implemented");
 for(std::size_t i=0;i<256;++i)gamma_[i]=inverse_gamma_[i]=std::uint8_t(i);return true;
}
std::optional<std::array<std::uint8_t,256>> NativeRetailRendererImages::gamma_table(bool inverse,RetailRendererImageFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};return inverse?inverse_gamma_:gamma_;
}
bool NativeRetailRendererImages::apply_gamma(const NativeRetailImageHandle& h,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->destroying)return reject(f,RetailRendererImageError::reentrant,"0406 image destruction active");
 const auto depth=r->description.depth;
 if(depth!=4&&depth!=8&&depth!=32)return reject(f,RetailRendererImageError::unsupported_depth,"0406 source gamma depth",0x80000008u);
 auto bytes=region(*r,depth!=32);
 if((depth==32&&!r->pixels)||(depth!=32&&!r->palette))return reject(f,RetailRendererImageError::missing_backing,"0406 gamma backing required",0x80000016u);
 const auto map=[&](std::span<std::byte> row,std::size_t n){
  for(std::size_t i=0;i<n;++i)for(unsigned c=0;c<3;++c)row[i*4+c]=std::byte(gamma_[std::to_integer<std::uint8_t>(row[i*4+c])]);
 };
 if(depth==32){
  if(!r->stride||r->description.width<0)return reject(f,RetailRendererImageError::extent,"0406 actual stride/width required");
  const auto n=std::size_t(r->description.width);
  if(n>std::numeric_limits<std::size_t>::max()/4||n*4>*r->stride)return reject(f,RetailRendererImageError::extent,"0406 gamma row range");
  for(std::int32_t y=0;y<r->description.height;++y){
   const auto at=std::size_t(y)* *r->stride;
   if(at>bytes.size()||n*4>bytes.size()-at)return reject(f,RetailRendererImageError::extent,"0406 gamma backing changed");
   map(bytes.subspan(at,n*4),n);
  }
 }else{const auto n=std::size_t(1)<<depth;if(bytes.size()<n*4)return reject(f,RetailRendererImageError::extent,"0406 palette range");map(bytes,n);}
 r->flags|=2;return true;
}
bool NativeRetailRendererImages::destroy_image(const NativeRetailImageHandle& h,RetailRendererImageFailure& f){
 const auto lease=admit(f);if(!lease)return false;const auto r=record(h,f);if(!r)return false;
 if(r->constructing||r->callout_active)return reject(f,RetailRendererImageError::reentrant,"0406 callback owns current image");
 if(!r->destroying){
  if(r->flags&1){r->pixels.reset();r->palette.reset();r->pixel_length=r->palette_length=0;r->flags&=~1u;}
  r->destroying=true;r->destroy_cursor=extensions_.size();
 }
 try{
  while(r->destroy_cursor){const auto i=--r->destroy_cursor;r->callout_active=true;extensions_[i]->destroy(*this,h,r->extensions[i]);r->callout_active=false;}
 }catch(const detail::NativeMainReentry&){r->callout_active=false;throw;}
 catch(...){r->callout_active=false;after_effect_unknown_=true;return reject(f,RetailRendererImageError::after_effect_unknown,"0406 destructor callback effect cannot replay");}
 r->alive=false;records_.erase(std::find(records_.begin(),records_.end(),r));return true;
}
bool NativeRetailRendererImages::set_search_path(std::string_view input,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;
 if(input.find('\0')!=std::string_view::npos||input.size()==std::numeric_limits<std::size_t>::max())return reject(f,RetailRendererImageError::extent,"0406 native path value");
 if(input.size()+1>path_.requested_size()&&!allocator_->reallocate_in_place(path_,input.size()+1,f.allocator))
  return reject(f,RetailRendererImageError::allocator,"0406 search path growth",0x80000013u);
 if(!input.empty())std::memmove(path_.data(),input.data(),input.size());
 static_cast<char*>(path_.data())[input.size()]=0;path_length_=input.size();return true;
}
std::optional<std::string> NativeRetailRendererImages::search_path(RetailRendererImageFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};
 try{return std::string(static_cast<const char*>(path_.data()),path_length_);}
 catch(...){reject(f,RetailRendererImageError::allocation,"0406 path observation");return {};}
}
bool NativeRetailRendererImages::register_format(std::string_view extension,RetailRendererImageReader reader,
 RetailRendererImageWriter writer,RetailRendererImageFailure& f)noexcept{
 const auto lease=admit(f);if(!lease)return false;
 if(extension.find('\0')!=std::string_view::npos)return reject(f,RetailRendererImageError::extent,"0406 extension value");
 try{
  const auto lower=extension_key(extension,false);std::shared_ptr<Format> entry;
  for(const auto& x:formats_)if(x->lower==lower){entry=x;break;}
  if(!entry){entry=heap_object<Format>(allocator_,f,[&](void* p){return new(p)Format(lower,extension_key(extension,true));});if(!entry)return false;formats_.insert(formats_.begin(),entry);}
  if(reader)entry->reader=std::move(reader);if(writer)entry->writer=std::move(writer);return true;
 }catch(...){return reject(f,RetailRendererImageError::allocation,"0406 actual format ownership");}
}
std::optional<std::vector<RetailRendererImageFormatObservation>> NativeRetailRendererImages::formats(RetailRendererImageFailure& f)const noexcept{
 const auto lease=admit(f);if(!lease)return {};
 try{std::vector<RetailRendererImageFormatObservation> out;for(const auto& x:formats_)out.push_back({x->lower,x->upper,bool(x->reader),bool(x->writer)});return out;}
 catch(...){reject(f,RetailRendererImageError::allocation,"0406 format observation");return {};}
}
std::optional<NativeRetailImageHandle> NativeRetailRendererImages::read_image(std::string_view filename,RetailRendererImageFailure& f){
 const auto lease=admit(f);if(!lease)return {};
 if(filename.find('\0')!=std::string_view::npos){reject(f,RetailRendererImageError::extent,"0406 filename value");return {};}
 try{
  const auto slash=filename.find_last_of(":/\\");const auto dot=filename.find_last_of('.');
  if(dot==std::string_view::npos||(slash!=std::string_view::npos&&dot<slash)){reject(f,RetailRendererImageError::no_format,"0406 format extension required");return {};}
  std::shared_ptr<Format> format;
  for(const auto& x:formats_)if(filename.substr(dot)==x->lower||filename.substr(dot)==x->upper){format=x;break;}
  if(!format){reject(f,RetailRendererImageError::no_format,"0406 registered image format required");return {};}
  if(!format->reader){reject(f,RetailRendererImageError::no_reader,"0406 concrete image reader required");return {};}
  auto reader=format->reader;const auto paths=search_path(f);if(!paths)return {};
  // Actual2CD750:leading backslash OR ASCII drive letter followed by colon.
  const bool drive=filename.size()>=2&&((filename[0]>='A'&&filename[0]<='Z')||(filename[0]>='a'&&filename[0]<='z'))&&filename[1]==':';
  const bool direct=filename.starts_with('\\')||drive||paths->empty();
  std::vector<std::string> candidates;
  if(direct)candidates.emplace_back(filename);
  else{std::size_t begin=0;while(begin<paths->size()){const auto semi=paths->find(';',begin);
   candidates.push_back(paths->substr(begin,semi==std::string::npos?semi:semi-begin)+std::string(filename));
   if(semi==std::string::npos)break;begin=semi+1;}}
  for(const auto& candidate:candidates){
   if(candidate.size()==std::numeric_limits<std::size_t>::max()||
      (candidate.size()+1>scratch_.requested_size()&&!allocator_->reallocate_in_place(scratch_,candidate.size()+1,f.allocator))){
    reject(f,RetailRendererImageError::allocator,"0406 filename scratch growth",0x80000013u);return {};
   }
   std::memcpy(scratch_.data(),candidate.c_str(),candidate.size()+1);
   content::ContentVolumeFailure detail;auto file=content_.open_resource(candidate,detail);
   if(!file){
    if(detail.kind==content::ContentVolumeError::open_failed&&detail.host_error==std::errc::no_such_file_or_directory)continue;
    f.content=detail;reject(f,RetailRendererImageError::content,"0406 checked native image file");return {};
   }
   std::optional<NativeRetailImageHandle> image;
   try{image=reader(*this,candidate,*file,f);}
   catch(const detail::NativeMainReentry&){throw;}
   catch(...){after_effect_unknown_=true;reject(f,RetailRendererImageError::after_effect_unknown,"0406 reader callback effect cannot replay");return {};}
   if(!file->close_checked(detail)){f.content=detail;reject(f,RetailRendererImageError::content,"0406 checked reader close");return {};}
   if(image){
    const auto decoded=record(*image,f);if(!decoded)return {};
    if(decoded->destroying){reject(f,RetailRendererImageError::callback_result,"0406 reader returned a destroying image");return {};}
    f={};return image;
   }
   if(f.kind!=RetailRendererImageError::none)return {};
  }
  reject(f,RetailRendererImageError::not_found,"0406 no candidate decoded an image");return {};
 }catch(const detail::NativeMainReentry&){throw;}
 catch(const std::bad_alloc&){reject(f,RetailRendererImageError::allocation,"0406 filename/callout ownership");return {};}
 catch(...){reject(f,RetailRendererImageError::native_exception,"0406 native filename/content operation");return {};}
}
void NativeRetailRendererImages::retire()noexcept{
 active_=false;
 // Module destruction frees resources,not per-image semantic callbacks.
 for(const auto& r:records_)r->alive=false;records_.clear();formats_.clear();extensions_.clear();
 path_=memory::NativeAllocation{};scratch_=memory::NativeAllocation{};
}
std::shared_ptr<NativeRetailRendererImages> NativeRetailRendererCore::images(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};if(!images_){f.kind=RetailRendererCoreError::unbound_factory;f.operation="0406 actual image resource required";return {};}return images_;
}
std::optional<std::uint32_t> NativeRetailRendererCore::module_0406_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return images_?1u:0u;
}
bool NativeRetailRendererCore::register_image_extension(std::shared_ptr<const RetailRendererImageExtension> extension,RetailRendererCoreFailure& f)noexcept{
 if(!access(f))return false;
 if(sealed_||images_){f.kind=RetailRendererCoreError::registration_closed;f.operation="0406 image extension definitions sealed";return false;}
 if(!extension||!extension->construct||!extension->destroy){f.kind=RetailRendererCoreError::unbound_factory;f.operation="0406 real native extension callouts required";return false;}
 for(const auto& x:image_extensions_)if(x->id==extension->id)return true;
 try{image_extensions_.push_back(std::make_shared<const RetailRendererImageExtension>(*extension));return true;}
 catch(...){f.kind=RetailRendererCoreError::allocation;f.operation="0406 native extension definition";return false;}
}
namespace renderer_required {
bool construct_core_plugin_0406(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!core.access(f))return false;
 if(core.images_){f.kind=RetailRendererCoreError::reentrant;f.operation="0406 live image resource already published";return false;}
 RetailRendererImageFailure detail;auto owner=NativeRetailRendererImages::create(core,detail);
 if(!owner){f.kind=detail.kind==RetailRendererImageError::allocation||detail.allocator.heap.kind==memory::NativeHeapError::allocation_failed?
   RetailRendererCoreError::allocation:RetailRendererCoreError::allocator;f.operation=detail.operation;f.allocator=detail.allocator;f.content=detail.content;return false;}
 core.images_=std::move(owner);return true;
}
void destroy_core_plugin_0406(NativeRetailRendererCore& core)noexcept{
 if(core.images_){core.images_->retire();core.images_.reset();}
}
}
}
