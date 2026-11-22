#include "mka/retail_frame_work_storage.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool initialize_retail_frame_work_buffers(NativeRetailMainState& main,StartupFailure& failure) {
    failure={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()||!main.startup.game_session||
       !main.startup.game_session->validate_current_thread(failure.game)) {
        failure.kind=StartupError::game_session;failure.operation="2E9930 actual Main/Session lifetime and thread";return false;
    }
    if(!main.startup.memory_owners) {
        failure.kind=StartupError::memory_owners;failure.operation="2E9930 actual native memory owners missing";return false;
    }
    RetailFrameWorkFailure f;
    auto allocator=NativeRetailRendererAllocatorService::create(*main.startup.memory_owners,f.allocator);
    if(!allocator) {
        failure.kind=StartupError::memory_owners;failure.operation="2E9930 actual Permanent heap capability";return false;
    }
    if(main.frame_work_storage) {
        if(main.frame_work_storage->same_heap(*allocator,f)&&main.frame_work_storage->reset_for_initialization(f))return true;
    } else {
        auto storage=NativeRetailFrameWorkStorage::create(main.lifetime,std::move(allocator),f);
        if(storage){main.frame_work_storage=std::move(storage);return true;}
    }
    failure.kind=StartupError::memory_owners;failure.operation=f.operation;return false;
}
}
