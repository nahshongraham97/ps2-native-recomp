#include "mka/retail_task_registry.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
bool count_allocations=false;
long allocation_attempts=0, fail_allocation_at=-1;
void check(bool good,const char* message) {
    if(!good) throw std::runtime_error(message);
}
struct TestBody final: NativeRetailTaskBody {
    explicit TestBody(std::vector<int>& events,int label):events(events),label(label) {}
    NativeTaskStep run() override {
        events.push_back(100+label);
        return NativeTaskStep::returned(1.0f);
    }
    void cleanup(bool release_allowed) override {
        ++cleanup_calls;
        if(!release_allowed) return;
        events.push_back(label);
        if(throw_main) throw detail::NativeMainReentry{nullptr,7};
        if(throw_ordinary) throw std::runtime_error("ordinary cleanup failure");
        if(registry) {
            RetailTaskRegistryFailure f;
            const auto attempted=registry->find_first_live(77,f);
            observed_callout_refusal=!attempted&&
                f.kind==RetailTaskRegistryError::callout_active;
        }
    }
    std::vector<int>& events;
    int label;
    bool throw_main{},throw_ordinary{},observed_callout_refusal{};
    int cleanup_calls{};
    NativeRetailTaskRegistry* registry{};
};
std::shared_ptr<NativeRetailTaskObject> publish(NativeRetailTaskRegistry& registry,
    std::uint32_t kind,std::int32_t priority,std::uint32_t cookie,
    const std::shared_ptr<TestBody>& body) {
    RetailTaskRegistryFailure f;
    auto task=registry.publish_task(kind,priority,cookie,body,f);
    check(task&&f.kind==RetailTaskRegistryError::none,"real native task body published");
    return task;
}
}
void* operator new(std::size_t size) {
    if(count_allocations&&allocation_attempts++==fail_allocation_at)
        throw std::bad_alloc();
    if(void* p=std::malloc(size?size:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }

int main() {
    try {
        RetailTaskRegistryFailure f;
        auto registry=NativeRetailTaskRegistry::create(f);
        check(bool(registry),"native dynamic task registry born");
        check(!registry->publish_task(1,4,1,{},f)&&
              f.kind==RetailTaskRegistryError::invalid_body,
              "kind metadata alone cannot become a live task");
        std::vector<int> events;
        auto b0=std::make_shared<TestBody>(events,0);
        auto b1=std::make_shared<TestBody>(events,1);
        auto b2=std::make_shared<TestBody>(events,2);
        auto b3=std::make_shared<TestBody>(events,3);
        auto t0=publish(*registry,17,4,11,b0);
        auto t1=publish(*registry,17,-2,12,b1);
        auto t2=publish(*registry,17,4,13,b2);
        auto t3=publish(*registry,19,32,14,b3);
        auto order=registry->observe(f);
        check(order&&order->ordered.size()==4&&
              order->ordered[0].object==t1&&order->ordered[1].object==t0&&
              order->ordered[2].object==t2&&order->ordered[3].object==t3,
              "signed priority ascends, and equal-priority publication follows equals");
        auto first=registry->find_first_live(17,f);
        check(first&&*first==t1,"lookup returns first actual matching object in link order");
        const auto direct_step=b0->run();
        check(direct_step.kind==NativeTaskStepKind::returned_float,
              "test body supplies a typed native result");
        check(events.size()==1&&events[0]==100,"published body is executable native code");
        check(registry->store_object_cookie_from_source(t1,8,f),
              "source cookie writer may replace identity with arbitrary value");
        first=registry->find_first_live(17,f);
        order=registry->observe(f);
        check(first&&*first==t0&&order&&order->ordered.size()==3&&
              order->retained_links==1&&events.size()==1,
              "stale lookup saves next, writes cached one, and skips cleanup for cookie eight");
        check(events.size()==1&&registry->observe(f)->retained_links==1,
              "unlinked stale task remains owned with no invented cleanup caller");

        // Source 12ED10 writes cached identity one before 12FA00. If the
        // object's real cookie is one, virtual cleanup still runs.
        auto one=publish(*registry,23,5,9,std::make_shared<TestBody>(events,4));
        check(registry->store_object_cookie_from_source(one,1,f),
              "source cookie one is admitted without monotonic generation rule");
        auto absent=registry->find_first_live(23,f);
        check(absent&&!*absent&&events.back()==4&&
              registry->observe(f)->recycled_links>=1,
              "stale cached one equals object cookie one and reaches cleanup");

        auto guarded=publish(*registry,31,6,15,std::make_shared<TestBody>(events,5));
        check(registry->store_link_flags_from_source(guarded,2,f)&&
              registry->retire_link(guarded,f)&&
              registry->observe(f)->ordered.size()==4,
              "12FA00 flag bit1 skips unlink and recycle");
        check(registry->store_link_flags_from_source(guarded,1,f)&&
              registry->retire_link(guarded,f)&&
              registry->observe(f)->retained_links==2&&events.back()!=5,
              "flag bit0 suppresses immediate virtual cleanup");

        auto current_body=std::make_shared<TestBody>(events,6);
        auto current=publish(*registry,37,7,16,current_body);
        check(registry->set_current_task(current,f)&&
              registry->store_object_retention_flag2_from_source(current,true,f)&&
              registry->retire_link(current,f)&&
              current_body->cleanup_calls==1&&events.back()!=6&&
              registry->observe(f)->retained_links==3,
              "12FA00 calls virtual cleanup; 12EE70 current/flag2 guard retains body");
        check(registry->clear_current_task(f)&&
              registry->store_object_retention_flag2_from_source(current,false,f)&&
              registry->observe(f)->retained_links==3&&events.back()!=6,
              "clearing guards does not invent a second cleanup source point");

        auto reentrant=std::make_shared<TestBody>(events,7);
        reentrant->registry=registry.get();
        auto t7=publish(*registry,41,8,17,reentrant);
        check(registry->retire_link(t7,f)&&reentrant->observed_callout_refusal&&
              events.back()==7,
              "unknown cleanup-time registry mutation is refused by name");

        bool wrong_thread=false;
        std::thread worker([&] {
            RetailTaskRegistryFailure off;
            wrong_thread=!registry->find_first_live(17,off)&&
                off.kind==RetailTaskRegistryError::wrong_thread&&
                !registry->observe(off)&&
                off.kind==RetailTaskRegistryError::wrong_thread;
        });
        worker.join();
        check(wrong_thread&&registry->find_first_live(17,f),
              "foreign thread refusal does not change ordered source links");

        auto insertion=NativeRetailTaskRegistry::create(f);
        auto stale_predecessor=publish(*insertion,60,-5,20,
                                       std::make_shared<TestBody>(events,10));
        auto later=publish(*insertion,61,7,21,
                           std::make_shared<TestBody>(events,11));
        check(insertion->store_object_cookie_from_source(stale_predecessor,22,f),
              "source producer changes predecessor cookie before insertion scan");
        auto between=publish(*insertion,62,4,23,
                             std::make_shared<TestBody>(events,12));
        auto inserted=insertion->observe(f);
        check(inserted&&inserted->ordered.size()==2&&
              inserted->ordered[0].object==between&&
              inserted->ordered[1].object==later&&inserted->retained_links==1,
              "12F260 saves next, retires stale predecessor, then inserts by signed priority");
        check(insertion->observe(f)->retained_links==1,
              "unlinked predecessor remains owned without fabricated cleanup");

        auto throwing=std::make_shared<TestBody>(events,8);
        throwing->throw_main=true;
        auto t8=publish(*registry,43,9,18,throwing);
        bool transferred=false;
        try { registry->retire_link(t8,f); }
        catch(const detail::NativeMainReentry& transfer) {
            transferred=transfer.owner==nullptr&&transfer.value==7;
        }
        order=registry->observe(f);
        check(transferred&&order&&order->pending_links==1&&
              !order->after_effect_unknown&&throwing->cleanup_calls==1&&
              registry->find_first_live(17,f)&&
              f.kind==RetailTaskRegistryError::none,
              "main transfer propagates, retains detached object, and resumes lookup");
        auto resumed=publish(*registry,44,10,19,
                             std::make_shared<TestBody>(events,13));
        check(resumed&&throwing->cleanup_calls==1&&
              registry->observe(f)->pending_links==1,
              "next source publication succeeds without replaying transferred cleanup");

        auto ordinary_registry=NativeRetailTaskRegistry::create(f);
        auto ordinary_body=std::make_shared<TestBody>(events,14);
        ordinary_body->throw_ordinary=true;
        auto ordinary_task=publish(*ordinary_registry,45,11,20,ordinary_body);
        bool ordinary_threw=false;
        try { ordinary_registry->retire_link(ordinary_task,f); }
        catch(const std::runtime_error&) { ordinary_threw=true; }
        auto ordinary_state=ordinary_registry->observe(f);
        check(ordinary_threw&&ordinary_state&&ordinary_state->pending_links==1&&
              ordinary_state->after_effect_unknown&&
              !ordinary_registry->find_first_live(45,f)&&
              f.kind==RetailTaskRegistryError::after_effect_unknown,
              "ordinary callback failure retains pending object and names unknown effect");

        auto fault_body=std::make_shared<TestBody>(events,9);
        auto measured=NativeRetailTaskRegistry::create(f);
        allocation_attempts=0;fail_allocation_at=-1;count_allocations=true;
        auto measured_task=measured->publish_task(50,4,1,fault_body,f);
        count_allocations=false;
        const auto cuts=allocation_attempts;
        check(measured_task&&cuts>0,"publication has actual native object/link allocations");
        for(long cut=0;cut<cuts;++cut) {
            auto fault_registry=NativeRetailTaskRegistry::create(f);
            allocation_attempts=0;fail_allocation_at=cut;count_allocations=true;
            auto failed=fault_registry->publish_task(50,4,1,fault_body,f);
            count_allocations=false;fail_allocation_at=-1;
            check(!failed&&f.kind==RetailTaskRegistryError::allocation&&
                  fault_registry->observe(f)->ordered.empty(),
                  "each pre-publication allocation cut leaves source root empty");
        }
        std::printf("PASS native retail ordered task registry: %ld publication allocation cuts; actual task producers/dispatch/main gate open\n",cuts);
        return 0;
    } catch(const std::exception& e) {
        count_allocations=false;fail_allocation_at=-1;
        std::fprintf(stderr,"FAIL native retail task registry: %s\n",e.what());
        return 1;
    }
}
