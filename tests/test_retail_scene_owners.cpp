#include "mka/retail_scene_events.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
// Opaque identity/lifetime fixtures only. These do not implement retail node,
// resource or action bodies; this executable never claims populated retirement.
namespace mka::retail {
class NativeRetailSceneNode final {
public:
    explicit NativeRetailSceneNode(int& count):count_(count){}
    ~NativeRetailSceneNode(){++count_;}
private:int& count_;
};
class NativeRetailSceneAction final {
public:
    explicit NativeRetailSceneAction(int& count):count_(count){}
    ~NativeRetailSceneAction(){++count_;}
private:int& count_;
};
}
namespace {
void check(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL scene: %s\n",text);throw std::runtime_error(text);}}
}
int main(){std::set_terminate([]{std::fputs("FAIL scene: owner left active during fixture unwind\n",stderr);std::fflush(stderr);std::_Exit(3);});try{
    using namespace mka::retail;
    NativeRetailSceneEventGraph graph;
    auto state=graph.observe();
    check(state.mask.enabled&&state.mask.current==0xffff&&state.mask.saved==0xffff&&
        state.elapsed_word==10000&&!state.has_root&&!state.has_current&&
        !state.active_selections&&!state.primary_actions&&!state.secondary_actions,"original constructor defaults");
    check(graph.permits_local_delivery(15,false)&&graph.permits_local_delivery(47,false)&&
        !graph.permits_local_delivery(16,false)&&graph.permits_local_delivery(16,true),
        "original low-five-bit selector and force bypass");
    graph.set_event_mask(0);graph.set_event_mask(0);
    state=graph.observe();
    check(!state.mask.enabled&&!state.mask.current&&state.mask.saved==0xffff,"repeated disable preserves source saved mask");
    graph.set_event_mask(1u<<10);
    check(graph.permits_local_delivery(10,false)&&!graph.permits_local_delivery(9,false),"selected source event mask");
    graph.accumulate_elapsed(0xffffffffu);
    check(graph.observe().elapsed_word==9999,"source elapsed word wraps independently of host allocation size");
    check(graph.reset_empty()&&graph.observe().elapsed_word==10000&&graph.observe().mask.current==(1u<<10),
        "empty reset preserves mask independently of scene teardown");

    int nodes_destroyed=0,actions_destroyed=0;
    auto root=std::make_shared<NativeRetailSceneNode>(nodes_destroyed);
    auto selected=std::make_shared<NativeRetailSceneNode>(nodes_destroyed);
    std::weak_ptr<NativeRetailSceneNode> root_weak=root,selected_weak=selected;
    check(graph.install_root(root)&&graph.select_current(selected)&&graph.record_active_selection(selected),"owned native references");
    root.reset();selected.reset();
    check(!root_weak.expired()&&!selected_weak.expired()&&!graph.reset_empty(),"live owners cannot be silently reset");
    auto root_owner=root_weak.lock(),selection_owner=selected_weak.lock();
    check(!graph.release_root_after_retirement(*root_owner),"live selection prevents root release");
    check(graph.select_current({})&&graph.forget_active_selection(*selection_owner),"explicit structural retirement commit");
    selection_owner.reset();check(selected_weak.expired()&&nodes_destroyed==1,"retired selection lifetime");
    check(graph.release_root_after_retirement(*root_owner),"root release after retirement preconditions");
    root_owner.reset();check(root_weak.expired()&&nodes_destroyed==2,"root retained until explicit release");

    auto first=std::make_shared<NativeRetailSceneAction>(actions_destroyed);
    auto second=std::make_shared<NativeRetailSceneAction>(actions_destroyed);
    check(graph.retain_action(RetailSceneQueue::primary,first,false,true)&&
        graph.retain_action(RetailSceneQueue::secondary,second,true,true),"separate source queues");
    check(graph.permits_local_delivery(10,false),"inactive primary and active secondary do not block local delivery");
    check(graph.update_action(RetailSceneQueue::primary,*first,true,true)&&
        !graph.permits_local_delivery(10,false)&&graph.permits_local_delivery(10,true),"active primary blocks ordinary delivery only");
    check(!graph.retain_action(RetailSceneQueue::secondary,first,true,true),"one intrusive action membership cannot alias both queues");
    check(!graph.retain_action(static_cast<RetailSceneQueue>(99),first,true,true),"unknown queue rejected");
    check(!graph.reset_empty(),"queued owners prevent empty-reset stand-in");
    std::weak_ptr<NativeRetailSceneAction> action_weak=first;
    first.reset();auto retained=action_weak.lock();
    check(retained&&graph.forget_action(RetailSceneQueue::primary,*retained),"explicit queue retirement commit");
    retained.reset();check(action_weak.expired()&&actions_destroyed==1,"queue release retains exact owner until commit");
    check(graph.forget_action(RetailSceneQueue::secondary,*second),"secondary retirement commit");
    second.reset();check(actions_destroyed==2&&graph.reset_empty(),"fully retired graph permits normal empty shutdown");
    std::puts("PASS native scene constructor/gates/structural ownership; no populated retail effects or game/device");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native scene ownership: %s\n",e.what());return 1;}}
