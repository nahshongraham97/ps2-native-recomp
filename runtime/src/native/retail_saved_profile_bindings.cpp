#include "mka/retail_saved_profile_records.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail {
namespace {
std::shared_ptr<NativeRetailGameSession> actual_session(NativeRetailMainState& main,
    StartupFailure& failure) noexcept {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = StartupError::saved_profile_records;
        failure.operation = "saved profile Main lifetime and owner thread";
        return {};
    }
    auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "saved profile retained actual Session";
        return {};
    }
    return session;
}
bool report_profile_failure(StartupFailure& failure) noexcept {
    failure.kind = StartupError::saved_profile_records;
    failure.operation = failure.saved_profile_records.operation;
    return false;
}
} // namespace
namespace main_binding {
RetailStartupGate initialize_retail_saved_profile_records(NativeRetailMainState& main,
    StartupFailure& failure) {
    const auto session = actual_session(main, failure);
    if (!session) return RetailStartupGate::native_failure;
    auto* records = session->saved_profile_records(failure.game);
    if (!records) {
        failure.kind = StartupError::game_session;
        failure.operation = failure.game.operation;
        return RetailStartupGate::native_failure;
    }
    if (!records->initialize_from_171b10(main, failure.saved_profile_records)) {
        report_profile_failure(failure);
        return RetailStartupGate::native_failure;
    }
    return RetailStartupGate::ready;
}
} // namespace main_binding
namespace initial_reset_required {
bool reset_saved_profile_children_246070(NativeRetailMainState& main, StartupFailure& failure) {
    const auto session = actual_session(main, failure);
    if (!session) return false;
    auto* records = session->saved_profile_records(failure.game);
    if (!records) {
        failure.kind = StartupError::game_session;
        failure.operation = failure.game.operation;
        return false;
    }
    if (!records->reset_first_two_from_246070(main, failure.saved_profile_records))
        return report_profile_failure(failure);
    return true;
}
} // namespace initial_reset_required
} // namespace mka::retail
