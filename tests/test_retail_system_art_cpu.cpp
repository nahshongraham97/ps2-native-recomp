#include "mka/retail_system_art_cpu.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int fail_after=-1;}
void* operator new(std::size_t n){if(fail_after>=0&&fail_after--==0)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
struct Source {
 std::optional<mka::content::NativeContentVolume> volume;std::unique_ptr<NativeRetailFilePaths> paths;std::unique_ptr<NativeRetailPackage> package;
 explicit Source(const std::filesystem::path&p){mka::content::ContentVolumeFailure vf;volume=mka::content::NativeContentVolume::open(p,vf);check(bool(volume),"actual native volume");RetailFilePathFailure pf;paths=NativeRetailFilePaths::create(pf);check(bool(paths),"actual native alias owner");RetailPackageFailure f;package=NativeRetailPackage::open(*volume,*paths,f);check(bool(package),"actual native loose package");}
};
void change(const std::filesystem::path&p,std::size_t at,std::uint32_t w){std::fstream f(p,std::ios::binary|std::ios::in|std::ios::out);check(bool(f),"disposable original copy writer");f.seekp(std::streamoff(at));for(unsigned i=0;i<4;++i)f.put(char(w>>(8*i)));check(bool(f),"disposable byte mutation");}
int main(int argc,char**argv)try{
 check(argc==3,"actual loose root and unique disposable error root");Source original(argv[1]);RetailSystemArtCpuFailure f;
 std::array<std::shared_ptr<const NativeRetailSystemArtCpuSection>,5> retained;
 for(int language=0;language<5;++language){auto p=prepare_system_art_cpu_section(*original.package,language,f);check(bool(p)&&f.kind==RetailSystemArtCpuError::none,"actual selected CPU section");check(p->language_selector()==language&&p->directory().records().size()==34,"actual descriptor/section identity");check(p->textures().size()==21&&p->metrics().size()==9&&p->group_ranges().size()==3,"all actual texture/MET/group CPU data, no false script decode");
  const auto& records=p->directory().records();for(const auto&t:p->textures())check((records[t.ordinal].raw_type&0x3fffffffu)==2||(records[t.ordinal].raw_type&0x3fffffffu)==3,"both texture source kinds decoded");
  auto it=std::find_if(p->metrics().begin(),p->metrics().end(),[&](const auto&m){return records[m.ordinal].name=="EDDMONDGRAY14_MET";});check(it!=p->metrics().end()&&it->data.glyph_count()==224,"real font0 metrics");mka::text::FontMetricsFailure mf;check(it->data.advance(32,mf)==6,"original font0 special space width");retained[language]=std::move(p);
 }
 check(!prepare_system_art_cpu_section(*original.package,-1,f)&&f.kind==RetailSystemArtCpuError::language,"negative locale is not defaulted");check(!prepare_system_art_cpu_section(*original.package,5,f)&&f.kind==RetailSystemArtCpuError::language,"unproved locale is not defaulted");
 bool wrong=false;std::thread t([&]{RetailSystemArtCpuFailure other;wrong=!prepare_system_art_cpu_section(*original.package,0,other)&&other.kind==RetailSystemArtCpuError::package&&other.package.kind==RetailPackageError::wrong_thread;});t.join();check(wrong,"real package cursor owner-thread guard");
 unsigned cuts=0;bool succeeded=false;for(int cut=0;cut<512;++cut){fail_after=cut;auto p=prepare_system_art_cpu_section(*original.package,0,f);fail_after=-1;if(p){succeeded=true;break;}check(f.kind==RetailSystemArtCpuError::allocation||f.package.kind==RetailPackageError::allocation||f.section.kind==mka::content::ArtSectionError::allocation_failed||f.texture.code==mka::FontAtlasError::allocation_failure||f.metrics.kind==mka::text::FontMetricsError::allocation_failed,"allocation cuts name their actual owner failure");++cuts;}check(succeeded&&cuts>50,"every fallible CPU preparation cut reaches unchanged source retry");
 original.package.reset();original.paths.reset();original.volume.reset();for(const auto&p:retained)check(p->textures().size()==21&&p->metrics()[4].data.glyph_count()==224,"partial CPU owner survives file/package/volume retirement");
 auto root=std::filesystem::path(argv[2]);check(!std::filesystem::exists(root),"unique disposable root must not exist");std::filesystem::create_directories(root/"art");{Source empty(root);check(!prepare_system_art_cpu_section(*empty.package,0,f)&&f.kind==RetailSystemArtCpuError::package&&f.package.kind==RetailPackageError::missing_member,"missing loose source never archive/font fallback");}
 const auto asset=root/"art/gameart.ssf";std::filesystem::copy_file(std::filesystem::path(argv[1])/"art/gameart.ssf",asset);change(asset,0x800+0x3e880+25,0);{Source bad(root);check(!prepare_system_art_cpu_section(*bad.package,0,f)&&f.kind==RetailSystemArtCpuError::texture&&f.ordinal==17,"real malformed font0 atlas fails by decoder name");}
 std::filesystem::copy_file(std::filesystem::path(argv[1])/"art/gameart.ssf",asset,std::filesystem::copy_options::overwrite_existing);change(asset,0x800+0x8350+32,0x7f800000u);{Source bad(root);check(!prepare_system_art_cpu_section(*bad.package,0,f)&&f.kind==RetailSystemArtCpuError::metrics&&f.ordinal==4,"real malformed font0 MET fails without synthesized metrics");}
 std::printf("PASS original5 selected sysart CPU sections/all105 textures/45MET/15groups, lifetime/thread/locale/missing/decode and%u allocation cuts; whole3011D0/GPU/script/cache remain required\n",cuts);return 0;
}catch(const std::exception&e){fail_after=-1;std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
