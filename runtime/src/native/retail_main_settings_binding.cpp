#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
namespace mka::retail::main_binding {
namespace {
std::shared_ptr<NativeRetailGameSession> require_session(const NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "native persistent game session missing or unavailable";
        return {};
    }
    return session;
}
void settings_failure(StartupFailure& failure, const char* operation) {
    failure.kind = StartupError::game_session;
    failure.operation = operation;
}
}
std::optional<std::uint32_t> read_retail_profile_selector(NativeRetailMainState& main,
    StartupFailure& failure) {
    const auto session = require_session(main, failure);
    if (!session) return {};
    const auto settings = session->settings(failure.game);
    if (!settings) { settings_failure(failure, "native profile settings owner"); return {}; }
    const auto value = settings->profile_selector(failure.game.settings);
    if (!value) settings_failure(failure, "native retail profile selector");
    return value;
}
std::optional<bool> retail_timing_override_active(const NativeRetailMainState& main,
    StartupFailure& failure) {
    const auto session = require_session(main, failure);
    if (!session) return {};
    const auto settings = session->settings(failure.game);
    if (!settings) { settings_failure(failure, "native timing settings owner"); return {}; }
    const auto value = settings->timing_override_active(failure.game.settings);
    if (!value) settings_failure(failure, "native retail timing override read");
    return value;
}
std::optional<std::int32_t> read_retail_frame_rate(const NativeRetailMainState& main,
    StartupFailure& failure) {
    const auto session = require_session(main, failure);
    if (!session) return {};
    const auto settings = session->settings(failure.game);
    if (!settings) { settings_failure(failure, "native frame-rate settings owner"); return {}; }
    const auto value = settings->effective_frame_rate(failure.game.settings);
    if (!value) settings_failure(failure, "native retail effective frame rate");
    return value;
}
std::optional<std::uint16_t> read_retail_startup_seed_component(NativeRetailMainState& main,
    StartupFailure& failure) {
    failure = {};
    if (!main.startup.crt) {
        failure = {StartupError::crt, "native startup seed clock owner missing"};
        return {};
    }
    if (!main.startup.crt->validate_current_thread(failure.crt)) {
        failure.kind = StartupError::crt;
        failure.operation = "native startup seed clock validation";
        return {};
    }
    const auto counter = main.startup.crt->now_nanoseconds(failure.crt);
    if (!counter) {
        failure.kind = StartupError::crt;
        failure.operation = "native startup seed counter sample";
        return {};
    }
    // Native platform entropy policy: low bits of the retained CRT's elapsed
    // counter. No EE Count register, frequency or epoch is reproduced.
    return static_cast<std::uint16_t>(*counter & 0xffffu);
}
bool seed_retail_startup_random_state(NativeRetailMainState& main, std::uint32_t seed,
    StartupFailure& failure) {
    const auto session = require_session(main, failure);
    if (!session) return false;
    auto* random = session->random(failure.game);
    if (!random || !random->seed(seed, failure.game.random)) {
        failure.kind = StartupError::game_session;
        failure.operation = "native retail startup random seed";
        return false;
    }
    return true;
}
} // namespace mka::retail::main_binding
