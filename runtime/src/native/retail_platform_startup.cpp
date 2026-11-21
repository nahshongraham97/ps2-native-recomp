#include "native_retail_startup.hpp"
namespace mka::retail {
bool initialize_native_retail_platform(NativeRetailStartupState& state,StartupFailure& f) {
    f={};
    if(state.platform){f.kind=StartupError::already_initialized;f.operation="native retail platform already initialized";return false;}
    if(!state.crt){
        f.kind=StartupError::platform;f.operation="native platform requires live published CRT";f.platform.kind=RetailPlatformError::missing_crt;return false;
    }
    if(!state.crt->validate_current_thread(f.crt)){
        f.kind=StartupError::crt;f.operation="native platform CRT owner-thread validation";return false;
    }
    auto platform=NativeRetailPlatform::create(state.content,f.platform);
    if(!platform){f.kind=StartupError::platform;f.operation="native retail window/input/loading presentation";return false;}
    state.platform=std::move(platform);return true;
}
} // namespace mka::retail
