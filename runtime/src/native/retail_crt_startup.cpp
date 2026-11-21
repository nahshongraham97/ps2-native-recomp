#include "native_retail_startup.hpp"
namespace mka::retail {
bool initialize_native_retail_crt(NativeRetailStartupState& state,StartupFailure& failure) {
    failure={};
    if(state.crt){failure.kind=StartupError::already_initialized;failure.operation="retail CRT owner already initialized";return false;}
    auto owner=NativeRetailCrt::create(failure.crt);
    if(!owner){failure.kind=StartupError::crt;failure.operation="native CRT ownership creation";return false;}
    state.crt=std::move(owner);
    return true;
}
bool finalize_native_retail_pre_main(NativeRetailStartupState& state,StartupFailure& failure) {
    failure={};
    if(!state.crt){failure.kind=StartupError::crt;failure.operation="native CRT owner missing before publication";failure.crt.kind=RetailCrtError::invalid_capability;return false;}
    if(!state.crt->finalize_pre_main(failure.crt)){
        failure.kind=StartupError::crt;failure.operation="native CRT pre-main publication";return false;
    }
    return true;
}
} // namespace mka::retail
