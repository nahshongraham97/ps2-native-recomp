#include "mka/retail_service_constructors.hpp"
#include "mka/retail_dispatch_owners.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <utility>
namespace mka::retail::main_binding {
bool construct_retail_aux_dispatch_object(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.auxiliary_dispatch_object) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "retail auxiliary prefix descriptor exists";
        return false;
    }
    RetailDispatchOwnerFailure construction;
    auto owner = NativeRetailPrefixDispatchDescriptor::create(construction);
    if (!owner) {
        failure.kind = construction.kind == RetailDispatchOwnerError::allocation
            ? StartupError::allocation : StartupError::native_exception;
        failure.operation = "retail auxiliary prefix descriptor construction";
        return false;
    }
    // Commit only the complete native owner. No registration or packet effect
    // is fabricated, and this owner is independent of the earlier descriptor.
    main.auxiliary_dispatch_object = std::move(owner);
    return true;
}
} // namespace mka::retail::main_binding
