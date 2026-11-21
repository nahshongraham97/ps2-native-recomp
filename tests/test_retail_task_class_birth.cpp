#include "mka/retail_game_pool.hpp"
#include "mka/retail_observer_list.hpp"
#include "mka/retail_task_registry.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace {
void check(bool value,const char* message) {
    if(!value) { std::cerr<<message<<'\n'; std::abort(); }
}
struct Body final : mka::retail::NativeRetailTaskBody {
    Body(int id,std::vector<int>& events):id(id),events(events) {}
    mka::retail::NativeTaskStep run() override {
        return mka::retail::NativeTaskStep::returned(1.0f);
    }
    void cleanup(bool release_allowed) override {
        check(release_allowed,"body release follows original virtual+C guard");
        ++calls;
        events.push_back(id);
    }
    int id,calls{};
    std::vector<int>& events;
};
struct Observed final : mka::retail::NativeRetailObservedObject {
    Observed(int id,std::vector<int>& events):id(id),events(events) {}
    std::uint32_t source_cookie() const noexcept override { return cookie; }
    void cleanup_from_12fa00() override {
        ++calls;
        events.push_back(id);
        cookie=0;
    }
    int id,calls{};
    std::vector<int>& events;
    std::uint32_t cookie{1};
};
struct Published {
    std::shared_ptr<mka::retail::NativeRetailTaskObject> task;
    std::shared_ptr<Body> body;
    std::shared_ptr<Observed> e8,ec;
};
Published publish(mka::retail::NativeRetailTaskRegistry& registry,
    const std::shared_ptr<mka::retail::NativeRetailGamePool>& pool,
    mka::retail::RetailTaskProgramClass program_class,
    std::uint32_t kind,std::int32_t priority,std::uint32_t cookie,
    int base,std::vector<int>& events) {
    using namespace mka::retail;
    Published result;
    result.body=std::make_shared<Body>(base+2,events);
    result.e8=std::make_shared<Observed>(base,events);
    result.ec=std::make_shared<Observed>(base+1,events);
    auto e8=std::make_shared<NativeRetailObserverList>(pool);
    auto ec=std::make_shared<NativeRetailObserverList>(pool);
    RetailObserverListFailure of;
    check(bool(e8->insert_owned_argument_from_12fee0(result.e8,of)) &&
          bool(ec->insert_owned_argument_from_12fee0(result.ec,of)),
          "both native observer roots retain owned source objects");
    RetailTaskRegistryFailure rf;
    if(program_class==RetailTaskProgramClass::plain_12ec50)
        result.task=registry.publish_task(kind,priority,cookie,result.body,e8,ec,rf);
    else
        result.task=registry.publish_retained_task_from_12ea90(
            kind,priority,cookie,result.body,e8,ec,rf);
    check(bool(result.task),"source-specific task birth publishes executable body");
    return result;
}
void tail(const std::vector<int>& events,int a,int b,int c,const char* message) {
    check(events.size()>=3 && events[events.size()-3]==a &&
          events[events.size()-2]==b && events.back()==c,message);
}
}

int main() {
    using namespace mka::retail;
    RetailGamePoolFailure pf;
    std::shared_ptr<NativeRetailGamePool> pool(NativeRetailGamePool::initialize(2,pf));
    check(bool(pool),"one real growable source link pool");
    RetailTaskRegistryFailure rf;
    auto registry=NativeRetailTaskRegistry::create(pool,rf);
    check(bool(registry),"one source task registry");
    std::vector<int> events;

    auto plain=publish(*registry,pool,RetailTaskProgramClass::plain_12ec50,
                       0x6001,1,11,10,events);
    auto retained=publish(*registry,pool,RetailTaskProgramClass::retained_frame_12ea90,
                          0x6001,2,12,20,events);
    auto observed=registry->observe(rf);
    check(observed && observed->ordered.size()==2 &&
          observed->ordered[0].program_class==RetailTaskProgramClass::plain_12ec50 &&
          observed->ordered[1].program_class==RetailTaskProgramClass::retained_frame_12ea90 &&
          observed->ordered[0].kind==observed->ordered[1].kind,
          "constructor path, never kind, fixes each task class");
    check(registry->retire_matching_from_12edc0(0x6001,rf) &&
          plain.body->calls==1 && retained.body->calls==1 &&
          events==std::vector<int>{10,11,12,22,20,21},
          "direct visitor uses stock E8 EC body and retained body E8 EC order");
    auto absent=registry->find_first_live(0x9999,rf);
    check(absent && !*absent && plain.body->calls==1 && retained.body->calls==1,
          "later source lookup recycles both stale links without cleanup replay");

    auto plain_stale=publish(*registry,pool,RetailTaskProgramClass::plain_12ec50,
                             0x7001,3,31,30,events);
    auto retained_stale=publish(*registry,pool,RetailTaskProgramClass::retained_frame_12ea90,
                                0x7001,4,32,40,events);
    check(registry->store_object_cookie_from_source(plain_stale.task,1,rf) &&
          registry->store_object_cookie_from_source(retained_stale.task,1,rf),
          "source stale cached-cookie rewrite can match real cookie one");
    absent=registry->find_first_live(0x9999,rf);
    check(absent && !*absent && plain_stale.body->calls==1 &&
          retained_stale.body->calls==1,
          "generic stale retirement invokes both constructor-specific releases");
    check(events==std::vector<int>{10,11,12,22,20,21,30,31,32,42,40,41},
          "generic stale route preserves both exact observer/frame orders");

    auto plain_cancel=publish(*registry,pool,RetailTaskProgramClass::plain_12ec50,
                              0x8001,5,51,50,events);
    auto retained_cancel=publish(*registry,pool,RetailTaskProgramClass::retained_frame_12ea90,
                                 0x8001,6,52,60,events);
    check(registry->cancel_first_root_from_12f030(rf) &&
          plain_cancel.body->calls==1 && retained_cancel.body->calls==1,
          "source first-root cancellation releases both classes");
    check(events==std::vector<int>{10,11,12,22,20,21,30,31,32,42,40,41,
                                   50,51,52,62,60,61},
          "first-root cancellation keeps stock and retained release ordering");

    auto simple=registry->publish_retained_task_from_12ea90(
        0x9001,7,53,std::make_shared<Body>(72,events),rf);
    observed=registry->observe(rf);
    check(simple && observed && observed->ordered.size()==1 &&
          observed->ordered[0].program_class==RetailTaskProgramClass::retained_frame_12ea90,
          "retained birth overload constructs same explicit source class");
    check(!registry->publish_retained_task_from_12ea90(0x9002,8,54,{},rf) &&
          rf.kind==RetailTaskRegistryError::invalid_body,
          "retained birth cannot publish missing executable native frame body");
    std::cout<<"native retail task class birth fixture passed\n";
}
