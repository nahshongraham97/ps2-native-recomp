#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail::main_binding {
bool clear_retail_mode_stack(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    // This is the original185EC0 effect, not implicit readiness in a reader.
    if (!main.mode_history) main.mode_history.emplace();
    main.mode_history->clear();
    return true;
}

bool enter_retail_mode_zero(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.mode_history) {
        failure = {StartupError::native_exception, "retail mode history reset missing before push"};
        return false;
    }
    RetailModeFailure native;
    if (main.mode_history->push(0, native)) return true;
    failure = {StartupError::allocation, native.operation};
    return false;
}

std::optional<std::int32_t> read_retail_top_mode(const NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.mode_history) {
        failure = {StartupError::native_exception, "retail mode history reset missing before read"};
        return {};
    }
    //1830A0 reads this history, distinct from the185CB0 main-mode scalar.
    return main.mode_history->top();
}
} // namespace mka::retail::main_binding
