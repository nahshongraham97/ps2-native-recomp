#include "mka/retail_renderer_images.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int fail_after=-1;bool fail_close{},inject_reader_close{};}
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
extern "C" int __real_fclose(std::FILE*);
extern "C" int __wrap_fclose(std::FILE* file){
 const bool injected=fail_close;fail_close=false;const auto result=__real_fclose(file);
 if(injected){errno=EIO;return EOF;}return result;
}
using namespace mka::retail;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
enum class Later{refuse,transfer,allow};Later later{Later::refuse};
NativeRetailMainState* transfer_main{};std::shared_ptr<NativeRetailRendererImages> captured;NativeRetailImageHandle captured_image;
unsigned module_cuts{},image_cuts{},pixel_cuts{},path_cuts{},format_cuts{},suffix_calls{};
bool fixture_construct(unsigned id,NativeRetailRendererCore& c,RetailRendererCoreFailure&){
 if(id==0x407&&later!=Later::allow){
  RetailRendererCoreFailure cf;captured=c.images(cf);check(bool(captured),"actual0406 before later fixture");
  RetailRendererImageFailure f;auto h=captured->create_image({3,2,32},f);check(h&&captured->allocate_pixels(*h,f),"actual image before later fixture");captured_image=*h;
  if(later==Later::transfer)reenter_native_retail_main(*transfer_main,83);return false;
 }
 if(later!=Later::allow)std::abort();return true;
}
struct Fixture{
 NativeRetailStartupState startup;mka::input::ControllerInput input;NativeRetailMainState main;
 StartupFailure sf;RetailRendererCoreFailure f;RetailRendererAllocatorFailure af;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
 explicit Fixture(const std::filesystem::path& root):startup(open(root),{}),main(startup,input){
  check(initialize_native_retail_game_session(startup,sf)&&initialize_retail_memory_owners(startup,sf),"actual Session/heaps");
  allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual native Permanent service");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& root){
  mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(root,f);check(bool(v),"actual native content");return std::move(*v);
 }
 std::shared_ptr<NativeRetailRendererCore> core(){auto c=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);check(bool(c),"actual persistent Core");return c;}
 void prepare(){auto c=core();auto ok=c->initialize_core(f);check(ok&&*ok&&construct_native_renderer_error(*c,f),"actual CoreInit/error resource");}
 std::shared_ptr<NativeRetailRendererImages> module(){prepare();auto c=core();check(renderer_required::construct_core_plugin_0406(*c,f),"ACTUAL0406 factory");return c->images(f);}
 std::size_t live(){RetailMemoryOwnerFailure f;auto o=startup.memory_owners->observe(RetailMemoryRole::permanent,f);check(bool(o),"actual Permanent observation");return o->heap.actual_live_allocations;}
 void tasks(){RetailGamePoolFailure pf;startup.game_pool=std::shared_ptr<NativeRetailGamePool>(NativeRetailGamePool::initialize(3,pf));
  RetailGameSessionFailure sf;RetailTaskDomainFailure df;auto* d=startup.game_session->task_domain(sf);
  check(startup.game_pool&&d&&d->reset_task_roots_from_12f0d0(startup.game_pool,df),"actual native task roots");}
};
NativeRetailImageHandle image(const std::shared_ptr<NativeRetailRendererImages>& m,RetailRendererImageDescription d={3,2,32}){
 RetailRendererImageFailure f;auto h=m->create_image(d,f);check(bool(h),"actual image birth");return *h;
}
struct Bytes:NativeRetailImageBytes{
 std::vector<std::byte> data;explicit Bytes(std::size_t n):data(n,std::byte{0x7b}){}
 std::span<std::byte> bytes()noexcept override{return data;}
};
std::shared_ptr<RetailRendererImageExtension> extension(unsigned id,std::vector<int>& events,bool accepted=true){
 auto e=std::make_shared<RetailRendererImageExtension>();e->id=id;
 e->construct=[&,id,accepted](auto&,auto,auto&,auto&){events.push_back(int(id));return accepted;};
 e->destroy=[&,id](auto&,auto,auto&){events.push_back(-int(id));};return e;
}
}
namespace mka::retail::renderer_required{
#define OTHER(ID,V) bool construct_core_plugin_##ID(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(V,c,f);} \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&)noexcept{if(later!=Later::allow)std::abort();}
OTHER(0407,0x407) OTHER(0408,0x408) OTHER(0409,0x409) OTHER(0412,0x412) OTHER(040a,0x40a) OTHER(040b,0x40b)
#undef OTHER
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&){std::abort();}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&){std::abort();}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&){std::abort();}
}
namespace mka::retail::scene_bootstrap_required{
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState&,std::shared_ptr<NativeRetailRendererCore> c,StartupFailure& f){
 ++suffix_calls;RetailRendererCoreFailure cf;check(!c->construct_registered(cf),"explicit later0407 refusal after actual0406");
 f={StartupError::memory_owners,"test-only0407/full renderer suffix refusal"};return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
int main(int argc,char** argv)try{
 check(argc==3,"new disposable root and independent gamma JSON required");
 const auto root=std::filesystem::absolute(argv[1]);check(std::filesystem::create_directory(root),"exclusive new image fixture");
 std::filesystem::create_directory(root/"a");std::filesystem::create_directory(root/"b");
 {std::ofstream(root/"a/test.raw",std::ios::binary)<<"R";std::ofstream(root/"b/test.raw",std::ios::binary)<<"K";}
 std::ifstream gold(argv[2]);std::string text((std::istreambuf_iterator<char>(gold)),{});auto cursor=text.find("\"tables\"");
 check(cursor!=std::string::npos,"independently derived gamma fixture");std::array<std::uint8_t,512> expected{};
 for(auto& v:expected){cursor=text.find_first_of("0123456789",cursor);check(cursor!=std::string::npos,"all gamma entries");unsigned n{};
  const auto result=std::from_chars(text.data()+cursor,text.data()+text.size(),n);check(result.ec==std::errc{}&&n<256,"fixture byte");v=std::uint8_t(n);cursor=std::size_t(result.ptr-text.data());}
 {
  Fixture x(root);auto c=x.core();check(c->module_0406_count(x.f)==0&&!c->images(x.f),"no image resource before actual factory");
  auto m=x.module();RetailRendererImageFailure f;check(c->module_0406_count(x.f)==1&&m->live_images(f)==0,"actual0406 ownership/count");
  for(bool inverse:{false,true}){auto table=m->gamma_table(inverse,f);check(bool(table),"real initialized gamma table");
   for(unsigned i=0;i<256;++i)check((*table)[i]==expected[(inverse?256:0)+i],"independent original-derived gamma1 bytes");}
  check(!m->set_gamma(2,f)&&f.kind==RetailRendererImageError::unverified_gamma_domain,"non1 gamma is not silently approximated");
  check(!renderer_required::construct_core_plugin_0406(*c,x.f)&&x.f.kind==RetailRendererCoreError::reentrant,"duplicate cannot replace live resource");
  struct Case{int depth;std::size_t stride,pixels,palette;};const Case cases[]{{4,4,8,64},{8,4,8,1024},{16,8,16,0},{24,12,24,0},{32,12,24,0}};
  auto diagnostics=c->construct_error_domain(x.f);check(bool(diagnostics),"actual shared native source error facade");
  const auto baseline=x.live();
  for(const auto& test:cases){
   auto h=image(m,{3,2,test.depth});auto birth=m->observe(h,f);check(birth&&!birth->stride&&birth->flags==0&&birth->pixel_bytes==0,"source birth leaves stride unavailable");
   check(m->allocate_pixels(h,f),"actual native pixel allocation");auto o=m->observe(h,f);
   check(o&&o->stride==test.stride&&o->pixel_bytes==test.pixels&&o->palette_bytes==test.palette&&o->flags==1,"independent source extents");
   std::vector<std::byte> bytes(test.pixels),palette(test.palette);
   for(std::size_t i=0;i<bytes.size();++i)bytes[i]=std::byte((i*37+255)&255);
   for(std::size_t i=0;i<palette.size();++i)palette[i]=std::byte((i*19+255)&255);
   check(m->write_bytes(h,false,0,bytes,f)&&m->write_bytes(h,true,0,palette,f),"actual bounded pixel/palette writes");
   const bool mapped=m->apply_gamma(h,f);
   check(mapped==(test.depth!=16&&test.depth!=24),"source gamma supported depths");
   if(!mapped)check(f.source_code==0x80000008u,"source unsupported-depth diagnostic");
   std::vector<std::byte> out(bytes.size()),pout(palette.size());
   check(m->read_bytes(h,false,0,out,f)&&m->read_bytes(h,true,0,pout,f)&&out==bytes&&pout==palette,"gamma1 RGB/alpha/indices/padding preserved");
   check(!m->allocate_pixels(h,f)&&f.kind==RetailRendererImageError::already_backed,"no guessed/leaking backing replacement");
   check(m->destroy_image(h,f)&&x.live()==baseline,"physical record/pixels release while weak handle survives");
   check(!m->observe(h,f)&&f.kind==RetailRendererImageError::retired_image,"retired handle cannot alias reused image");
  }
  (void)diagnostics->take(x.f);
  auto missing=image(m);check(!m->apply_gamma(missing,f)&&f.source_code==0x80000016u,"source missing-backing refusal");
  check(diagnostics->take(x.f)==RetailRendererErrorRecord{0,0x80000016u}&&m->destroy_image(missing,f),"real Core40F receives source-normalized image error");
  auto invalid=image(m,{-1,2,32});check(!m->allocate_pixels(invalid,f)&&f.kind==RetailRendererImageError::extent&&m->destroy_image(invalid,f),"negative extent named before allocation");
  auto zero=image(m,{0,0,4});check(m->allocate_pixels(zero,f)&&m->observe(zero,f)->palette_bytes==64&&m->destroy_image(zero,f),"zero indexed extent still owns source palette");
  auto borrowed=std::make_shared<Bytes>(10);auto weak=std::weak_ptr<Bytes>(borrowed);auto h=image(m,{1,2,32});
  check(m->bind_borrowed(h,5,borrowed,{},f)&&m->apply_gamma(h,f),"borrowed unaligned source stride with actual backing");
  borrowed.reset();check(!weak.expired()&&m->destroy_image(h,f)&&weak.expired(),"borrowed typed owner retained exactly through image lifetime");
  std::vector<NativeRetailImageHandle> many;for(unsigned i=0;i<64;++i)many.push_back(image(m));
  check(m->live_images(f)==64&&x.live()==baseline+64,"64 actual growing records without source pool cap");
  for(auto v:many)check(m->destroy_image(v,f),"actual image release");
  bool wrong=false;std::thread other([&]{RetailRendererImageFailure f;wrong=!m->live_images(f)&&f.kind==RetailRendererImageError::wrong_thread;});other.join();check(wrong,"thread guard");
  Fixture foreign(root);auto fm=foreign.module();auto fh=image(fm);check(!m->observe(fh,f)&&f.kind==RetailRendererImageError::foreign_owner,"actual owner identity");
  check(fm->destroy_image(fh,f),"foreign owner still usable");renderer_required::destroy_core_plugin_0406(*foreign.core());
  renderer_required::destroy_core_plugin_0406(*c);check(c->module_0406_count(x.f)==0&&!m->live_images(f)&&f.kind==RetailRendererImageError::retired,"real retirement/count");
 }
 {
  Fixture x(root);auto c=x.core();std::vector<int> events;
  auto original=extension(1,events);
  check(c->register_image_extension(original,x.f)&&c->register_image_extension(extension(2,events,false),x.f)&&
   c->register_image_extension(extension(3,events),x.f),"actual native extension definitions");
  original->construct=[](auto&,auto,auto&,auto&)->bool{std::abort();};
  original->destroy=[](auto&,auto,auto&){std::abort();};
  auto m=x.module();RetailRendererImageFailure f;auto h=image(m);
  check(events==std::vector<int>({1,2,-1})&&!m->observe(h,f)->extension_chain_completed,"source forward construction/reverse prefix rollback; image birth ignores refusal");
  check(m->allocate_pixels(h,f)&&m->destroy_image(h,f)&&events==std::vector<int>({1,2,-1,-3,-2,-1}),"source full reverse normal destruction even after ignored registry refusal");
  check(!c->register_image_extension(extension(4,events),x.f)&&x.f.kind==RetailRendererCoreError::registration_closed,"live definitions not replaced");
  renderer_required::destroy_core_plugin_0406(*c);
 }
 {
  Fixture x(root);x.tasks();auto c=x.core();std::vector<int> events;auto a=extension(1,events),b=extension(2,events);
  b->destroy=[&](auto& m,auto h,auto&){events.push_back(-2);RetailRendererImageFailure f;
   auto o=m.observe(h,f);check(o&&o->destroying&&o->pixel_bytes==0&&!(o->flags&1),"owned pixels released BEFORE extension destructor");
   reenter_native_retail_main(x.main,89);};
  check(c->register_image_extension(a,x.f)&&c->register_image_extension(b,x.f),"real transfer callbacks");
  auto m=x.module();auto h=image(m);RetailRendererImageFailure f;check(m->allocate_pixels(h,f),"owned backing before transfer");
  bool moved=false;try{(void)m->destroy_image(h,f);}catch(const detail::NativeMainReentry& e){moved=e.owner==&x.main&&e.value==89;}
  check(moved&&m->observe(h,f)->destroying&&main_binding::retail_task_exists(x.main,0x9876,x.sf)==false,"legitimate cleanup transfer preserves owner and next real task lookup");
  check(m->destroy_image(h,f)&&events==std::vector<int>({1,2,-2,-1}),"continued cleanup does not replay completed transfer effect");
  renderer_required::destroy_core_plugin_0406(*c);
 }
 {
  Fixture x(root);auto m=x.module();RetailRendererImageFailure f;std::vector<std::string> calls;NativeRetailImageHandle last_decoded;
  RetailRendererImageReader reader=[&](auto& images,std::string_view path,auto& file,auto& failure)->std::optional<NativeRetailImageHandle>{
   calls.emplace_back(path);char byte{};const auto read=file.read(&byte,1,failure.content);check(read.bytes==1&&!read.error,"real native file bytes reached codec");
   if(byte!='K')return {};
   auto h=images.create_image({1,1,32},failure);if(!h||!images.allocate_pixels(*h,failure))return {};
   const std::array<std::byte,4> rgba{std::byte(byte),std::byte{2},std::byte{3},std::byte{4}};
   check(images.write_bytes(*h,false,0,rgba,failure),"actual codec output backing");last_decoded=*h;
   if(inject_reader_close){inject_reader_close=false;fail_close=true;}return h;
  };
  check(m->register_format("RaW",reader,{},f)&&m->register_format("raw",{},{},f),"format duplicate preserves nonnull reader");
  check(m->register_format("unused",{},{},f)&&m->formats(f)->front().lower_extension==".unused","actual LIFO registration");
  check(!m->read_image("test.unused",f)&&f.kind==RetailRendererImageError::no_reader,"missing concrete reader named");
  check(!m->read_image("test.RaW",f)&&f.kind==RetailRendererImageError::no_format,"source matches lower or upper,not invented arbitrary case-fold lookup");
  check(m->set_search_path("missing/;a/;b/",f),"actual path growth");auto h=m->read_image("test.raw",f);
  check(h&&calls==std::vector<std::string>({"a/test.raw","b/test.raw"}),"existence miss and decoder refusal continue;actual success stops search");
  check(m->destroy_image(*h,f),"actual decoded image destruction");
  inject_reader_close=true;
  check(!m->read_image("test.raw",f)&&f.kind==RetailRendererImageError::content&&f.content.kind==mka::content::ContentVolumeError::close_failed,
   "real close fault is consumed/reported,not decode success or retry");
  check(m->destroy_image(last_decoded,f),"known decoded image remains owned after output close failure");
  check(!m->read_image("../test.raw",f)&&f.kind==RetailRendererImageError::content,"native content safety is not bypassed");
  check(m->set_search_path("a/",f),"source relative-path predicate fixture");
  check(!m->read_image("/b/test.raw",f)&&f.content.kind==mka::content::ContentVolumeError::invalid_key,
   "source forward slash alone is NOT absolute;candidate prefix remains applied");
  check(!m->read_image("cdrom0:b/test.raw",f)&&f.content.kind==mka::content::ContentVolumeError::foreign_prefix,
   "source arbitrary colon is NOT an ASCII drive prefix;no guessed direct-file selection");
  check(m->set_search_path("",f),"direct native filename selection");
  Fixture foreign(root);auto fm=foreign.module();auto fh=image(fm);
  check(m->register_format("raw",[&](auto&,auto,auto&,auto&)->std::optional<NativeRetailImageHandle>{return fh;},{},f),"actual foreign-result probe");
  check(!m->read_image("b/test.raw",f)&&f.kind==RetailRendererImageError::foreign_owner,"codec cannot publish a foreign image");
  check(fm->destroy_image(fh,f),"foreign result retained by its actual owner");renderer_required::destroy_core_plugin_0406(*foreign.core());
  renderer_required::destroy_core_plugin_0406(*x.core());
 }
 {
  Fixture x(root);x.tasks();auto c=x.core();std::vector<int> events;auto a=extension(1,events),b=extension(2,events);NativeRetailImageHandle live;
  b->construct=[&](auto&,auto h,auto&,auto&)->bool{events.push_back(2);live=h;reenter_native_retail_main(x.main,97);return false;};
  check(c->register_image_extension(a,x.f)&&c->register_image_extension(b,x.f),"actual constructor transfer callbacks");
  auto m=x.module();RetailRendererImageFailure f;bool moved=false;
  try{(void)m->create_image({1,1,32},f);}catch(const detail::NativeMainReentry& e){moved=e.owner==&x.main&&e.value==97;}
  check(moved&&m->observe(live,f)&&m->live_images(f)==1&&main_binding::retail_task_exists(x.main,0x9876,x.sf)==false,"constructor transfer preserves real image and next task lookup");
  check(m->destroy_image(live,f)&&events==std::vector<int>({1,2,-2,-1}),"constructor transfer cleanup retains actual typed owner");
  renderer_required::destroy_core_plugin_0406(*c);
 }
 {
  Fixture x(root);auto m=x.module();RetailRendererImageFailure f;unsigned calls=0;
  check(m->register_format("raw",[&](auto&,auto,auto&,auto&)->std::optional<NativeRetailImageHandle>{++calls;throw std::runtime_error("explicit after-effect fixture");},{},f),"actual throwing reader probe");
  check(!m->read_image("b/test.raw",f)&&f.kind==RetailRendererImageError::after_effect_unknown&&calls==1,"ordinary callback exception remains a named after-effect blocker");
  check(!m->read_image("b/test.raw",f)&&f.kind==RetailRendererImageError::after_effect_unknown&&calls==1,"uncertain callback is not replayed");
  renderer_required::destroy_core_plugin_0406(*x.core());
 }
 for(unsigned kind=0;kind<5;++kind){
  bool reached=false;
  for(int cut=0;cut<32;++cut){
   Fixture x(root);x.prepare();auto c=x.core();std::shared_ptr<NativeRetailRendererImages> m;
   NativeRetailImageHandle keep;RetailRendererImageFailure f;
   if(kind){check(renderer_required::construct_core_plugin_0406(*c,x.f),"resource for allocation cuts");m=c->images(x.f);}
   if(kind==1||kind==2)keep=image(m);
   if(kind==3)check(m->set_search_path("old",f),"prior path before growth refusal");
   const std::string large(2048,'x');const auto baseline=x.live();bool ok=false;std::optional<NativeRetailImageHandle> result;
   fail_after=cut;
   if(kind==0)ok=renderer_required::construct_core_plugin_0406(*c,x.f);
   if(kind==1){result=m->create_image({3,2,32},f);ok=bool(result);}
   if(kind==2)ok=m->allocate_pixels(keep,f);
   if(kind==3)ok=m->set_search_path(large,f);
   if(kind==4)ok=m->register_format("verylongextensionname",{},{},f);
   fail_after=-1;
   if(ok){reached=true;if(result)check(m->destroy_image(*result,f),"successful cut endpoint image");if(!keep.empty())check(m->destroy_image(keep,f),"retained image cleanup");
    renderer_required::destroy_core_plugin_0406(*c);break;}
   check(x.live()==baseline,"failed native admission releases all actual payload allocations");
   if(kind==0){++module_cuts;check(c->module_0406_count(x.f)==0&&!c->images(x.f),"factory failure publishes no native owner");}
   if(kind==1){++image_cuts;check(m->live_images(f)==1,"existing image retained after record allocation refusal");}
   if(kind==2){++pixel_cuts;auto o=m->observe(keep,f);check(o&&o->stride==12&&o->pixel_bytes==0&&o->flags==0,"source stride effect retained on allocation refusal");}
   if(kind==3){++path_cuts;check(m->search_path(f)=="old","growth refusal preserves actual prior path");}
   if(kind==4){++format_cuts;check(m->formats(f)->empty(),"format refusal publishes no false record");}
   if(m){if(!keep.empty())check(m->destroy_image(keep,f),"cut retained payload cleanup");renderer_required::destroy_core_plugin_0406(*c);}
  }
  check(reached,"all reached allocation cuts completed before deadline");
 }
 {
  Fixture x(root);later=Later::refuse;check(main_binding::initialize_retail_first_scene(x.main,x.sf)==RetailStartupGate::native_failure&&suffix_calls==1,"actual Main/Core prefix0403/4/5/6 then explicit0407 refusal");
  RetailRendererImageFailure f;check(captured&&!captured->observe(captured_image,f)&&f.kind==RetailRendererImageError::retired,"later failure retires actual0406 payloads");captured.reset();
 }
 {
  Fixture x(root);x.tasks();x.prepare();auto c=x.core();later=Later::transfer;transfer_main=&x.main;bool moved=false;
  try{(void)c->construct_registered(x.f);}catch(const detail::NativeMainReentry& e){moved=e.owner==&x.main&&e.value==83;}
  RetailRendererImageFailure f;check(moved&&captured&&captured->observe(captured_image,f)&&main_binding::retail_task_exists(x.main,0x9876,x.sf)==false,"Main transfer preserves actual0406 prefix and later task lookup");
  check(c->destroy_constructed(x.f)&&!captured->live_images(f),"real prefix cleanup once");captured.reset();transfer_main=nullptr;later=Later::refuse;
 }
 {
  std::shared_ptr<NativeRetailRendererImages> held;std::weak_ptr<NativeRetailGameSession> session;later=Later::allow;
  {Fixture x(root);x.prepare();auto c=x.core();check(c->construct_registered(x.f),"explicit later fixtures around real0406");held=c->images(x.f);(void)image(held);session=x.startup.game_session;}
  RetailRendererImageFailure f;check(session.expired()&&!held->live_images(f)&&f.kind==RetailRendererImageError::retired,"retained module cannot retain usable Session/payload");later=Later::refuse;
 }
 std::printf("PASS_NATIVE0406_SHARED_CPU_IMAGES; actual Core/Main prefix,512 independent gamma bytes,owned/borrowed extents,64 records,palette/alpha/stride,callback rollback/reverse order/Main transfer/no replay,real file decode/path,physical lifetime; allocation cuts module=%u image=%u pixel=%u path=%u format=%u; non1/codecs/write/clone/GPU/oracle/game OPEN; no window/audio\n",module_cuts,image_cuts,pixel_cuts,path_cuts,format_cuts);
 return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL native0406 images: %s\n",e.what());return 1;}
