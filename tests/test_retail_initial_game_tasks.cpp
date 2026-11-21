#include "mka/retail_initial_game_tasks.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
void check(bool okay,const char* label) {
    if(!okay) {
        std::fprintf(stderr,"retail initial game tasks: %s\n",label);
        std::abort();
    }
}
}

int main() {
    using namespace mka::retail;
    RetailGamePoolFailure pf;
    std::shared_ptr<NativeRetailGamePool> pool(
        NativeRetailGamePool::initialize(3,pf));
    check(bool(pool),"real shared 12DA70 pool");
    RetailGameTaskListFailure lf;
    auto lists=NativeRetailGameTaskLists::create(pool,lf);
    check(bool(lists),"real persistent source callback heads");
    check(lists->clear_from_228b90(lf)&&
          lists->clear_first_pair_from_13c180(lf)&&
          lists->clear_later_pair_from_13c180(lf)&&
          lists->clear_from_139a30(lf)&&lists->clear_from_16c1b0(lf),
          "source constructor head-clear order");
    RetailInitialGameTaskFailure tf;
    auto tasks=NativeRetailInitialGameTasks::create(lists,pool,tf);
    check(bool(tasks),"same pool and callback-head owner");
    RetailGameControlsFailure cf;
    auto controls=NativeRetailGameControls::create(cf);
    check(bool(controls),"real game-info gate owner");
    RetailTaskDomainFailure df;
    auto domain=NativeRetailTaskDomain::create(controls,df);
    check(bool(domain)&&domain->seed_counter_from_185ee0(df)&&
          domain->reset_task_roots_from_12f0d0(pool,df),
          "one source task domain and root birth");
    const auto camera=tasks->publish_source_task(
        RetailInitialGameTaskBodyKind::camera_13c220,*domain,tf);
    const auto actors=tasks->publish_source_task(
        RetailInitialGameTaskBodyKind::actors_13a7a0,*domain,tf);
    const auto scene=tasks->publish_source_task(
        RetailInitialGameTaskBodyKind::scene_16c1f0,*domain,tf);
    const auto per_task=tasks->publish_source_task(
        RetailInitialGameTaskBodyKind::per_task_158080,*domain,tf);
    check(camera&&actors&&scene&&per_task,
          "four executable source task bodies published with real cookies");
    auto* registry=domain->registry(df);
    RetailTaskRegistryFailure rf;
    const auto order=registry->observe(rf);
    check(order&&order->ordered.size()==4&&
          order->ordered[0].kind==0x5001&&
          order->ordered[1].kind==0x500b&&
          order->ordered[2].kind==0x5006&&
          order->ordered[3].kind==0x5003,
          "12F260 signed priority order after source publication order");
    check(domain->dispatch_first_root_zero_flags_from_12e4e0(df),
          "12E670 invokes all four real bodies on source empty-head first frame");
    const auto observation=tasks->observe_camera(tf);
    check(observation&&observation->basis==std::array<float,9>{
              1.0f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f,1.0f}&&
          observation->position==std::array<float,3>{0.0f,0.0f,0.0f}&&
          observation->flags==0x00020003u,
          "127120 no-actor BSS branch writes identity camera and source flags");
    check(domain->cancel_first_root_from_12f030(df)&&
          pool->observe(pf)->active_records==0,
          "12F030 retires four bodies and both per-task nested roots");
    std::puts("native retail initial game tasks: PASS");
}
