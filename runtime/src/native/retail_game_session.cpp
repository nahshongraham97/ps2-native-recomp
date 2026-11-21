#include "mka/retail_game_session.hpp"
#include <new>
#include <utility>

namespace mka::retail {
NativeRetailGameSession::NativeRetailGameSession(std::shared_ptr<NativeRetailGameControls> controls,
    std::shared_ptr<NativeRetailMainSettings> settings,
    std::unique_ptr<NativeRetailTaskDomain> task_domain) noexcept
    : controls_(std::move(controls)), settings_(std::move(settings)),
      task_domain_(std::move(task_domain)) {}
std::shared_ptr<NativeRetailGameSession> NativeRetailGameSession::create(RetailGameSessionFailure& failure) noexcept {
    failure = {};
    try {
        auto controls = NativeRetailGameControls::create(failure.controls);
        if (!controls) {
            failure.kind = RetailGameSessionError::controls;
            failure.operation = "native game controls construction";
            return {};
        }
        auto settings = NativeRetailMainSettings::create(controls, failure.settings);
        if (!settings) {
            failure.kind = RetailGameSessionError::settings;
            failure.operation = "native game settings construction";
            return {};
        }
        auto task_domain = NativeRetailTaskDomain::create(controls, failure.task_domain);
        if (!task_domain) {
            failure.kind = RetailGameSessionError::task_domain;
            failure.operation = "native persistent task domain construction";
            return {};
        }
        return std::shared_ptr<NativeRetailGameSession>(new NativeRetailGameSession(
            std::move(controls), std::move(settings), std::move(task_domain)));
    } catch (const std::bad_alloc&) {
        failure.kind = RetailGameSessionError::allocation;
        failure.operation = "native game session allocation";
    } catch (...) {
        failure.kind = RetailGameSessionError::native_exception;
        failure.operation = "native game session construction";
    }
    return {};
}
bool NativeRetailGameSession::validate_current_thread(RetailGameSessionFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == owner_thread_) return true;
    failure.kind = RetailGameSessionError::wrong_thread;
    failure.operation = "native game session owner thread";
    return false;
}
std::shared_ptr<NativeRetailGameControls> NativeRetailGameSession::controls(RetailGameSessionFailure& failure) const noexcept {
    if (!validate_current_thread(failure)) return {};
    return controls_;
}
std::shared_ptr<NativeRetailMainSettings> NativeRetailGameSession::settings(RetailGameSessionFailure& failure) const noexcept {
    if (!validate_current_thread(failure)) return {};
    return settings_;
}
std::optional<std::int32_t> NativeRetailGameSession::scalar_mode(
    RetailGameSessionFailure& failure) const noexcept {
    if (!validate_current_thread(failure)) return {};
    return scalar_mode_;
}
bool NativeRetailGameSession::store_scalar_mode_from_source(
    std::int32_t mode, RetailGameSessionFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return false;
    scalar_mode_ = mode;
    return true;
}
NativeRetailRandom* NativeRetailGameSession::random(RetailGameSessionFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return nullptr;
    return &random_;
}
NativeRetailAuxiliaryWords* NativeRetailGameSession::auxiliary_words(RetailGameSessionFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return nullptr;
    return &auxiliary_;
}
NativeRetailInitialTaskState* NativeRetailGameSession::initial_task_state(
    RetailGameSessionFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return nullptr;
    return &initial_task_state_;
}
NativeRetailTaskDomain* NativeRetailGameSession::task_domain(
    RetailGameSessionFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return nullptr;
    return task_domain_.get();
}
RetailGameRecordResetSnapshot* NativeRetailGameSession::game_record_values(
    RetailGameSessionFailure& failure) noexcept {
    if (!validate_current_thread(failure)) return nullptr;
    return &game_record_values_;
}
} // namespace mka::retail
