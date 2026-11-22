#include "mka/retail_package.hpp"
#include "mka/retail_system_art_cpu.hpp"
#include <array>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <chrono>
using namespace mka::content;using namespace mka::retail;
namespace {thread_local int fail_after=-1;}
void* operator new(std::size_t n){if(fail_after>=0&&fail_after--==0)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
void check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
NativeFileReadStatus finish(NativeFileReadJob& job,unsigned& ticks){
 NativeFileReadFailure f;for(unsigned i=0;i<10000;++i){auto s=job.poll(f);check(bool(s),"owner poll");++ticks;if(*s!=NativeFileReadStatus::pending)return *s;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("native IO did not retire");
}
int main(int argc,char**argv)try{
 check(argc==3,"actual loose root and unique fixture directory");auto root=std::filesystem::path(argv[2]);check(!std::filesystem::exists(root),"unique disposable fixture");std::filesystem::create_directories(root);auto path=root/L"native-read-λ.bin";
 {std::ofstream out(path,std::ios::binary);for(unsigned i=0;i<1048576;++i)out.put(char((i*37u+(i>>8))&255));check(bool(out),"independent fixture bytes");}
 NativeFile file;std::error_code ec;check(file.open(path,ec)&&file.seek(123,SeekOrigin::begin,ec),"actual source handle/cursor");NativeFileReadFailure f;
 auto job=NativeFileReadJob::begin(file,65531,131079,f);check(bool(job),"actual overlapped request");
 bool wrong=false;std::thread foreign([&]{NativeFileReadFailure x;wrong=!job->poll(x)&&x.kind==NativeFileReadError::wrong_thread&&!job->cancel(x)&&x.kind==NativeFileReadError::wrong_thread&&!job->bytes(x)&&x.kind==NativeFileReadError::wrong_thread;});foreign.join();check(wrong,"wrong thread refuses before native IO state");
 file.close();unsigned ticks=0;check(finish(*job,ticks)==NativeFileReadStatus::complete,"read survives original handle close");auto bytes=job->bytes(f);check(bytes&&bytes->size()==131079,"owned terminal bytes");for(unsigned i=0;i<bytes->size();++i){unsigned at=i+65531;check((*bytes)[i]==std::uint8_t((at*37u+(at>>8))&255),"independent offset/byte identity");}
 check(job->cancel(f)&&job->poll(f)==NativeFileReadStatus::complete,"terminal completion never relabeled cancellation");
 check(!NativeFileReadJob::begin(file,0,1,f)&&f.kind==NativeFileReadError::closed,"closed source rejects");check(file.open(path,ec),"reopen disposable fixture");
 check(!NativeFileReadJob::begin(file,1048577,1,f)&&f.kind==NativeFileReadError::invalid_range,"out of range refuses");check(!NativeFileReadJob::begin(file,UINT64_MAX,1,f)&&f.kind==NativeFileReadError::invalid_range,"overflow refuses");
 auto zero=NativeFileReadJob::begin(file,1048576,0,f);check(zero&&zero->poll(f)==NativeFileReadStatus::complete&&zero->bytes(f)->empty(),"actual EOF zero-byte request");
 unsigned allocation_cuts=0;
 for(int cut=0;cut<16;++cut){fail_after=cut;auto attempt=NativeFileReadJob::begin(file,0,131072,f);fail_after=-1;if(attempt){check(finish(*attempt,ticks)==NativeFileReadStatus::complete,"allocation retry completes actual read");break;}check(f.kind==NativeFileReadError::allocation,"pre-IO allocation failure is named");++allocation_cuts;}
 check(allocation_cuts==4,"four actual ownership allocations precede IO admission");
 check(file.seek(123,SeekOrigin::begin,ec),"fixture cursor set");auto independent=NativeFileReadJob::begin(file,1,900000,f);check(independent&&finish(*independent,ticks)==NativeFileReadStatus::complete,"actual independent read");std::uint64_t pos=0;check(file.position(pos,ec)&&pos==123,"overlapped read never changes original FILE cursor");
 ContentVolumeFailure vf;auto volume=NativeContentVolume::open(argv[1],vf);check(bool(volume),"actual original loose volume");RetailFilePathFailure pf;auto paths=NativeRetailFilePaths::create(pf);RetailPackageFailure pkgf;auto package=NativeRetailPackage::open(*volume,*paths,pkgf);check(bool(package),"existing actual package");
 constexpr std::array<std::uint64_t,5> offsets{0x800,0x61800,0xc2800,0x123800,0x184800};
 for(auto offset:offsets){auto read=package->begin_read("gameart.ssf",offset,0x60880,pkgf,f);check(read&&finish(*read,ticks)==NativeFileReadStatus::complete,"all actual locale descriptor reads");auto actual=read->bytes(f);std::vector<std::uint8_t> reference(0x60880);check(package->read("gameart.ssf",offset,reference,pkgf)==reference.size(),"existing independent synchronous package reference");check(actual&&std::equal(actual->begin(),actual->end(),reference.begin(),reference.end()),"async exact actual section bytes");}
 auto member=package->find("gameart.ssf",pkgf);check(bool(member),"actual member");auto tail=package->begin_read("gameart.ssf",member->size-3,999,pkgf,f);check(tail&&finish(*tail,ticks)==NativeFileReadStatus::complete&&tail->bytes(f)->size()==3,"source package end clipping");
 check(!package->begin_read("missing.ssf",0,1,pkgf,f)&&pkgf.kind==RetailPackageError::missing_member,"no missing fallback");
 unsigned cancelled=0,completed=0;
 for(unsigned i=0;i<16;++i){auto read=package->begin_read("gameart.ssf",0,std::size_t(member->size),pkgf,f);check(bool(read),"actual cancel-race request");check(read->cancel(f),"actual cancellation admitted");auto status=finish(*read,ticks);if(status==NativeFileReadStatus::cancelled){++cancelled;check(!read->bytes(f)&&f.kind==NativeFileReadError::not_complete,"cancelled buffer not published");}else{check(status==NativeFileReadStatus::complete,"cancellation race has actual terminal result");++completed;}}
 auto survivor=package->begin_read("gameart.ssf",offsets[0],0x60880,pkgf,f);check(bool(survivor),"retained actual read");package.reset();paths.reset();volume.reset();check(finish(*survivor,ticks)==NativeFileReadStatus::complete&&survivor->bytes(f)->size()==0x60880,"job retains actual handle and volume identity after provider retirement");
 std::printf("PASS actual OVERLAPPED native reads, all5 original sysart ranges, cursor/owner/lifetime/clipping/error; polls=%u cancelled=%u completed_race=%u; pendingqueue/conversion/cache/Mainwait scheduling still unbound; GPU_context=false visible_window=false presentation=false audio_endpoint=false\n",ticks,cancelled,completed);return 0;
}catch(const std::exception&e){fail_after=-1;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
