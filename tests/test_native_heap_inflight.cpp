#include "mka/native_heap.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>
#include <new>
#include <thread>
#include <utility>

using namespace mka::memory;
namespace {
void check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::abort();}}
enum class Operation {none,allocate,release};
struct HostGate {
    std::mutex mutex;
    std::condition_variable changed;
    Operation operation=Operation::none;
    bool entered=false,opened=false,throw_failure=false;
    void* watched=nullptr;
    void arm(Operation next,bool fail=false,void* pointer=nullptr){
        std::lock_guard lock(mutex);operation=next;entered=opened=false;throw_failure=fail;watched=pointer;
    }
    bool pause(Operation current,std::size_t bytes,std::align_val_t alignment,void* pointer){
        std::unique_lock lock(mutex);
        if(operation!=current)return false;
        if(current==Operation::allocate&&(bytes!=512||alignment!=std::align_val_t{256}))return false;
        if(current==Operation::release&&pointer!=watched)return false;
        entered=true;changed.notify_all();
        changed.wait(lock,[&]{return opened;});
        operation=Operation::none;return throw_failure;
    }
    void wait(){
        std::unique_lock lock(mutex);
        check(changed.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}),"actual host allocator/deleter reached");
    }
    void open(){std::lock_guard lock(mutex);opened=true;changed.notify_all();}
} gate;
auto owner(const char* name){
    NativeHeapFailure f;auto result=NativeHeapOwner::create(name,{},f);
    check(bool(result),"real native heap owner");return result;
}
void verify_access(const std::shared_ptr<NativeHeapOwner>& owner,std::size_t live,std::size_t inflight,bool actual){
    auto probe=std::async(std::launch::async,[&]{return owner->observe();});
    check(probe.wait_for(std::chrono::seconds(5))==std::future_status::ready,
        "owner mutex is not held across host allocation or delete");
    const auto state=probe.get();
    check(state.active&&state.live_allocations==live&&state.inflight_allocations==inflight&&state.child_owners==0,
        "exact live/inflight ownership without hidden child or anchor");
    check(state.requested_live_allocations==(actual?0:live)&&state.actual_live_allocations==(actual?live:0)&&state.active_requests==0,
        "actual/requested roles change only at host allocation/deletion completion");
    auto close=std::async(std::launch::async,[&]{NativeHeapFailure f;return std::pair{owner->shutdown(f),f.kind};});
    check(close.wait_for(std::chrono::seconds(5))==std::future_status::ready,"shutdown can inspect live host transaction");
    const auto result=close.get();
    check(!result.first&&result.second==NativeHeapError::owner_busy,"only actual in-flight/live work blocks shutdown");
}
void allocation_cut(bool throw_failure){
    auto requested=owner("requested"),actual=owner("actual");
    const auto requested_identity=requested->observe().identity,actual_identity=actual->observe().identity;
    gate.arm(Operation::allocate,throw_failure);
    std::optional<NativeAllocation> result;NativeHeapFailure f;
    std::thread worker([&]{result=requested->allocate_from(actual,384,f,256);});
    gate.wait();
    verify_access(requested,0,1,false);verify_access(actual,0,1,true);
    gate.open();worker.join();
    check(requested->observe().identity==requested_identity&&actual->observe().identity==actual_identity,
        "failed concurrent shutdown cannot revoke either owner generation");
    if(throw_failure){
        check(!result&&f.kind==NativeHeapError::allocation_failed,"host failure delivered through actual allocation path");
        check(requested->observe().live_allocations==0&&actual->observe().live_allocations==0&&
            requested->observe().inflight_allocations==0&&actual->observe().inflight_allocations==0,
            "real failed transaction rolls back both reservations");
        result=requested->allocate_from(actual,384,f,256);
        check(result.has_value(),"owners remain usable after failed host allocation");
    }
    check(result&&result->requested_owner()==requested_identity&&result->actual_owner()==actual_identity,
        "result preserves actual/requested native ownership");
    static_cast<unsigned char*>(result->data())[383]=71;
    check(static_cast<unsigned char*>(result->data())[383]==71,"real host storage remains usable");
    check(result->release_from(*actual,f)&&requested->shutdown(f)&&actual->shutdown(f),"actual completion permits retirement");
}
void release_cut(){
    auto requested=owner("release requested"),actual=owner("release actual");
    NativeHeapFailure f;auto allocation=requested->allocate_from(actual,384,f,256);
    check(allocation.has_value(),"real release candidate");
    gate.arm(Operation::release,false,allocation->data());
    std::thread worker([&]{NativeHeapFailure error;check(allocation->release(error),"actual owned release");});
    gate.wait();
    verify_access(requested,1,0,false);verify_access(actual,1,0,true);
    gate.open();worker.join();
    check(requested->observe().live_allocations==0&&actual->observe().live_allocations==0&&
        requested->shutdown(f)&&actual->shutdown(f),"counter retirement follows actual host delete");
}
}
// The native library has no gate or injection path. These symbols are wrapped
// only by this fixture's linker command, around the actual host allocator.
extern "C" void* __real__ZnwySt11align_val_t(std::size_t,std::align_val_t);
extern "C" void __real__ZdlPvSt11align_val_t(void*,std::align_val_t) noexcept;
extern "C" void* __wrap__ZnwySt11align_val_t(std::size_t bytes,std::align_val_t alignment){
    if(gate.pause(Operation::allocate,bytes,alignment,nullptr))throw std::bad_alloc{};
    return __real__ZnwySt11align_val_t(bytes,alignment);
}
extern "C" void __wrap__ZdlPvSt11align_val_t(void* pointer,std::align_val_t alignment)noexcept {
    gate.pause(Operation::release,0,alignment,pointer);
    __real__ZdlPvSt11align_val_t(pointer,alignment);
}
int main(){
    allocation_cut(false);allocation_cut(true);release_cut();
    std::puts("PASS_NATIVE_HEAP_INFLIGHT: real host allocation success/failure and deletion cuts; test-only linker gates");
    return std::fflush(stdout)||std::ferror(stdout)?1:0;
}
