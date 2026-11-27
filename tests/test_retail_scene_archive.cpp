#include "mka/retail_scene_archive.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_renderer_matrix.hpp"
#include "native_retail_startup.hpp"
#include "retail_scene_archive_fixtures.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>

namespace { thread_local int fail_after=-1;std::size_t checks{}; }
void* operator new(std::size_t n){
 if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;
 if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
using namespace mka::retail;
namespace fs=mka::test::scene_archive;
namespace {
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
void save(const std::filesystem::path& p,const fs::Bytes& b){
 check(!std::filesystem::exists(p),"never overwrite preserved fixture");
 std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));check(bool(f),"serialized fixture write");
}
struct Fixture {
 NativeRetailStartupState startup;
 std::shared_ptr<NativeRetailRendererCore> core;
 std::shared_ptr<NativeRetailRendererRasters> rasters;
 std::shared_ptr<NativeRetailRendererTextures> textures;
 std::shared_ptr<NativeRetailRendererMaterials> materials;
 std::shared_ptr<NativeRetailRendererFrames> frames;
 std::shared_ptr<NativeRetailRendererWorldModule> worlds;
 std::shared_ptr<NativeRetailWorld> world;
 static mka::content::NativeContentVolume volume(const std::filesystem::path& p){
  mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(p,f);check(bool(v),"actual native content volume");return std::move(*v);
 }
 explicit Fixture(const std::filesystem::path& p):startup(volume(p),{}){
  StartupFailure sf;RetailRendererAllocatorFailure af;RetailRendererCoreFailure f;RetailRendererWorldFailure wf;
  check(initialize_native_retail_game_session(startup,sf)&&initialize_retail_memory_owners(startup,sf),"actual Session and host heaps");
  auto allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual renderer allocator");
  core=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,f);check(bool(core),"actual persistent Core");
  auto initialized=core->initialize_core(f);check(initialized&&*initialized,"actual Core registry initialization");
  check(renderer_required::construct_core_plugin_0403(*core,f)&&renderer_required::construct_core_plugin_0407(*core,f)&&
   renderer_required::construct_core_plugin_0408(*core,f)&&renderer_required::construct_core_plugin_040a(*core,f)&&
   renderer_required::construct_core_plugin_0412(*core,f),"actual frame raster texture material world constructors");
  rasters=core->rasters(f);textures=core->textures(f);materials=core->materials(f);frames=core->frames(f);worlds=core->world_module(f);
  check(rasters&&textures&&materials&&frames&&worlds,"actual typed module owners");
  auto h=worlds->create_world(wf);world=h?worlds->resolve_world(*h,wf):nullptr;check(bool(world),"actual scene world");
 }
 auto collector(){return std::make_unique<RetailSceneResources>(rasters,textures,materials,frames,world);}
 std::array<std::size_t,4> counts(){
  RetailRendererRasterFailure rf;RetailRendererFrameFailure ff;
  auto r=rasters->live_rasters(rf);auto f=frames->observe_module(ff);check(r&&f,"actual live module observations");
  return {*r,textures->active_count(),materials->active_count(),f->live_base_frames};
 }
 auto load(const RetailArchiveSceneStreams& c,RetailSceneResources& owner,RetailSceneArchiveFailure& f){
  return load_retail_scene_streams_from_archive(startup.content,c,1000,2000,*rasters,*textures,*materials,*frames,*world,{},f,owner);
 }
};
RetailArchiveSceneStreams config(std::string key="scene.pak"){
 RetailArchiveSceneStreams c;c.archive_resource_key=std::move(key);
 c.txd=RetailArchiveMemberSelector{std::string("scene.txd"),{}};
 c.world=RetailArchiveMemberSelector{{},1};
 c.geometry=RetailArchiveMemberSelector{std::string("fighter.dff"),{}};return c;
}
}

// Explicit, uncalled test boundaries: these kernels/suffix constructors must
// never be used to make the parser/owner component pass.
namespace mka::retail::renderer_required {
bool construct_core_plugin_0409(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0409(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_040b(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_040b(NativeRetailRendererCore&)noexcept{std::abort();}
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&){std::abort();}
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&){std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&){std::abort();}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&){std::abort();}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&){std::abort();}
}
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(NativeRetailMainState&,std::shared_ptr<NativeRetailRendererCore>,StartupFailure&){std::abort();}
bool continue_scene_reset_at_125d60(NativeRetailMainState&,StartupFailure&){std::abort();}
}

int main(int argc,char** argv)try{
 check(argc==2||argc==3,"fresh fixture directory and optional actual original volume");
 const std::filesystem::path root(argv[1]);check(std::filesystem::create_directory(root),"fresh fixture directory preserves prior evidence");
 const auto t=fs::txd(),w=fs::world(),g=fs::geometry();
 save(root/"scene.pak",fs::pak({{"scene.txd",t},{"arena.bsp",w},{"fighter.dff",g}}));
 save(root/"late-failure.pak",fs::pak({{"scene.txd",t},{"arena.bsp",w},{"fighter.dff",fs::geometry(true)}}));
 auto sec=fs::Bytes(28,std::byte{});sec[0]=std::byte{' '};sec[1]=std::byte{'C'};sec[2]=std::byte{'E'};sec[3]=std::byte{'S'};
 save(root/"sec.pak",fs::pak({{"scene.txd",t},{"arena.bsp",sec},{"fighter.dff",g}}));
 auto wrong=w;wrong[0]=std::byte{0xf};
 save(root/"wrong-root.pak",fs::pak({{"scene.txd",t},{"arena.bsp",wrong},{"fighter.dff",g}}));
 auto trailing=w;trailing.push_back(std::byte{});
 save(root/"trailing.pak",fs::pak({{"scene.txd",t},{"arena.bsp",trailing},{"fighter.dff",g}}));
 save(root/"scene.txd",t);save(root/"arena.bsp",w);
 Fixture x(root);const auto baseline=x.counts();RetailSceneArchiveFailure f;RetailRendererWorldFailure wf;
 {
  auto owner=x.collector();const auto c=config();auto result=x.load(c,*owner,f);
  check(result&&result->txd_ingested&&result->world_ingested&&result->geometry_ingested&&result->bytes_read==t.size()+w.size()+g.size(),"all explicitly selected PAK streams reach actual typed modules");
  check(owner->textures.size()==1&&owner->rasters.size()==1&&owner->materials.size()==2&&owner->frames.size()==1&&
   owner->scene_objects.size()==1&&owner->owns_bsp,"successful ingestion retains actual created handles in Scene owner");
  const auto live=x.counts();check(live==std::array<std::size_t,4>{baseline[0]+1,baseline[1]+1,baseline[2]+2,baseline[3]+1},"actual module records account for all created resources");
  check(x.world->scene_objects().size()==1&&x.world->bsp_sectors().size()==1,"actual selected geometry and BSP published");
  check(!x.load(c,*owner,f)&&f.kind==RetailSceneArchiveError::resource_owner,"populated collector cannot be stolen or silently replaced");
  check(owner->cleanup(wf)&&owner->empty()&&owner->cleanup(wf),"reverse dependency cleanup is explicit and idempotent");
  check(x.counts()==baseline&&x.world->scene_objects().empty()&&x.world->bsp_sectors().empty(),"scene teardown restores all actual module and world counts");
 }
 for(const auto& [key,expected]:std::array<std::pair<const char*,RetailSceneArchiveError>,4>{{
  {"late-failure.pak",RetailSceneArchiveError::ingestion},{"sec.pak",RetailSceneArchiveError::unsupported_sec},
  {"wrong-root.pak",RetailSceneArchiveError::invalid_rw_root},{"trailing.pak",RetailSceneArchiveError::invalid_rw_root}}}){
  auto owner=x.collector();check(!x.load(config(key),*owner,f)&&f.kind==expected,"failed selected stream refuses by exact stage");
  check(owner->empty()&&x.counts()==baseline&&x.world->scene_objects().empty()&&x.world->bsp_sectors().empty(),"prevalidation or late ingestion failure publishes no resources");
 }
 for(unsigned variant=0;variant<4;++variant){
  auto owner=x.collector();auto c=config();
  if(variant==0)c.geometry->name="missing.dff";
  if(variant==1)c.geometry->index=2;
  if(variant==2){c.world->index.reset();}
  if(variant==3)c.world->index=999;
  check(!x.load(c,*owner,f)&&owner->empty()&&x.counts()==baseline,"missing ambiguous absent or out-of-range selection cannot mutate modules");
 }
 {
  auto owner=x.collector();bool refused{};auto c=config();
  std::thread other([&]{RetailSceneArchiveFailure q;refused=!x.load(c,*owner,q)&&q.kind==RetailSceneArchiveError::resource_owner;});other.join();
  check(refused&&owner->empty()&&x.counts()==baseline,"foreign thread refused before archive extraction or renderer mutation");
 }
 {
  auto owner=x.collector();auto loaded=load_retail_scene_streams_from_volume(x.startup.content,"scene.txd","arena.bsp",3000,
   *x.rasters,*x.textures,*x.materials,*x.world,wf,owner.get());
  check(loaded&&owner->textures.size()==1&&owner->materials.size()==1&&owner->owns_bsp,"existing loose stream loader also retains created handles");
 }
 check(x.counts()==baseline&&x.world->bsp_sectors().empty(),"RAII destructor cleans successful loose stream ownership");
 std::size_t allocation_failures{};bool reached_success=false;const auto c=config();
 for(int n=0;n<512;++n){
  auto owner=x.collector();fail_after=n;auto result=x.load(c,*owner,f);fail_after=-1;
  if(result){check(owner->cleanup(wf),"allocation sweep successful scene cleanup");reached_success=true;}
  else {++allocation_failures;check(owner->empty(),"allocation failure cannot lose actual created handles");}
  check(x.counts()==baseline&&x.world->scene_objects().empty()&&x.world->bsp_sectors().empty(),"each allocation prefix failure restores module/world baselines");
  if(reached_success)break;
 }
 check(reached_success&&allocation_failures>10,"bounded allocation prefix sweep reaches whole successful composition");
 if(argc==3){
  auto volume=Fixture::volume(argv[2]);auto owner=x.collector();RetailArchiveSceneStreams actual;
  actual.archive_resource_key="mkda.pak";actual.txd=RetailArchiveMemberSelector{{},0};
  check(!load_retail_scene_streams_from_archive(volume,actual,1000,2000,*x.rasters,*x.textures,*x.materials,*x.frames,*x.world,{},f,*owner)&&
   f.kind==RetailSceneArchiveError::unsupported_sec&&owner->empty()&&x.counts()==baseline,"actual retail SSF member is refused as SEC instead of invented named RW data");
 }
 std::printf("PASS archive scene checks=%zu allocation_prefix_failures=%zu actual_SEC=%s; constructed composition only, no gameplay/native credit\n",checks,allocation_failures,argc==3?"refused":"not-run");
 return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL archive scene after%zu checks: %s\n",checks,e.what());return 1;}
