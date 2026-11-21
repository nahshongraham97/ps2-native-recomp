#include "native_typed_retail.hpp"

#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
using namespace mka::retail::generated;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}
int main() {
    try {
        RetailGameSessionFailure failure;
        auto session = NativeRetailGameSession::create(failure);
        check(bool(session), "actual native session creation");
        auto retained = session;
        const std::weak_ptr<NativeRetailGameSession> lifetime = session;
        auto controls = session->controls(failure);
        auto settings = session->settings(failure);
        auto* auxiliary = session->auxiliary_words(failure);
        auto* random = session->random(failure);
        check(controls && settings && auxiliary && random, "actual native owner bindings");
        RetailGameControlsFailure controls_failure;
        RetailMainSettingsFailure settings_failure;
        RetailRandomFailure random_failure;
        NativeRetailRandom reference;
        auxiliary->first = 0x76543210u;
        auxiliary->second = 0xabcdef01u;
        check(controls->store_selection_flags_from_source(3, controls_failure) &&
              controls->store_analog_input_inhibited_from_source(true, controls_failure),
              "nondefault independent control state");
        check(retail_scalar_mode_get_185cb0(*session, failure) == 0,
              "generated getter reaches actual source BSS owner");
        const std::array values{std::numeric_limits<std::int32_t>::min(), -1, 0, 17,
                                std::numeric_limits<std::int32_t>::max()};
        for (const auto value : values) {
            failure.kind = RetailGameSessionError::native_exception;
            check(retail_scalar_mode_set_185cc0(*session, value, failure) &&
                  failure.kind == RetailGameSessionError::none,
                  "generated source-void setter returns native checked completion");
            failure.kind = RetailGameSessionError::native_exception;
            check(retail_scalar_mode_get_185cb0(*session, failure) == value &&
                  failure.kind == RetailGameSessionError::none,
                  "generated getter preserves signed value and clears stale status");
            check(settings->profile_selector(settings_failure) == 13 &&
                  settings->stored_frame_rate(settings_failure) == 60 &&
                  controls->selection_state_word(controls_failure) == 3 &&
                  controls->analog_input_inhibited(controls_failure) == true &&
                  auxiliary->first == 0x76543210u && auxiliary->second == 0xabcdef01u &&
                  random->next(random_failure) == reference.next(random_failure),
                  "generated scalar effects preserve distinct settings/control/auxiliary/random state");
        }
        bool foreign_refused = false;
        std::thread foreign([&] {
            RetailGameSessionFailure other;
            foreign_refused = !retail_scalar_mode_get_185cb0(*session, other) &&
                other.kind == RetailGameSessionError::wrong_thread &&
                !retail_scalar_mode_set_185cc0(*session, -99, other) &&
                other.kind == RetailGameSessionError::wrong_thread;
        });
        foreign.join();
        check(foreign_refused && retail_scalar_mode_get_185cb0(*session, failure) == values.back(),
              "generated foreign-thread calls refuse without mutation");
        session.reset();
        check(!lifetime.expired() && retail_scalar_mode_set_185cc0(*retained, -37, failure) &&
              retail_scalar_mode_get_185cb0(*retained, failure) == -37 &&
              retained->controls(failure) == controls && retained->settings(failure) == settings,
              "generated calls use retained persistent native ownership");
        retained.reset();
        check(lifetime.expired(), "generated layer does not create hidden session ownership");
        std::puts("PASS compiled typed retail AOT: original getter/setter, signed state, owner lifetime, thread refusal and independent state; no guest runtime or endpoint");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL compiled typed retail AOT: %s\n", error.what());
        return 1;
    }
}
