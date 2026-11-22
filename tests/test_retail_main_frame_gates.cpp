#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>

extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }
using namespace mka::retail;
namespace {
void check(bool value, const char* why) {
    if (!value) throw std::runtime_error(why);
}
using Gate = std::optional<bool>(*)(const NativeRetailMainState&, StartupFailure&);
constexpr std::array<Gate, 3> gates{
    main_binding::retail_frame_processing_inhibited,
    main_binding::retail_ordinary_counter_inhibited,
    main_binding::retail_render_enabled};
// Explicit narrow world identity fixture; never a production camera/renderer.
struct WorldFixture final : NativeRetailRenderWorld {
    explicit WorldFixture(const std::shared_ptr<NativeRetailGameSession>& session)
        : NativeRetailRenderWorld(session) {}
};
void expect(const NativeRetailMainState& main, Gate gate, bool expected, const char* why) {
    StartupFailure failure;
    const auto result = gate(main, failure);
    check(result && *result == expected && failure.kind == StartupError::none, why);
}
}
int main(int argc, char** argv) try {
    check(argc == 2, "requires disposable existing empty content directory");
    mka::content::ContentVolumeFailure vf;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), vf);
    check(bool(volume), "native content root");
    NativeRetailStartupState startup(std::move(*volume), {});
    StartupFailure failure;
    check(initialize_native_retail_game_session(startup, failure), "actual Session initialization");
    const auto session = startup.game_session;
    mka::input::ControllerInput input;
    NativeRetailMainState main(startup, input);
    for (const auto gate : gates) expect(main, gate, false, "actual zero-born gate state");
    RetailGameSessionFailure sf;
    const auto controls = session->controls(sf);
    RetailGameControlsFailure cf;
    check(controls->store_selection_flags_from_source(0xff, cf), "distinct selection word source writer");
    check(controls->store_analog_input_inhibited_from_source(true, cf), "neighboring byte1 bit6 writer");
    expect(main, gates[0], false, "Main byte1 bit5 must not alias selection byte or bit6");
    expect(main, gates[1], false, "ordinary gate must not alias controls flags");

    auto* cameras = session->render_cameras(sf);
    RetailCameraWorldFailure camera_failure;
    check(cameras->store_initial_count_from_127260(camera_failure), "actual count prefix");
    check(cameras->detach_cameras_from_181a30(camera_failure), "actual suspended flag with null world");
    expect(main, gates[2], false, "suspended true does not replace null world test");
    auto world = std::make_shared<WorldFixture>(session);
    check(cameras->publish_world_after_186404(world, camera_failure), "fixture supplies completed world producer only");
    expect(main, gates[2], true, "Main renders for nonnull current world even with suspension true");
    check(cameras->publish_world_after_186404({}, camera_failure), "actual current world clear boundary");
    expect(main, gates[2], false, "live world clear is observed without cached gate");
    check(cameras->publish_world_after_186404(world, camera_failure), "republish real owned world identity");

    bool foreign_refused = true;
    std::thread worker([&] {
        for (const auto gate : gates) {
            StartupFailure f;
            foreign_refused = foreign_refused && !gate(main, f) && f.kind == StartupError::game_session;
        }
    });
    worker.join();
    check(foreign_refused, "foreign thread rejected before borrowed state");
    startup.game_session.reset();
    for (const auto gate : gates) check(!gate(main, failure) && failure.kind == StartupError::game_session, "missing actual Session refuses");
    startup.game_session = session;
    main.lifetime->retire();
    for (const auto gate : gates) check(!gate(main, failure) && failure.kind == StartupError::game_session, "retired Main refuses");
    NativeRetailMainState next(startup, input);
    expect(next, gates[2], true, "current world persists across Main recreation in actual Session");
    expect(next, gates[0], false, "actual control owner persists without guessed bit5 producer");

    // An actual already-implemented producer leaves a required followup pending.
    // Ordinary-counter lookup must propagate that native domain failure.
    check(controls->store_selection_flags_from_source(0, cf), "source live-pair selector");
    auto* tasks = session->task_domain(sf);
    RetailTaskDomainFailure tf;
    const auto update = tasks->begin_live_pair_from_28e2b0(0, 1, tf);
    check(update && update->required_followup == RetailTaskLiveFollowup::source_2451f0, "actual pending source followup");
    check(!gates[1](next, failure) && failure.game.task_domain.kind == RetailTaskDomainError::pending_source_followup, "task domain error propagates through Main gate");
    std::puts("PASS actual Main frame/ordinary/render gates: live owners, null/nonnull world, suspension distinction, neighbor bits, domain failure, thread/lifetime; nonzero bit5/root-wait producers remain unimplemented");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL %s\n", error.what());
    return 1;
}
