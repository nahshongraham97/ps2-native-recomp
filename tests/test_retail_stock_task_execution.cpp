#include "mka/retail_game_pool.hpp"
#include "mka/retail_observer_list.hpp"
#include "mka/retail_task_execution.hpp"
#include "mka/retail_task_registry.hpp"

#include <cstdlib>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {
void check(bool value,const char* message) {
    if(!value) { std::cerr<<message<<'\n'; std::abort(); }
}
struct Context {
    Context() {
        mka::retail::RetailGamePoolFailure pf;
        pool=std::shared_ptr<mka::retail::NativeRetailGamePool>(
            mka::retail::NativeRetailGamePool::initialize(2,pf));
        check(bool(pool),"one real growable native source link pool");
        mka::retail::RetailTaskRegistryFailure rf;
        registry=mka::retail::NativeRetailTaskRegistry::create(pool,rf);
        check(bool(registry),"one native ordered task registry");
    }
    std::shared_ptr<mka::retail::NativeRetailGamePool> pool;
    std::unique_ptr<mka::retail::NativeRetailTaskRegistry> registry;
};
struct Body final : mka::retail::NativeRetailTaskBody {
    mka::retail::NativeTaskStep run() override {
        ++run_calls;
        check(bool(next),"test body supplies explicit native step");
        return next();
    }
    void cleanup(bool release_allowed) override {
        check(release_allowed,"source virtual+C guard precedes body release");
        ++cleanup_calls;
        if(events) events->push_back(body_event);
    }
    std::function<mka::retail::NativeTaskStep()> next;
    std::vector<int>* events{};
    int body_event{},run_calls{},cleanup_calls{};
};
struct Continuation final : mka::retail::NativeRetailTaskContinuation {
    mka::retail::NativeTaskStep run() override {
        ++calls;
        return mka::retail::NativeTaskStep::returned(2.5f);
    }
    int calls{};
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
std::shared_ptr<mka::retail::NativeRetailTaskObject> publish(
    Context& context,std::uint32_t cookie,const std::shared_ptr<Body>& body,
    std::vector<int>* events=nullptr,
    std::shared_ptr<Observed>* out_e8=nullptr,
    std::shared_ptr<Observed>* out_ec=nullptr) {
    using namespace mka::retail;
    RetailTaskRegistryFailure rf;
    if(!events) {
        auto task=context.registry->publish_task(0x6001,1,cookie,body,rf);
        check(bool(task),"actual stock12EC50 native birth");
        return task;
    }
    auto e8=std::make_shared<NativeRetailObserverList>(context.pool);
    auto ec=std::make_shared<NativeRetailObserverList>(context.pool);
    auto first=std::make_shared<Observed>(1,*events);
    auto second=std::make_shared<Observed>(2,*events);
    RetailObserverListFailure of;
    check(bool(e8->insert_owned_argument_from_12fee0(first,of)) &&
          bool(ec->insert_owned_argument_from_12fee0(second,of)),
          "source E8 and EC retain their real owned objects");
    auto task=context.registry->publish_task(0x6001,1,cookie,body,e8,ec,rf);
    check(bool(task),"stock birth retains both distinct observer heads");
    *out_e8=first;
    *out_ec=second;
    return task;
}
void assert_plain(Context& context) {
    mka::retail::RetailTaskRegistryFailure rf;
    auto observed=context.registry->observe(rf);
    check(observed && observed->ordered.size()==1 &&
          observed->ordered[0].program_class==
              mka::retail::RetailTaskProgramClass::plain_12ec50,
          "source constructor, not task kind, fixes stock execution class");
}
}

int main() {
    using namespace mka::retail;
    {
        Context context;
        auto body=std::make_shared<Body>();
        auto resumed=std::make_shared<Continuation>();
        int phase=0;
        body->next=[&]() {
            switch(++phase) {
            case 1: return NativeTaskStep::returned(0.0f);
            case 2: return NativeTaskStep::returned(-0.0f);
            case 3: return NativeTaskStep::yield(resumed,0.0f);
            default: std::abort();
            }
        };
        auto task=publish(context,11,body);
        assert_plain(context);
        NativeRetailTaskExecution execution(*context.registry);
        RetailTaskExecutionFailure ef;
        auto result=execution.invoke_virtual_10_initial(task,ef);
        check(result && result->kind==NativeTaskRunKind::returned_nonnegative &&
              result->stored_wait==2.5f && result->immediate_calls==4 &&
              body->run_calls==3 && resumed->calls==1 && body->cleanup_calls==0,
              "stock +0/-0 loop and +20 yield resume immediately before positive wait");
        auto state=execution.observe(task,ef);
        check(state && state->stored_wait==2.5f && !state->resume_pending,
              "stock positive return stores wait without retained-frame suspension");
    }
    {
        Context context;
        auto body=std::make_shared<Body>();
        std::shared_ptr<NativeRetailTaskObject> task;
        body->next=[&]() {
            RetailTaskRegistryFailure rf;
            check(context.registry->store_object_cookie_from_source(task,0,rf),
                  "source body can invalidate selected cookie before its return");
            return NativeTaskStep::returned(3.0f);
        };
        task=publish(context,12,body);
        NativeRetailTaskExecution execution(*context.registry);
        RetailTaskExecutionFailure ef;
        auto result=execution.invoke_virtual_10_initial(task,ef);
        check(result && result->kind==NativeTaskRunKind::returned_nonnegative &&
              result->stored_wait==3.0f && body->cleanup_calls==0,
              "stock null +DC skips selected-cookie recheck after body return");
    }
    {
        Context context;
        std::vector<int> events;
        auto body=std::make_shared<Body>();
        body->events=&events;
        body->body_event=3;
        body->next=[] { return NativeTaskStep::returned(-1.0f); };
        std::shared_ptr<Observed> e8,ec;
        auto task=publish(context,13,body,&events,&e8,&ec);
        NativeRetailTaskExecution execution(*context.registry);
        RetailTaskExecutionFailure ef;
        auto result=execution.invoke_virtual_10_initial(task,ef);
        check(result && result->kind==NativeTaskRunKind::cleaned_negative &&
              result->stored_wait==-1.0f && body->cleanup_calls==1 &&
              e8->calls==1 && ec->calls==1 &&
              events==std::vector<int>{1,2,3},
              "stock negative return drains E8 then EC before native body release");
        RetailTaskRegistryFailure rf;
        check(!context.registry->current_task_observers_from_158080(rf) &&
              rf.kind==RetailTaskRegistryError::foreign_task,
              "negative branch clears current before virtual+C cleanup");
        auto observed=context.registry->observe(rf);
        check(observed && observed->ordered.size()==1 &&
              observed->ordered[0].object_cookie==0,
              "stock negative cleanup invalidates object before later stale recycle");
    }
    {
        Context context;
        std::vector<int> events;
        auto body=std::make_shared<Body>();
        body->events=&events;
        body->body_event=3;
        std::shared_ptr<NativeRetailTaskObject> task;
        body->next=[&]() {
            RetailTaskRegistryFailure rf;
            check(context.registry->store_object_retention_flag2_from_source(
                      task,true,rf),"live body writes source bit2 before negative return");
            return NativeTaskStep::returned(-2.0f);
        };
        std::shared_ptr<Observed> e8,ec;
        task=publish(context,14,body,&events,&e8,&ec);
        NativeRetailTaskExecution execution(*context.registry);
        RetailTaskExecutionFailure ef;
        auto result=execution.invoke_virtual_10_initial(task,ef);
        RetailTaskRegistryFailure rf;
        auto observed=context.registry->observe(rf);
        check(result && result->kind==NativeTaskRunKind::cleanup_guarded &&
              observed && observed->ordered[0].object_cookie==14 &&
              body->cleanup_calls==0 && e8->calls==0 && ec->calls==0 &&
              events.empty(),
              "late bit2 guard makes stock virtual+C return without release effects");
    }
    {
        Context context;
        auto body=std::make_shared<Body>();
        auto resume=std::make_shared<Continuation>();
        body->next=[resume] { return NativeTaskStep::wait(resume,1.0f); };
        auto task=publish(context,15,body);
        NativeRetailTaskExecution execution(*context.registry);
        RetailTaskExecutionFailure ef;
        check(!execution.invoke_virtual_10_initial(task,ef) &&
              ef.kind==RetailTaskExecutionError::unsupported_source_branch &&
              body->cleanup_calls==0 && resume->calls==0,
              "stock +14 JR RA cannot become an invented retained wait");
    }
    {
        Context context;
        auto body=std::make_shared<Body>();
        body->next=[] { return NativeTaskStep::returned(1.0f); };
        auto task=publish(context,16,body);
        NativeRetailTaskExecution execution(*context.registry);
        bool refused=false;
        std::thread foreign([&] {
            RetailTaskExecutionFailure ef;
            refused=!execution.invoke_virtual_10_initial(task,ef) &&
                    ef.kind==RetailTaskExecutionError::wrong_thread;
        });
        foreign.join();
        RetailTaskExecutionFailure ef;
        auto result=execution.invoke_virtual_10_initial(task,ef);
        check(refused && result && body->run_calls==1,
              "foreign stock dispatch refuses before body effects and owner can retry");
    }
    std::cout<<"native retail stock task execution fixture passed\n";
}
