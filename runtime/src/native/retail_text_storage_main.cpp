#include "mka/retail_main.hpp"
#include "mka/retail_text_storage.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
namespace {
bool fail(StartupFailure& f,const char* operation) {
    f.kind=StartupError::text_storage; f.operation=operation; return false;
}
bool session_access(NativeRetailMainState& main,StartupFailure& f) {
    if(!main.startup.game_session) {
        f.text_storage.kind=RetailTextStorageError::missing_owner;
        return fail(f,"native text storage actual Session missing");
    }
    if(!main.startup.game_session->validate_current_thread(f.game))
        return fail(f,"native text storage actual Session thread");
    return true;
}
}
bool install_retail_text_allocator_pair_16fd80(NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!session_access(main,f)) return false;
    if(!main.startup.memory_owners || !main.startup.game_pool) {
        f.text_storage.kind=RetailTextStorageError::missing_owner;
        return fail(f,"16FD80 actual memory group/game pool missing");
    }
    auto wave=main.startup.memory_owners->wave_allocator(f.memory);
    if(!wave) return fail(f,"16FD80 source Wave allocation service missing");
    if(main.startup.text_storage) {
        // Source2EBB20 republishes the same pair; it does not clear the
        // existing64DF2C deferred list on normal repeated initialization.
        if(!main.startup.text_storage->same_bindings(wave,main.startup.game_pool,f.text_storage))
            return fail(f,"16FD80 changed native allocation/pool identity");
        return true;
    }
    auto storage=NativeRetailTextStorage::create(std::move(wave),main.startup.game_pool,f.text_storage);
    if(!storage) return fail(f,"16FD80 install real native text allocation/free service");
    main.startup.text_storage=std::move(storage);
    return true;
}
bool retire_retail_deferred_storage(NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!session_access(main,f)) return false;
    if(!main.startup.text_storage) {
        f.text_storage.kind=RetailTextStorageError::missing_owner;
        return fail(f,"12D930 source16FD80 allocation service missing");
    }
    if(!main.startup.text_storage->advance_frame_from_12d930(f.text_storage))
        return fail(f,"12D930 actual native deferred-free frame");
    return true;
}
} // namespace mka::retail::main_binding
