#include "mka/retail_action_history.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

#include <memory>
#include <new>

namespace mka::retail {

void NativeRetailActionHistory::reset() noexcept {
    // 163210 traverses 30 20-byte source records in each of four journals.
    // Its only record stores are at +00, +04 and +08. Clearing the owned
    // string represents the original empty literal at 5F93F0.
    for (auto& journal : journals) {
        for (auto& record : journal.records) {
            record.action = -1;
            record.frame = -1;
            record.label.clear();
        }
    }
    // The two recent cursors are separate from the four journal cursors.
    for (auto& history : recent) {
        history.cursor = 0;
        history.canonical.fill(-1);
    }
}

namespace main_binding {
bool initialize_retail_action_service(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.action_history) {
        main.action_history->reset();
        return true;
    }
    try {
        auto history = std::make_unique<NativeRetailActionHistory>();
        history->reset();
        main.action_history = std::move(history);
        return true;
    } catch (const std::bad_alloc&) {
        failure = {StartupError::allocation, "retail action journal allocation"};
        return false;
    } catch (...) {
        failure = {StartupError::native_exception, "retail action journal construction"};
        return false;
    }
}
} // namespace main_binding

} // namespace mka::retail
