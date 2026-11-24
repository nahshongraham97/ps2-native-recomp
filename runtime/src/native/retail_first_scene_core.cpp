#include "mka/retail_renderer_core.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "native_retail_startup.hpp"
namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_1a6dd0(NativeRetailMainState& main,
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator,StartupFailure& f){
 f={};
 if(!main.lifetime||!main.lifetime->active_on_owner_thread()||!main.startup.game_session){f={StartupError::game_session,"1A6DD0 actual Main lifetime/thread/Session"};return RetailStartupGate::native_failure;}
 const auto session=main.startup.game_session;
 RetailGameSessionFailure gf;RetailCameraWorldFailure cf;
 auto* cameras=session->render_cameras(gf);const auto observed=cameras?cameras->observe(cf):std::optional<RetailRenderCameraObservation>{};
 if(!observed||!observed->count_producer_admitted){f={StartupError::game_session,"1A6DD0 actual first-scene producer"};return RetailStartupGate::native_failure;}
 RetailRendererCoreFailure detail;
 auto core=NativeRetailRendererCore::for_session(session,std::move(allocator),main.startup.content,detail);
 const auto initialized=core?core->initialize_core(detail):std::optional<bool>{};
 if(!initialized){
  if(detail.kind==RetailRendererCoreError::content)f.kind=StartupError::content;
  else if(detail.kind==RetailRendererCoreError::owner||detail.kind==RetailRendererCoreError::wrong_thread)f.kind=StartupError::game_session;
  else if(detail.kind==RetailRendererCoreError::allocation||detail.allocator.heap.kind==memory::NativeHeapError::allocation_failed)f.kind=StartupError::allocation;
  else if(detail.kind==RetailRendererCoreError::native_exception)f.kind=StartupError::native_exception;
  else f.kind=StartupError::memory_owners;
  f.operation=detail.operation;f.content=detail.content;f.memory.heap=detail.allocator.heap;return RetailStartupGate::native_failure;
 }
 if(!*initialized)return RetailStartupGate::source_refused;
 if(!main.lifetime->active_on_owner_thread()||main.startup.game_session!=session){f={StartupError::game_session,"1A6DD0 Main identity changed after initialization"};return RetailStartupGate::native_failure;}
 //1A6DD0 success is CoreInit only. Every later127260 effect including
 // realEngineOpen/Start,their errorunwinds and finalgame effects is required.
 return continue_first_scene_at_225d40(main,std::move(core),f);
}
}
