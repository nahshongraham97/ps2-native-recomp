#include "native_retail_startup.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace {
bool fail_allocation=false;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
// Test-only cooperative target. It records actual dispatch and validates the
// receipt running state; no game task, device or source ABI is represented.
struct Target final:NativeCrtTaskCapability {
    bool runnable=false,fail=false;unsigned wakes=0,parks=0,yields=0;
    NativeRetailCrt* crt=nullptr;std::shared_ptr<NativeCrtWorkReceipt> receipt;
    bool visit(NativeCrtTaskOperation op,RetailCrtFailure& f){
        check(receipt&&receipt->observe().state==NativeCrtWorkState::running,"receipt running during concrete effect");
        RetailCrtFailure nested;check(!crt->pump(1,nested)&&nested.kind==RetailCrtError::work_active,"nested work cannot run recursively");
        if(fail){f.kind=RetailCrtError::invalid_capability;return false;}
        if(op==NativeCrtTaskOperation::wake){runnable=true;++wakes;}
        else if(op==NativeCrtTaskOperation::park){runnable=false;++parks;}
        else {++yields;std::this_thread::yield();}
        return true;
    }
    bool wake(RetailCrtFailure& f)override{return visit(NativeCrtTaskOperation::wake,f);}
    bool park(RetailCrtFailure& f)override{return visit(NativeCrtTaskOperation::park,f);}
    bool yield(RetailCrtFailure& f)override{return visit(NativeCrtTaskOperation::yield,f);}
};
struct Argument final:NativeCrtCallbackArgument {
    NativeRetailCrt* crt;NativeCrtCallbackToken* token;bool* destroyed;
    ~Argument(){*destroyed=true;}
};
bool callback(const std::shared_ptr<const NativeCrtCallbackArgument>& base,RetailCrtFailure&){
    const auto arg=std::static_pointer_cast<const Argument>(base);RetailCrtFailure f;
    check(!arg->crt->invoke_callback(*arg->token,f)&&f.kind==RetailCrtError::work_active,"recursive callback rejected");
    check(!arg->crt->shutdown(f)&&f.kind==RetailCrtError::work_active,"active callback blocks shutdown");
    check(arg->crt->unregister_callback(*arg->token,f),"active callback can remove registration");
    check(!*arg->destroyed,"owned callback argument survives removal");return true;
}
}
void* operator new(std::size_t size){if(fail_allocation)throw std::bad_alloc();if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){try{
    check(argc==2,"existing fixture content root");mka::content::ContentVolumeFailure cf;
    auto volume=mka::content::NativeContentVolume::open(std::filesystem::u8path(argv[1]),cf);check(bool(volume),"real content volume");
    NativeRetailStartupState startup(std::move(*volume),{"cdrom0:\\SLUS_214.10"});StartupFailure f;
    check(!finalize_native_retail_pre_main(startup,f)&&f.kind==StartupError::crt&&!startup.crt,"pre-main cannot publish missing owner");
    fail_allocation=true;const auto initialized=initialize_native_retail_crt(startup,f);fail_allocation=false;
    check(!initialized&&f.kind==StartupError::crt&&f.crt.kind==RetailCrtError::allocation&&!startup.crt,"real allocation failure is unpublished");
    check(initialize_native_retail_crt(startup,f)&&startup.crt&&!startup.memory_owners,"production CRT publishes only actual CRT owner");
    auto* crt=startup.crt.get();check(!initialize_native_retail_crt(startup,f)&&f.kind==StartupError::already_initialized&&crt==startup.crt.get(),"duplicate cannot replace owner");
    RetailCrtFailure c;check(!crt->pump(1,c)&&c.kind==RetailCrtError::not_finalized,"work waits for real pre-main publication");
    bool foreign=false;std::thread worker([&]{StartupFailure tf;foreign=!finalize_native_retail_pre_main(startup,tf)&&tf.crt.kind==RetailCrtError::wrong_thread;});worker.join();check(foreign,"foreign thread cannot finalize");
    check(finalize_native_retail_pre_main(startup,f),"actual production pre-main finalization");
    std::thread foreign_platform([&]{StartupFailure tf;foreign=!initialize_native_retail_platform(startup,tf)&&
        tf.kind==StartupError::crt&&tf.crt.kind==RetailCrtError::wrong_thread&&!startup.platform;});
    foreign_platform.join();check(foreign,"platform cannot open endpoints on foreign CRT thread");
    check(!finalize_native_retail_pre_main(startup,f)&&f.crt.kind==RetailCrtError::already_finalized,"duplicate finalization explicit");
    const auto before=crt->now_nanoseconds(c),after=crt->now_nanoseconds(c);check(before&&after&&*after>=*before,"real monotonic clock epoch");
    check(!crt->submit({},NativeCrtTaskOperation::wake,c)&&c.kind==RetailCrtError::invalid_capability,"null capability rejected");
    auto target=std::make_shared<Target>();target->crt=crt;
    check(!crt->submit(target,static_cast<NativeCrtTaskOperation>(99),c)&&c.kind==RetailCrtError::invalid_capability,"unknown operation rejected");
    for(auto op:{NativeCrtTaskOperation::wake,NativeCrtTaskOperation::park,NativeCrtTaskOperation::yield}){
        target->receipt=crt->submit(target,op,c);check(bool(target->receipt),"actual pending receipt");
        check(target->receipt->observe().state==NativeCrtWorkState::pending,"effect deferred, never fabricated complete");
        check(!crt->shutdown(c)&&c.kind==RetailCrtError::work_active,"pending effect blocks shutdown");
        const auto count=crt->pump(1,c);check(count&&*count==1&&target->receipt->observe().state==NativeCrtWorkState::completed,"real dispatch completion");
    }
    check(target->wakes==1&&target->parks==1&&target->yields==1&&!target->runnable,"exact concrete operation selected");
    target->fail=true;target->receipt=crt->submit(target,NativeCrtTaskOperation::wake,c);check(crt->pump(1,c)==1,"failed effect consumed once");
    check(target->receipt->observe().state==NativeCrtWorkState::failed&&target->receipt->observe().failure.kind==RetailCrtError::invalid_capability,"failed effect never successful completion");target->receipt.reset();
    bool destroyed=false;NativeCrtCallbackToken token;auto argument=std::make_shared<Argument>();argument->crt=crt;argument->token=&token;argument->destroyed=&destroyed;
    const auto registered=crt->register_callback(callback,argument,c);check(bool(registered),"typed callback registered");token=*registered;argument.reset();
    check(!crt->shutdown(c)&&c.kind==RetailCrtError::callbacks_active,"registered callback blocks shutdown");
    check(crt->invoke_callback(token,c)&&destroyed,"callback lifetime retained to completion");
    check(!crt->invoke_callback(token,c)&&c.kind==RetailCrtError::unknown_callback,"retired token cannot run");
    // Dynamic admission exceeds the original fixed512-entry queue without any
    // source hardware capacity or magic guest ID. No effect runs before pump.
    std::vector<std::shared_ptr<NativeCrtWorkReceipt>> pending;pending.reserve(541);
    for(unsigned i=0;i<541;++i)pending.push_back(crt->submit(target,NativeCrtTaskOperation::wake,c));
    for(const auto& receipt:pending)check(receipt&&receipt->observe().state==NativeCrtWorkState::pending,"dynamic owned admission");
    startup.crt.reset();for(const auto& receipt:pending)check(receipt->observe().state==NativeCrtWorkState::failed&&receipt->observe().failure.kind==RetailCrtError::closed,"unexecuted work destruction is failure");
    auto closed=NativeRetailCrt::create(c);check(closed&&closed->finalize_pre_main(c)&&closed->shutdown(c),"normal shutdown");
    check(!closed->now_nanoseconds(c)&&c.kind==RetailCrtError::closed&&!closed->submit(target,NativeCrtTaskOperation::wake,c),"closed owner cannot admit effects");
    std::puts("PASS production native CRT ownership/lifecycle, deferred work and callbacks; no game or device");return 0;
}catch(const std::exception& error){fail_allocation=false;std::fprintf(stderr,"FAIL native CRT: %s\n",error.what());return 1;}}
