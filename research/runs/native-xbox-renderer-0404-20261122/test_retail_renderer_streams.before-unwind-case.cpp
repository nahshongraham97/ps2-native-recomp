#include "mka/retail_renderer_streams.hpp"
#include "mka/retail_renderer_frames.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int fail_after=-1;bool fail_close{},short_read{};unsigned close_calls{};}
void* operator new(std::size_t n){if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
extern "C" int __real_fclose(std::FILE*);
extern "C" std::size_t __real_fread(void*,std::size_t,std::size_t,std::FILE*);
extern "C" int __wrap_fclose(std::FILE* f){
 ++close_calls;const bool injected=fail_close;fail_close=false;const auto result=__real_fclose(f);
 if(injected){errno=EIO;return EOF;}return result;
}
extern "C" std::size_t __wrap_fread(void* out,std::size_t size,std::size_t count,std::FILE* f){
 if(short_read){short_read=false;return __real_fread(out,size,std::min<std::size_t>(count,2),f);}
 return __real_fread(out,size,count,f);
}
using namespace mka::retail;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
enum class Later {refuse,transfer,allow};Later later{Later::refuse};
std::shared_ptr<NativeRetailRendererStreams> captured;NativeRetailRendererStreamHandle captured_stream;
NativeRetailMainState* transfer_main{};unsigned suffix_calls{},module_cuts{},record_cuts{};
auto memory_bytes(){return std::make_shared<const std::vector<std::uint8_t>>(std::initializer_list<std::uint8_t>{1,2,3,4,5});}
bool fixture_construct(unsigned id,NativeRetailRendererCore& core,RetailRendererCoreFailure&){
 if(id==0x405&&later!=Later::allow){
  RetailRendererCoreFailure cf;captured=core.streams(cf);check(bool(captured),"later fixture sees real0404 resource");
  RetailRendererStreamFailure f;auto h=captured->open_memory_read(memory_bytes(),f);
  check(bool(h),"real stream allocation before later fixture");captured_stream=*h;
  if(later==Later::transfer)reenter_native_retail_main(*transfer_main,71);
  return false;
 }
 if(later!=Later::allow)std::abort();return true;
}
void fixture_destroy(unsigned)noexcept{if(later!=Later::allow)std::abort();}
struct Fixture {
 NativeRetailStartupState startup;mka::input::ControllerInput input;NativeRetailMainState main;
 StartupFailure sf;RetailRendererCoreFailure f;RetailRendererAllocatorFailure af;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;
 explicit Fixture(const std::filesystem::path& root):startup(open(root),{}),main(startup,input){
  check(initialize_native_retail_game_session(startup,sf)&&initialize_retail_memory_owners(startup,sf),"actual Session/native heaps");
  allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual native allocator");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& root){mka::content::ContentVolumeFailure f;
  auto v=mka::content::NativeContentVolume::open(root,f);check(bool(v),"actual fixture volume");return std::move(*v);}
 std::shared_ptr<NativeRetailRendererCore> core(){auto c=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);check(bool(c),"actual Core");return c;}
 void prepare(){auto c=core();auto initialized=c->initialize_core(f);check(initialized&&*initialized&&construct_native_renderer_error(*c,f),"actual CoreInit and40F");}
 std::shared_ptr<NativeRetailRendererStreams> module(){prepare();auto c=core();check(renderer_required::construct_core_plugin_0404(*c,f),"ACTUAL0404 factory");return c->streams(f);}
 std::size_t live(){RetailMemoryOwnerFailure f;auto x=startup.memory_owners->observe(RetailMemoryRole::permanent,f);check(bool(x),"actual heap observation");return x->heap.actual_live_allocations;}
 void tasks(){RetailGamePoolFailure pf;startup.game_pool=std::shared_ptr<NativeRetailGamePool>(NativeRetailGamePool::initialize(3,pf));
  RetailGameSessionFailure sf;RetailTaskDomainFailure df;auto* d=startup.game_session->task_domain(sf);
  check(startup.game_pool&&d&&d->reset_task_roots_from_12f0d0(startup.game_pool,df),"actual task roots");}
};
void expect_error(const std::shared_ptr<NativeRetailRendererStreams>& m,RetailRendererErrorRecord expected){
 RetailRendererStreamFailure f;auto e=m->take_source_error(f);check(e&&*e==expected,"independent original40F diagnostic");
}
}
namespace mka::retail::renderer_required {
#define OTHER(ID,VALUE) bool construct_core_plugin_##ID(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(VALUE,c,f);} \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&)noexcept{fixture_destroy(VALUE);}
OTHER(0405,0x405) OTHER(0406,0x406) OTHER(0407,0x407) OTHER(0408,0x408)
OTHER(0409,0x409) OTHER(0412,0x412) OTHER(040a,0x40a) OTHER(040b,0x40b)
#undef OTHER
//0403/0404/registration are production. All unimplemented math must not execute.
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&){std::abort();}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&){std::abort();}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&){std::abort();}
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState&,std::shared_ptr<NativeRetailRendererCore> c,StartupFailure& f){
 ++suffix_calls;RetailRendererCoreFailure cf;check(!c->construct_registered(cf),"explicit later0405 refusal after real0403/0404");
 f={StartupError::memory_owners,"test-only later0405/full suffix refusal"};return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}
int main(int argc,char** argv)try{
 check(argc==2,"NEW disposable fixture directory required");
 const auto root=std::filesystem::absolute(argv[1]);check(std::filesystem::create_directory(root),"exclusive new stream fixture directory");
 {std::ofstream out(root/"data.bin",std::ios::binary);out<<"ABCDEFGHIJ";check(bool(out),"owned fixture bytes");}
#ifdef _WIN32
 {auto h=CreateFileW((root/"sparse.bin").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
  check(h!=INVALID_HANDLE_VALUE,"exclusive sparse fixture");DWORD n{};check(DeviceIoControl(h,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&n,nullptr),"host sparse support");
  LARGE_INTEGER p;p.QuadPart=(std::int64_t(1)<<32)+36;check(SetFilePointerEx(h,p,nullptr,FILE_BEGIN),"native64 sparse cursor");
  const char z='Z';check(WriteFile(h,&z,1,&n,nullptr)&&n==1&&CloseHandle(h),"owned sparse tail");}
#endif
 {
  Fixture x(root);auto c=x.core();check(!c->streams(x.f)&&c->module_0404_count(x.f)==0,"no resource before constructor");
  auto m=x.module();RetailRendererStreamFailure f;check(m&&c->module_0404_count(x.f)==1&&m->live_streams(f)==0,"real resource publication/empty birth");
  check(!renderer_required::construct_core_plugin_0404(*c,x.f)&&x.f.kind==RetailRendererCoreError::reentrant,"duplicate preserves actual resource");
  const auto data=memory_bytes();const auto baseline=x.live();auto h=m->open_memory_read(data,f);
  check(h&&x.live()==baseline+1,"real heap-backed stream record");std::array<std::uint8_t,8> out{};
  auto r=m->read(*h,std::span(out).first(2),f);check(r&&r->bytes==2&&!r->source_diagnostic&&out[0]==1&&out[1]==2,"actual memory bytes/cursor");
  check(m->skip_forward(*h,1,f)&&m->observe(*h,f)->position==3,"source forward memory cursor");
  check(!m->skip_forward(*h,0xffffffffu,f)&&f.kind==RetailRendererStreamError::source_wrapping_skip_unbound&&m->observe(*h,f)->position==3,"unadopted source wrap refuses before effects");
  r=m->read(*h,out,f);check(r&&r->bytes==2&&r->eof&&r->source_diagnostic==5&&out[0]==4&&out[1]==5&&m->observe(*h,f)->position==5,"source partial memory read preserves actual bytes");
  expect_error(m,{1,5});
  check(m->close(*h,f)&&x.live()==baseline&&!m->observe(*h,f)&&f.kind==RetailRendererStreamError::retired_stream,"close physically frees record while weak handle survives");
  auto over=m->open_memory_read(data,f);check(over&&!m->skip_forward(*over,99,f)&&m->observe(*over,f)->position==5,"source overshoot clamps before failure");
  expect_error(m,{1,5});check(m->skip_forward(*over,0,f)&&m->close(*over,f),"actual zero-skip/close cases");
  auto empty=m->open_memory_read(std::make_shared<const std::vector<std::uint8_t>>(),f);
  check(empty&&m->read(*empty,{},f)->bytes==0,"original empty memory/zero read");expect_error(m,{0,0x80000000u});check(m->close(*empty,f),"empty close");
  std::vector<NativeRetailRendererStreamHandle> many;for(unsigned i=0;i<64;++i){auto v=m->open_memory_read(data,f);check(bool(v),"real host-growing stream resource");many.push_back(*v);}
  check(m->live_streams(f)==64,"no16-record console pool");for(const auto& v:many)check(m->close(v,f),"real stream storage release");
  check(x.live()==baseline,"all64 payloads released with weak handles retained");
  auto file=m->open_resource_read("data.bin",f);check(bool(file),"actual owned native filename read");
  r=m->read(*file,std::span(out).first(3),f);check(r&&r->bytes==3&&out[0]=='A'&&out[2]=='C',"native file byte identity");
  check(m->skip_forward(*file,5,f),"native file forward seek");r=m->read(*file,out,f);
  check(r&&r->bytes==2&&r->eof&&r->source_diagnostic==5&&out[0]=='I'&&out[1]=='J',"actual file short EOF bytes");expect_error(m,{1,5});
  check(!m->skip_forward(*file,std::uint64_t(INT64_MAX),f)&&f.kind==RetailRendererStreamError::content,"native overflow-safe failed seek");
  expect_error(m,{1,5});check(m->close(*file,f),"owned file checked close");
  check(!m->open_resource_read("missing.bin",f)&&f.kind==RetailRendererStreamError::content&&f.content.kind==mka::content::ContentVolumeError::open_failed,"actual missing file fails by name");expect_error(m,{0,0x80000002u});
  auto partial=m->open_resource_read("data.bin",f);short_read=true;r=m->read(*partial,out,f);
  check(r&&r->bytes==2&&!r->eof&&r->source_diagnostic==0x8000001au&&m->observe(*partial,f)->position==2,"test-only non-EOF partial I/O preserves actual bytes/cursor");expect_error(m,{0,0x8000001au});check(m->close(*partial,f),"partial fixture close");
  mka::content::ContentVolumeFailure cf;auto opened=x.startup.content.open_resource("data.bin",cf);
  auto borrowed=std::make_shared<mka::content::NativeContentFile>(std::move(*opened));
  auto b=m->adopt_borrowed_file_read(borrowed,f);check(bool(b),"actual borrowed file validation");
  check(m->read(*b,std::span(out).first(2),f)->bytes==2,"shared opened cursor advances");
  const auto closes=close_calls;check(m->close(*b,f)&&borrowed->valid()&&close_calls==closes&&borrowed->position(cf)==2,"borrowed close does not fclose or reopen caller file");
  auto invalid=m->adopt_borrowed_file_read(borrowed,f);check(invalid&&borrowed->close_checked(cf),"explicit caller-owned close");
  r=m->read(*invalid,out,f);check(r&&r->bytes==0&&r->host_error&&f.kind==RetailRendererStreamError::content,"native stale backing failure remains visible");expect_error(m,{0,0x8000001au});check(m->close(*invalid,f),"borrowed record still retires safely");
  auto failed=m->open_resource_read("data.bin",f);const auto before_close=close_calls;fail_close=true;
  check(!m->close(*failed,f)&&f.content.kind==mka::content::ContentVolumeError::close_failed&&f.content.host_error.value()==EIO,"actual close failure preserved");
  check(close_calls==before_close+1&&!m->close(*failed,f)&&f.kind==RetailRendererStreamError::retired_stream&&close_calls==before_close+1,"failed close consumed record and cannot replay");
  expect_error(m,{0,0x80000000u});
#ifdef _WIN32
  auto wide=m->open_resource_read("sparse.bin",f);check(wide&&m->skip_forward(*wide,(std::uint64_t(1)<<32)+36,f),"native stream cursor beyond4GiB");
  check(m->observe(*wide,f)->position==(std::uint64_t(1)<<32)+36,"actual wide cursor");r=m->read(*wide,std::span(out).first(1),f);check(r&&r->bytes==1&&out[0]=='Z'&&m->close(*wide,f),"exact wide sparse tail");
#endif
  bool wrong=false;std::thread foreign([&]{RetailRendererStreamFailure q;wrong=!m->live_streams(q)&&q.kind==RetailRendererStreamError::wrong_thread;});foreign.join();check(wrong,"actual owner thread guard");
  Fixture other(root);auto other_m=other.module();auto foreign_h=other_m->open_memory_read(data,f);
  check(!m->close(*foreign_h,f)&&f.kind==RetailRendererStreamError::foreign_owner&&other_m->close(*foreign_h,f),"native stream identity cannot cross resources");
  renderer_required::destroy_core_plugin_0404(*other.core());renderer_required::destroy_core_plugin_0404(*c);
  check(c->module_0404_count(x.f)==0&&!m->live_streams(f)&&f.kind==RetailRendererStreamError::retired,"actual module retirement/count");
 }
 {
  const auto data=memory_bytes();bool done=false;
  for(int cut=0;cut<64;++cut){Fixture x(root);x.prepare();auto c=x.core();const auto baseline=x.live();
   fail_after=cut;const bool ok=renderer_required::construct_core_plugin_0404(*c,x.f);fail_after=-1;
   if(ok){done=true;renderer_required::destroy_core_plugin_0404(*c);break;}++module_cuts;
   check(c->module_0404_count(x.f)==0&&!c->streams(x.f)&&x.live()==baseline,"failed module admission publishes no resource/count/leak");
   check(renderer_required::construct_core_plugin_0404(*c,x.f),"actual constructor retry");renderer_required::destroy_core_plugin_0404(*c);
  }check(done&&module_cuts,"all reached module cuts");
  done=false;
  for(int cut=0;cut<64;++cut){Fixture x(root);auto m=x.module();RetailRendererStreamFailure f;auto keep=m->open_memory_read(data,f);std::array<std::uint8_t,2> bytes{};
   check(keep&&m->read(*keep,bytes,f)->bytes==2,"existing live cursor before allocation failure");const auto baseline=x.live();
   fail_after=cut;auto h=m->open_resource_read("data.bin",f);fail_after=-1;
   if(h){done=true;check(m->close(*h,f)&&m->close(*keep,f),"actual successful cut endpoint");renderer_required::destroy_core_plugin_0404(*x.core());break;}++record_cuts;
   check(m->live_streams(f)==1&&m->observe(*keep,f)->position==2&&x.live()==baseline,"refused allocation/open preserves existing record/cursor and physical storage");
   auto retry=m->open_resource_read("data.bin",f);check(retry&&m->close(*retry,f)&&m->close(*keep,f),"actual open retry/cleanup");renderer_required::destroy_core_plugin_0404(*x.core());
  }check(done&&record_cuts,"all reached stream/open allocation cuts");
 }
 {
  Fixture x(root);later=Later::refuse;check(main_binding::initialize_retail_first_scene(x.main,x.sf)==RetailStartupGate::native_failure&&suffix_calls==1,"actual Main/Core constructors execute0404 before explicit0405 refusal");
  RetailRendererStreamFailure f;check(captured&&!captured->live_streams(f)&&f.kind==RetailRendererStreamError::retired,"prefix refusal retires actual stream state");captured.reset();
 }
 {
  Fixture x(root);x.tasks();auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized,"actual transfer CoreInit");
  later=Later::transfer;transfer_main=&x.main;bool moved=false;
  try{(void)c->construct_registered(x.f);}catch(const detail::NativeMainReentry& e){moved=e.owner==&x.main&&e.value==71;}
  RetailRendererStreamFailure f;std::array<std::uint8_t,1> b{};
  check(moved&&captured&&captured->read(captured_stream,b,f)->bytes==1&&b[0]==1,"legitimate Main transfer retains actual stream resource/cursor");
  check(main_binding::retail_task_exists(x.main,0x7654,x.sf)==false,"next source frame still performs actual task lookup");
  check(c->destroy_constructed(x.f)&&!captured->live_streams(f),"actual retained prefix cleanup");captured.reset();transfer_main=nullptr;later=Later::refuse;
 }
 {
  Fixture x(root);auto c=x.core();auto initialized=c->initialize_core(x.f);later=Later::allow;
  check(initialized&&*initialized&&c->construct_registered(x.f),"explicit later-factory fixtures for checked cleanup");auto m=c->streams(x.f);RetailRendererStreamFailure f;
  auto h=m->open_resource_read("data.bin",f);check(bool(h),"actual owned file before retirement fault");const auto n=close_calls;fail_close=true;
  check(!c->destroy_constructed(x.f)&&x.f.kind==RetailRendererCoreError::after_effect_unknown&&x.f.cleanup_content.kind==mka::content::ContentVolumeError::close_failed&&x.f.cleanup_content.host_error.value()==EIO,"Core does not report successful checked cleanup failure");
  check(close_calls==n+1&&!c->destroy_constructed(x.f)&&close_calls==n+1,"ordinary cleanup failure cannot replay effects");
  check(!m->live_streams(f)&&f.kind==RetailRendererStreamError::retired,"failed cleanup still retired all records");later=Later::refuse;
 }
 {
  std::shared_ptr<NativeRetailRendererStreams> held;std::weak_ptr<NativeRetailGameSession> session;later=Later::allow;
  {Fixture x(root);auto c=x.core();auto initialized=c->initialize_core(x.f);check(initialized&&*initialized&&c->construct_registered(x.f),"actual lifetime prefix");
   held=c->streams(x.f);session=x.startup.game_session;RetailRendererStreamFailure f;check(bool(held->open_resource_read("data.bin",f)),"actual file lifetime");}
  RetailRendererStreamFailure f;check(session.expired()&&!held->live_streams(f)&&f.kind==RetailRendererStreamError::retired,"external module cannot retain Session/usable resources");later=Later::refuse;
 }
 std::printf("PASS_NATIVE0404_READ_STREAM_RESOURCE; actual Core/Main prefix,memory/borrowed/owned reads,partial/EOF/skip64/checked close,64 allocations,weak physical release,thread/identity/lifetime/Main transfer/Core cleanup no replay; allocation cuts module=%u stream=%u; writes/custom/source-wrap/chunks/GPU/game OPEN; no window/audio\n",module_cuts,record_cuts);
 return 0;
}catch(const std::exception& e){fail_after=-1;fail_close=false;short_read=false;std::fprintf(stderr,"FAIL native0404 streams: %s\n",e.what());return 1;}
