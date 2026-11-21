#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool reset_retail_auxiliary_words(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    // 26F1E0: first zero store, return, second zero store in its delay slot.
    // Neighboring transfer/resource owners and the PRNG are not reset here.
    main.auxiliary_words.first = 0;
    main.auxiliary_words.second = 0;
    return true;
}
} // namespace mka::retail::main_binding
