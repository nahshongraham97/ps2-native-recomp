#include "mka/retail_initial_task_required.hpp"
#include "mka/retail_initial_task.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include <memory>
#include <utility>

namespace mka::retail::initial_task_required {
namespace {

// Native initial child representing authentic retail movie 0 (LOGON.SFD / LOGOW.SFD) playback.
// In retail state machine table 0x005a3410 slot 0, begin_menu_18d450 advances the logo movie.
class NativeRetailMovie0Child final : public NativeRetailInitialChild {
public:
    NativeRetailMovie0Child(NativeRetailMainState& main, NativeRetailGameSession& session) noexcept
        : main_(main), session_(session) {}

    RetailInitialChildStep advance() override {
        if (!stepped_) {
            stepped_ = true;
            // First step: video frame advancement wait tick (1.0f frame)
            return RetailInitialChildStep::wait(1.0f);
        }
        // Subsequent step: movie playback complete
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    [[maybe_unused]] NativeRetailGameSession& session_;
    bool stepped_{false};
};

// Native initial child representing authentic retail boot intro sequence (0x0018deb0).
// In retail 0x0018deb0, intro playback waits 300.0f ticks before prompting.
class NativeRetailIntro18deb0Child final : public NativeRetailInitialChild {
public:
    NativeRetailIntro18deb0Child(NativeRetailMainState& main, NativeRetailGameSession& session) noexcept
        : main_(main), session_(session) {}

    RetailInitialChildStep advance() override {
        if (!waited_) {
            waited_ = true;
            // Retail 0x0018deb0 intro sequence: 300.0f frame duration
            return RetailInitialChildStep::wait(300.0f);
        }
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    [[maybe_unused]] NativeRetailGameSession& session_;
    bool waited_{false};
};

} // namespace

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d450(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    return std::make_shared<NativeRetailMovie0Child>(main, session);
}

std::shared_ptr<NativeRetailInitialChild> begin_intro_18deb0(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    return std::make_shared<NativeRetailIntro18deb0Child>(main, session);
}

void transition_1225d0(NativeRetailMainState& main, std::uint32_t mode,
    [[maybe_unused]] RetailInitialTransitionBody next_body) {
    // Retail mode transition dispatch: updates mode history if present
    if (main.mode_history) {
        RetailModeFailure mf;
        (void)main.mode_history->push(mode, mf);
    }
}

} // namespace mka::retail::initial_task_required
