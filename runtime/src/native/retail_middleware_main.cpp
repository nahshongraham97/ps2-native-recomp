#include "mka/retail_main.hpp"
#include "mka/retail_middleware_frame.hpp"
#include "mka/retail_middleware_work.hpp"
#include "native_retail_startup.hpp"
namespace mka::retail::main_binding {
bool pump_retail_middleware(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.startup.crt) {
        failure = {StartupError::crt, "retail middleware frame CRT missing"};
        return false;
    }
    if (!main.startup.middleware_frame) {
        failure = {StartupError::middleware_frame, "retail middleware frame owner missing"};
        return false;
    }
    if (!main.middleware_dispatcher || !main.middleware_work_group) {
        failure = {StartupError::middleware, "retail middleware dispatcher/group missing"};
        return false;
    }
    const auto* const frame = main.startup.middleware_frame.get();
    const auto* const crt = main.startup.crt.get();
    // Retain the actual pair across native completion callouts.
    const auto dispatcher = main.middleware_dispatcher;
    const auto group = main.middleware_work_group;
    if (!dispatcher->validate_attachment(group, failure.middleware)) {
        failure.kind = StartupError::middleware;
        failure.operation = "retail middleware frame attachment validation";
        return false;
    }
    if (!main.startup.middleware_frame->sample(*main.startup.crt, failure.middleware_frame)) {
        failure.kind = StartupError::middleware_frame;
        failure.operation = "retail middleware native time sample";
        return false;
    }
    if (!dispatcher->pump(failure.middleware)) {
        failure.kind = StartupError::middleware;
        failure.operation = "retail middleware actual dispatcher pump";
        return false;
    }
    if (main.startup.middleware_frame.get() != frame) {
        failure = {StartupError::middleware_frame, "retail middleware frame identity changed during completion"};
        return false;
    }
    if (main.startup.crt.get() != crt) {
        failure = {StartupError::crt, "retail middleware CRT identity changed during completion"};
        return false;
    }
    // Source328CE0 selects the callback after dispatcher work. The selected
    // native callable is retained during invocation; typed game reentry must
    // propagate to the real main loop rather than become an ordinary error.
    if (!main.startup.middleware_frame->invoke_after_dispatcher(
            *main.startup.crt, 0, failure.middleware_frame)) {
        failure.kind = StartupError::middleware_frame;
        failure.operation = "retail middleware native frame callback";
        return false;
    }
    return true;
}
} // namespace mka::retail::main_binding
