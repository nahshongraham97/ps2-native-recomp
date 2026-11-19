// Trace fixtures from retail337380/337670/337500/337C00 and original words.
// Pure native control only; no game, clock, GL/audio device or default backend.
#include "mka/movie_control.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mka::video;
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Playback final : NativeMoviePlayback {
    std::vector<std::string> trace;
    std::vector<float> volumes;
    MoviePlaybackStatus observed = MoviePlaybackStatus::preparing;
    MoviePresentation presentation = MoviePresentation::no_frame;
    const char* fail_operation = "";
    bool asynchronous_error = false, fail_cleanup = false;
    bool call(const char* operation, MovieControlFailure& failure) noexcept {
        trace.emplace_back(operation);
        failure = {};
        if (std::string_view(operation) == fail_operation ||
            (fail_cleanup && std::string_view(operation) == "stop")) {
            failure = {MovieControlError::native_service, operation, 719}; return false;
        }
        return true;
    }
    bool pause(bool value, MovieControlFailure& f) noexcept override {
        return call(value ? "pause" : "resume", f);
    }
    bool start(std::string_view key, MovieControlFailure& f) noexcept override {
        check(key == "MOVIEPS2/LOGON.SFD", "exact catalog key without prefix/search rewrite");
        return call("start", f);
    }
    bool pump_metadata(MovieControlFailure& f) noexcept override { return call("metadata", f); }
    bool pump_streams(MovieControlFailure& f) noexcept override { return call("streams", f); }
    bool take_error(MovieControlFailure& f) noexcept override {
        trace.emplace_back("error-check"); f = {};
        if (!asynchronous_error) return false;
        asynchronous_error = false;
        f = {MovieControlError::native_service, "original CRI failure", 41}; return true;
    }
    bool status(MoviePlaybackStatus& status, MovieControlFailure& f) noexcept override {
        status = observed; return call("status", f);
    }
    MoviePresentation try_present(MovieControlFailure& f) noexcept override {
        call("present-attempt", f); return presentation;
    }
    bool set_volume_scalar(float volume, MovieControlFailure& f) noexcept override {
        volumes.push_back(volume); return call("volume", f);
    }
    bool stop(MovieControlFailure& f) noexcept override { return call("stop", f); }
};
void expect(Playback& playback, std::initializer_list<const char*> wanted, const char* label) {
    std::vector<std::string> expected;
    for (auto value : wanted) expected.emplace_back(value);
    check(playback.trace == expected, label); playback.trace.clear();
}
void begin(NativeMovieController& player, Playback& playback, std::uint64_t fade = 4) {
    MovieControlFailure f;
    check(player.start("MOVIEPS2/LOGON.SFD", fade, f), "native start");
    expect(playback, {"pause", "start"}, "source start pauses before start");
}
void normal_and_skip() {
    Playback playback; NativeMovieControlContext context(1.0f);
    NativeMovieController player(playback, context); begin(player, playback);
    std::int64_t callback_value = 2;
    context.install_skip_if_present([&] { playback.trace.emplace_back("skip-query"); return callback_value; });
    check(player.poll().progress == MovieControlProgress::ongoing, "preparing is ongoing");
    expect(playback, {"metadata", "streams", "error-check", "status"}, "no presentation or skip while preparing");
    playback.observed = MoviePlaybackStatus::playing;
    check(player.poll().progress == MovieControlProgress::ongoing && player.state() == MovieControlState::playing,
          "callback2 does not mean skip");
    expect(playback, {"metadata", "streams", "error-check", "status", "resume", "present-attempt", "skip-query"},
           "resume then attempt then skip, including no frame");
    callback_value = 0x100000001ll;
    check(player.poll().progress == MovieControlProgress::ongoing && player.state() == MovieControlState::playing,
          "64-bit callback value with lowword1 does not skip");
    expect(playback, {"metadata", "streams", "error-check", "status", "present-attempt", "skip-query"},
           "exact full callback return comparison");
    // Empty input preserves the initialized shared callback across callers.
    context.install_skip_if_present({}); callback_value = 1;
    check(player.poll().progress == MovieControlProgress::ongoing && player.state() == MovieControlState::fading,
          "skip accepted only after presentation attempt");
    expect(playback, {"metadata", "streams", "error-check", "status", "present-attempt", "skip-query"},
           "no duplicate resume or early fade step");
    check(playback.volumes.empty(), "skip poll does not immediately alter volume");
    for (unsigned step = 1; step <= 4; ++step) {
        context.set_base_volume(step == 2 ? .5f : 1.f);
        check(player.poll().progress == MovieControlProgress::ongoing, "all four fade writes remain ongoing");
        expect(playback, {"metadata", "streams", "error-check", "volume"}, "fade bypasses status/render/skip");
    }
    check(playback.volumes == std::vector<float>{.75f, .25f, .25f, 0.f},
          "increment-first fade reads current base volume each poll");
    const auto end = player.poll();
    check(end.progress == MovieControlProgress::complete && end.completion == MovieControlCompletion::skipped,
          "fifth fade poll completes");
    expect(playback, {"metadata", "streams", "error-check", "stop"}, "stop happens before completion");
}
void ends_and_explicit_stop() {
    for (auto observed : {MoviePlaybackStatus::stopped, MoviePlaybackStatus::ended}) {
        Playback p; NativeMovieControlContext context(.7f); NativeMovieController c(p, context); begin(c, p);
        p.observed = observed;
        auto result = c.poll();
        check(result.progress == MovieControlProgress::complete && result.completion ==
              (observed == MoviePlaybackStatus::ended ? MovieControlCompletion::backend_ended : MovieControlCompletion::backend_stopped),
              "typed normal terminal reason");
        expect(p, {"metadata", "streams", "error-check", "status", "stop"}, "normal completion stops inside poll");
    }
    Playback p; NativeMovieControlContext context(.7f); NativeMovieController c(p, context);
    MovieControlFailure f; check(c.stop(f), "idle stop is no-op"); check(p.trace.empty(), "idle stop calls no backend");
    begin(c, p); check(c.stop(f), "preparing explicit stop");
    expect(p, {"resume", "stop"}, "preparing stop resumes first");
    begin(c, p); p.observed = MoviePlaybackStatus::playing; c.poll(); p.trace.clear();
    check(c.stop(f), "playing explicit stop"); expect(p, {"stop"}, "playing stop does not duplicate resume");
}
void zero_fade_and_errors() {
    Playback p; NativeMovieControlContext context(1.f); NativeMovieController c(p, context); begin(c, p, 0);
    context.install_skip_if_present([] { return 1; }); p.observed = MoviePlaybackStatus::playing;
    c.poll(); p.trace.clear(); MovieControlFailure f;
    check(c.stop(f) && p.trace.empty(), "source explicit stop in fade is no-op");
    check(c.poll().completion == MovieControlCompletion::skipped && p.volumes.empty(), "zero fade completes without divide or volume");
    expect(p, {"metadata", "streams", "error-check", "stop"}, "zero fade completion trace");
    begin(c, p); c.poll(); p.trace.clear(); p.asynchronous_error = true; p.fail_cleanup = true;
    const auto result = c.poll();
    check(result.progress == MovieControlProgress::error && result.completion == MovieControlCompletion::none &&
          result.failure.native_code == 41 && result.cleanup_failure.native_code == 719,
          "async CRI error and cleanup failure remain distinct, never EOF");
    expect(p, {"metadata", "streams", "error-check", "stop"}, "async error takes precedence while fading");
    check(c.poll().failure.native_code == 41 && p.trace.empty(), "failed owner never resumes hidden work");
    Playback q; NativeMovieControlContext other(1.f); NativeMovieController d(q, other); begin(d, q);
    q.observed = MoviePlaybackStatus::ended; q.fail_cleanup = true;
    check(d.poll().progress == MovieControlProgress::error, "completion cannot succeed when stop fails");
    expect(q, {"metadata", "streams", "error-check", "status", "stop"}, "failed completion stop is not retried silently");
}
void invalid_and_thread() {
    Playback p; NativeMovieControlContext context(1.f); NativeMovieController c(p, context);
    MovieControlFailure f;
    check(!c.start({}, 4, f) && f.kind == MovieControlError::invalid_key && p.trace.empty(), "missing key has no fallback");
    MovieControlFailure foreign;
    std::thread thread([&] { check(!c.start("MOVIEPS2/LOGON.SFD", 4, foreign), "foreign start fails"); }); thread.join();
    check(foreign.kind == MovieControlError::wrong_thread && p.trace.empty(), "foreign thread performs no native work");
    begin(c, p); p.observed = static_cast<MoviePlaybackStatus>(99);
    check(c.poll().failure.kind == MovieControlError::invalid_status, "unknown native state fails by name");
    expect(p, {"metadata", "streams", "error-check", "status", "stop"}, "unknown state cleanup");
    Playback q; NativeMovieControlContext other(1.f); NativeMovieController d(q, other); begin(d, q);
    other.install_skip_if_present([]() -> int { throw std::runtime_error("fixture"); });
    q.observed = MoviePlaybackStatus::playing;
    check(d.poll().failure.kind == MovieControlError::skip_callback, "throwing callback becomes named failure");
    expect(q, {"metadata", "streams", "error-check", "status", "resume", "present-attempt", "stop"}, "callback failure cleanup");
}
}
int main() try {
    normal_and_skip(); ends_and_explicit_stop(); zero_fade_and_errors(); invalid_and_thread();
    std::puts("PASS native retail movie start/poll/skip/fade/stop traces and named failures; no game/player/device binding");
    return 0;
} catch (const std::exception& e) { std::fprintf(stderr, "FAIL movie control: %s\n", e.what()); return 1; }
