#include "mka/retail_task_execution.hpp"
#include "mka/retail_task_registry.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <bit>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
void check(bool good,const char* message) {
    if(!good) throw std::runtime_error(message);
}
struct Steps final : NativeRetailTaskContinuation {
    explicit Steps(std::vector<NativeTaskStep> steps):steps(std::move(steps)) {}
    NativeTaskStep run() override {
        ++calls;
        if(cursor>=steps.size()) throw std::runtime_error("unexpected continuation replay");
        return steps[cursor++];
    }
    std::vector<NativeTaskStep> steps;
    std::size_t cursor{},calls{};
};
struct Body final : NativeRetailTaskBody {
    explicit Body(std::vector<NativeTaskStep> steps):steps(std::move(steps)) {}
    NativeTaskStep run() override {
        ++calls;
        if(throw_main) throw detail::NativeMainReentry{nullptr,9};
        if(throw_ordinary) throw std::runtime_error("body after-effect");
        if(nested_execution) {
            RetailTaskExecutionFailure nested_failure;
            const auto nested=nested_execution->invoke_virtual_10_initial(
                nested_task.lock(),nested_failure);
            nested_refused=!nested&&nested_failure.kind==RetailTaskExecutionError::reentrant;
        }
        if(set_flag2) {
            RetailTaskRegistryFailure f;
            check(registry->store_object_retention_flag2_from_source(task.lock(),true,f),
                  "body source flag2 writer");
        }
        if(invalidate) {
            RetailTaskRegistryFailure f;
            check(registry->store_object_cookie_from_source(task.lock(),0,f),
                  "body source identity writer");
        }
        if(cursor>=steps.size()) throw std::runtime_error("unexpected body replay");
        return steps[cursor++];
    }
    void cleanup(bool release_allowed) override {
        ++cleanups;
        last_release_allowed=release_allowed;
        if(throw_cleanup_main) throw detail::NativeMainReentry{nullptr,10};
        if(check_callout) {
            RetailTaskRegistryFailure f;
            const auto found=registry->find_first_live(0x2001,f);
            callout_refused=!found&&f.kind==RetailTaskRegistryError::callout_active;
        }
    }
    std::vector<NativeTaskStep> steps;
    std::size_t cursor{},calls{},cleanups{};
    bool last_release_allowed{},throw_main{},throw_cleanup_main{},throw_ordinary{},set_flag2{},invalidate{};
    bool check_callout{},callout_refused{},nested_refused{};
    NativeRetailTaskExecution* nested_execution{};
    std::weak_ptr<NativeRetailTaskObject> nested_task;
    NativeRetailTaskRegistry* registry{};
    std::weak_ptr<NativeRetailTaskObject> task;
};
std::shared_ptr<NativeRetailTaskObject> publish(
    NativeRetailTaskRegistry& registry,const std::shared_ptr<Body>& body) {
    RetailTaskRegistryFailure f;
    auto task=registry.publish_retained_task_from_12ea90(0x2001,0x20,17,body,f);
    check(task&&f.kind==RetailTaskRegistryError::none,
          "real retained-frame12EA90 body published with explicit constructor class");
    body->registry=&registry;body->task=task;
    return task;
}
}

int main() {
    try {
        RetailTaskRegistryFailure rf;
        RetailTaskExecutionFailure f;
        auto registry=NativeRetailTaskRegistry::create(rf);
        check(bool(registry),"native registry owner");
        NativeRetailTaskExecution execution(*registry);

        auto next=std::make_shared<Steps>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(2.0f)});
        auto initial=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::yield(next,-0.0f)});
        auto task=publish(*registry,initial);
        auto result=execution.invoke_virtual_10_initial(task,f);
        auto observed=execution.observe(task,f);
        check(result&&result->kind==NativeTaskRunKind::returned_nonnegative&&
              result->immediate_calls==2&&result->stored_wait==2.0f&&
              initial->calls==1&&next->calls==1&&observed&&
              observed->stored_wait==2.0f&&!observed->resume_pending,
              "negative zero yield replaces E0 and enters continuation immediately");

        auto repeat=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(0.0f),NativeTaskStep::returned(3.0f)});
        auto repeat_task=publish(*registry,repeat);
        result=execution.invoke_virtual_10_initial(repeat_task,f);
        check(result&&result->kind==NativeTaskRunKind::returned_nonnegative&&
              result->immediate_calls==2&&repeat->calls==2,
              "ordinary returned zero repeats same E0, not completion");

        auto resume=std::make_shared<Steps>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(1.0f)});
        auto waiter=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::wait(resume,0.0f)});
        auto wait_task=publish(*registry,waiter);
        result=execution.invoke_virtual_10_initial(wait_task,f);
        observed=execution.observe(wait_task,f);
        check(result&&result->kind==NativeTaskRunKind::retained_wait&&
              result->immediate_calls==1&&observed&&observed->resume_pending&&
              observed->stored_wait==0.0f&&resume->calls==0,
              "+14 wait at zero suspends with saved E4 instead of looping E0");
        result=execution.invoke_virtual_10_initial(wait_task,f);
        check(result&&result->kind==NativeTaskRunKind::returned_nonnegative&&
              resume->calls==1&&waiter->calls==1,
              "next source-ready entry resumes saved E4 once");

        auto negative=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(-1.0f)});
        negative->check_callout=true;
        auto negative_task=publish(*registry,negative);
        result=execution.invoke_virtual_10_initial(negative_task,f);
        auto order=registry->observe(rf);
        check(result&&result->kind==NativeTaskRunKind::cleaned_negative&&
              negative->cleanups==1&&negative->last_release_allowed&&
              negative->callout_refused&&order&&
              order->ordered.size()==4,
              "negative return clears current, guards cleanup callout, leaves source link");
        auto first=registry->find_first_live(0x7fffffffu,rf);
        order=registry->observe(rf);
        check(first&&!*first&&order&&order->ordered.size()==3&&
              order->recycled_links==1&&order->retained_links==0&&
              negative->cleanups==1,
              "later stale scan recycles released link without a second cleanup");

        auto cleanup_transfer_registry=NativeRetailTaskRegistry::create(rf);
        NativeRetailTaskExecution cleanup_transfer_execution(*cleanup_transfer_registry);
        auto cleanup_transfer=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(-1.0f)});
        cleanup_transfer->throw_cleanup_main=true;
        auto cleanup_transfer_task=publish(*cleanup_transfer_registry,cleanup_transfer);
        bool cleanup_transferred=false;
        try { cleanup_transfer_execution.invoke_virtual_10_initial(cleanup_transfer_task,f); }
        catch(const detail::NativeMainReentry& e) {
            cleanup_transferred=e.owner==nullptr&&e.value==10;
        }
        auto no_live=cleanup_transfer_registry->find_first_live(0x2001,rf);
        auto cleanup_transfer_state=cleanup_transfer_registry->observe(rf);
        check(cleanup_transferred&&no_live&&!*no_live&&cleanup_transfer_state&&
              cleanup_transfer_state->retained_links==1&&cleanup_transfer->cleanups==1,
              "12EE70 invalidates cookie before release transfer and keeps pending owner");
        std::weak_ptr<Body> pending_body=cleanup_transfer;
        std::weak_ptr<NativeRetailTaskObject> pending_task=cleanup_transfer_task;
        cleanup_transfer.reset();
        cleanup_transfer_task.reset();
        auto subsequent=publish(*cleanup_transfer_registry,
            std::make_shared<Body>(std::vector<NativeTaskStep>{
                NativeTaskStep::returned(1.0f)}));
        auto subsequent_result=cleanup_transfer_execution.invoke_virtual_10_initial(subsequent,f);
        auto retained_body=pending_body.lock();
        auto after_replacement=cleanup_transfer_registry->observe(rf);
        check(subsequent_result&&subsequent_result->kind==NativeTaskRunKind::returned_nonnegative&&
              !pending_task.expired()&&retained_body&&retained_body->cleanups==1&&
              after_replacement&&after_replacement->retained_links==1,
              "pending body survives caller release and selected-task replacement without replay");

        auto guarded=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(-2.0f)});
        guarded->set_flag2=true;
        auto guarded_task=publish(*registry,guarded);
        result=execution.invoke_virtual_10_initial(guarded_task,f);
        check(result&&result->kind==NativeTaskRunKind::cleanup_guarded&&
              guarded->cleanups==0&&
              registry->find_first_live(0x2001,rf),
              "negative cleanup sees cleared current; source bit2 returns before native release");

        auto invalid=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(4.0f)});
        invalid->invalidate=true;
        auto invalid_task=publish(*registry,invalid);
        result=execution.invoke_virtual_10_initial(invalid_task,f);
        check(result&&result->kind==NativeTaskRunKind::returned_nonnegative&&
              invalid->cleanups==0,
              "null source DC callback bypasses cookie recheck before nonnegative return");

        auto nested_target=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(1.0f)});
        auto nested_task=publish(*registry,nested_target);
        auto nesting=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(1.0f)});
        auto nesting_task=publish(*registry,nesting);
        nesting->nested_execution=&execution;
        nesting->nested_task=nested_task;
        result=execution.invoke_virtual_10_initial(nesting_task,f);
        check(result&&nesting->nested_refused&&nested_target->calls==0,
              "registry-wide execution guard refuses cross-task recursive invoke");

        auto detached_registry=NativeRetailTaskRegistry::create(rf);
        NativeRetailTaskExecution detached_execution(*detached_registry);
        auto detached_body=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(1.0f)});
        auto detached_task=publish(*detached_registry,detached_body);
        check(detached_registry->store_object_cookie_from_source(detached_task,8,rf)&&
              detached_registry->retire_link(detached_task,rf)&&
              !detached_execution.invoke_virtual_10_initial(detached_task,f)&&
              f.kind==RetailTaskExecutionError::foreign_task&&detached_body->calls==0,
              "retained detached task cannot be run as an active source link");

        auto transfer=std::make_shared<Body>(std::vector<NativeTaskStep>{});
        transfer->throw_main=true;
        auto transfer_task=publish(*registry,transfer);
        bool caught=false;
        try { execution.invoke_virtual_10_initial(transfer_task,f); }
        catch(const detail::NativeMainReentry& e) {
            caught=e.owner==nullptr&&e.value==9;
        }
        observed=execution.observe(transfer_task,f);
        auto post_transfer=publish(*registry,
            std::make_shared<Body>(std::vector<NativeTaskStep>{
                NativeTaskStep::returned(1.0f)}));
        check(caught&&observed&&observed->transfer_pending&&post_transfer&&
              registry->find_first_live(0x2001,rf),
              "typed MainReentry propagates and leaves unrelated registry lookup usable");

        auto malformed_registry=NativeRetailTaskRegistry::create(rf);
        NativeRetailTaskExecution malformed_execution(*malformed_registry);
        auto malformed=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::yield({},1.0f)});
        auto malformed_task=publish(*malformed_registry,malformed);
        check(!malformed_execution.invoke_virtual_10_initial(malformed_task,f)&&
              f.kind==RetailTaskExecutionError::malformed_step&&
              !malformed_registry->find_first_live(0x2001,rf)&&
              rf.kind==RetailTaskRegistryError::after_effect_unknown,
              "malformed typed yield after body effects latches named failure");

        auto ordinary_registry=NativeRetailTaskRegistry::create(rf);
        NativeRetailTaskExecution ordinary_execution(*ordinary_registry);
        auto ordinary=std::make_shared<Body>(std::vector<NativeTaskStep>{});
        ordinary->throw_ordinary=true;
        auto ordinary_task=publish(*ordinary_registry,ordinary);
        check(!ordinary_execution.invoke_virtual_10_initial(ordinary_task,f)&&
              f.kind==RetailTaskExecutionError::native_exception&&
              !ordinary_registry->find_first_live(0x2001,rf)&&
              rf.kind==RetailTaskRegistryError::after_effect_unknown,
              "ordinary body after-effect latches named registry failure");

        auto special_registry=NativeRetailTaskRegistry::create(rf);
        NativeRetailTaskExecution special_execution(*special_registry);
        const auto subnormal=std::bit_cast<float>(1u);
        auto special=std::make_shared<Body>(std::vector<NativeTaskStep>{
            NativeTaskStep::returned(subnormal)});
        auto special_task=publish(*special_registry,special);
        check(!special_execution.invoke_virtual_10_initial(special_task,f)&&
              f.kind==RetailTaskExecutionError::unsupported_float,
              "unproved EE subnormal parity is refused by name");

        bool wrong_thread=false;
        std::thread worker([&] {
            RetailTaskExecutionFailure off;
            wrong_thread=!execution.invoke_virtual_10_initial(task,off)&&
                off.kind==RetailTaskExecutionError::wrong_thread;
        });
        worker.join();
        check(wrong_thread,"execution owner thread is checked before source effect");

        std::puts("PASS native retail typed task control: immediate continuation, retained wait, direct cleanup; original bodies and Main dispatch open");
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"FAIL native retail typed task control: %s\n",e.what());
        return 1;
    }
}
