#pragma once
#include "mka/retail_auxiliary_words.hpp"
#include "mka/retail_game_controls.hpp"
#include "mka/retail_main_settings.hpp"
#include "mka/retail_random.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {
enum class RetailGameSessionError { none, wrong_thread, allocation, controls, settings, native_exception };
struct RetailGameSessionFailure {
    RetailGameSessionError kind{RetailGameSessionError::none};
    const char* operation{""};
    RetailGameControlsFailure controls;
    RetailMainSettingsFailure settings;
    RetailRandomFailure random;
};
// Native game globals outlive an individual main/frame-control object. This
// owner has no guest address map, register state or platform device model.
// Controls model only proven game-info state, not a complete registry value.
class NativeRetailGameSession final {
public:
    static std::shared_ptr<NativeRetailGameSession> create(RetailGameSessionFailure&) noexcept;
    NativeRetailGameSession(const NativeRetailGameSession&) = delete;
    NativeRetailGameSession& operator=(const NativeRetailGameSession&) = delete;
    bool validate_current_thread(RetailGameSessionFailure&) const noexcept;
    std::shared_ptr<NativeRetailGameControls> controls(RetailGameSessionFailure&) const noexcept;
    std::shared_ptr<NativeRetailMainSettings> settings(RetailGameSessionFailure&) const noexcept;
    // 185CB0/185CC0 scalar game mode; distinct from history, reentry and profile.
    std::optional<std::int32_t> scalar_mode(RetailGameSessionFailure&) const noexcept;
    bool store_scalar_mode_from_source(std::int32_t, RetailGameSessionFailure&) noexcept;
    // Borrowed values require this retained session to span the call.
    NativeRetailRandom* random(RetailGameSessionFailure&) noexcept;
    NativeRetailAuxiliaryWords* auxiliary_words(RetailGameSessionFailure&) noexcept;
private:
    NativeRetailGameSession(std::shared_ptr<NativeRetailGameControls>,
        std::shared_ptr<NativeRetailMainSettings>) noexcept;
    std::shared_ptr<NativeRetailGameControls> controls_;
    std::shared_ptr<NativeRetailMainSettings> settings_;
    NativeRetailRandom random_;
    NativeRetailAuxiliaryWords auxiliary_;
    std::int32_t scalar_mode_{}; // Source scalar has BSS zero birth, no guest storage.
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
