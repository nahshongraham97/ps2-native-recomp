#include "mka/retail_main.hpp"
#include "mka/native_user_paths.hpp"
#include "native_retail_startup.hpp"

#include <new>
#include <stdexcept>

namespace mka::retail::main_binding {

bool initialize_retail_saved_profile_storage(NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    try {
        if (!main.saved_profile_storage) {
            platform::NativeUserPaths paths;
            if (!platform::NativeUserPaths::resolve(paths, failure.saved_profile_user_paths)) {
                failure.kind = StartupError::saved_profile_storage;
                failure.operation = "225000 native user save directory";
                return false;
            }
            auto storage = save::NativeSaveStore::open_or_create(
                paths.application() / "saves", failure.saved_profile_storage);
            if (!storage) {
                failure.kind = StartupError::saved_profile_storage;
                failure.operation = "225000 native writable profile storage";
                return false;
            }
            if (!storage->validate(failure.saved_profile_storage)) {
                failure.kind = StartupError::saved_profile_storage;
                failure.operation = "225000 new native profile storage identity";
                return false;
            }
            main.saved_profile_storage = std::move(storage);
        } else if (!main.saved_profile_storage->validate(failure.saved_profile_storage)) {
            failure.kind = StartupError::saved_profile_storage;
            failure.operation = "225000 retained native profile storage identity";
            return false;
        }
        // Original225000 clears64E8D8 only after11BFA0 reports readiness.
        main.saved_profile_service_word = 0;
        return true;
    } catch (const std::bad_alloc&) {
        failure.kind = StartupError::allocation;
    } catch (const std::length_error&) {
        failure.kind = StartupError::host_capacity;
    } catch (...) {
        failure.kind = StartupError::native_exception;
    }
    failure.operation = "225000 native profile storage initialization";
    return false;
}

} // namespace mka::retail::main_binding
