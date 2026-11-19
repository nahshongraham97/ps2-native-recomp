#include "mka/movie_control.hpp"

#include <cmath>
#ifdef __FAST_MATH__
#error Native movie control requires finite-value checks
#endif

namespace mka::video {
namespace {
MovieControlFailure named(MovieControlError kind, const char* operation) noexcept {
    return {kind, operation, 0};
}
MovieControlFailure service_failure(MovieControlFailure failure,
                                    const char* operation) noexcept {
    if (failure.kind == MovieControlError::none)
        return named(MovieControlError::native_service, operation);
    return failure;
}
}
const char* movie_control_error_name(MovieControlError error) noexcept {
    switch (error) {
    case MovieControlError::none: return "none";
    case MovieControlError::wrong_thread: return "wrong_thread";
    case MovieControlError::invalid_state: return "invalid_state";
    case MovieControlError::invalid_key: return "invalid_key";
    case MovieControlError::invalid_volume: return "invalid_volume";
    case MovieControlError::native_service: return "native_service";
    case MovieControlError::invalid_status: return "invalid_status";
    case MovieControlError::skip_callback: return "skip_callback";
    }
    return "unknown_movie_control_error";
}
NativeMovieController::NativeMovieController(NativeMoviePlayback& playback,
        NativeMovieControlContext& context)
    : playback_(playback), context_(context), owner_(std::this_thread::get_id()) {}

bool NativeMovieController::on_owner(MovieControlFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() == owner_) return true;
    failure = named(MovieControlError::wrong_thread, "movie control owner");
    return false;
}
MovieControlResult NativeMovieController::fail(MovieControlFailure failure) noexcept {
    terminal_ = {MovieControlProgress::error, MovieControlCompletion::none,
                 service_failure(failure, "movie playback failure"), {}};
    MovieControlFailure cleanup;
    if (!playback_.stop(cleanup))
        terminal_.cleanup_failure = service_failure(cleanup, "movie failure stop");
    state_ = MovieControlState::failed;
    return terminal_;
}
MovieControlResult NativeMovieController::finish(MovieControlCompletion completion) noexcept {
    MovieControlFailure failure;
    if (!playback_.stop(failure)) {
        terminal_ = {MovieControlProgress::error, MovieControlCompletion::none,
                     service_failure(failure, "movie completion stop"), {}};
        state_ = MovieControlState::failed;
        return terminal_;
    }
    state_ = MovieControlState::idle;
    terminal_ = {MovieControlProgress::complete, completion, {}, {}};
    return terminal_;
}
bool NativeMovieController::start(std::string_view key, std::uint64_t fade_count,
                                MovieControlFailure& failure) noexcept {
    if (!on_owner(failure)) return false;
    if (state_ == MovieControlState::failed || state_ == MovieControlState::fading) {
        failure = named(MovieControlError::invalid_state, "movie start state"); return false;
    }
    if (key.empty() || key.find('\0') != key.npos) {
        failure = named(MovieControlError::invalid_key, "movie exact catalog key"); return false;
    }
    if (!std::isfinite(context_.base_volume_)) {
        failure = named(MovieControlError::invalid_volume, "movie base volume"); return false;
    }
    // Source337380: pause before StartFname, including an existing active owner.
    if (!playback_.pause(true, failure) || !playback_.start(key, failure)) {
        failure = fail(service_failure(failure, "movie pause/start")).failure;
        return false;
    }
    fade_length_ = fade_count; fade_step_ = 0;
    state_ = MovieControlState::preparing;
    terminal_ = {};
    return true;
}
MovieControlResult NativeMovieController::poll() noexcept {
    MovieControlFailure failure;
    if (!on_owner(failure)) return {MovieControlProgress::error, MovieControlCompletion::none, failure, {}};
    if (state_ == MovieControlState::failed) return terminal_;
    if (!playback_.pump_metadata(failure)) return fail(service_failure(failure, "movie metadata pump"));
    if (!playback_.pump_streams(failure)) return fail(service_failure(failure, "movie stream pump"));
    // Source337C00 consumes the asynchronous error even in the fade state.
    if (playback_.take_error(failure)) return fail(service_failure(failure, "movie asynchronous error"));
    failure = {};
    MovieControlCompletion completion = MovieControlCompletion::none;
    if (state_ != MovieControlState::fading) {
        MoviePlaybackStatus status = MoviePlaybackStatus::error;
        if (!playback_.status(status, failure)) return fail(service_failure(failure, "movie status query"));
        switch (status) {
        case MoviePlaybackStatus::stopped: completion = MovieControlCompletion::backend_stopped; break;
        case MoviePlaybackStatus::ended: completion = MovieControlCompletion::backend_ended; break;
        case MoviePlaybackStatus::preparing: state_ = MovieControlState::preparing; break;
        case MoviePlaybackStatus::playing:
            if (state_ == MovieControlState::preparing && !playback_.pause(false, failure))
                return fail(service_failure(failure, "movie ready resume"));
            state_ = MovieControlState::playing;
            break;
        case MoviePlaybackStatus::error: return fail(service_failure(failure, "movie playback status error"));
        default: return fail(named(MovieControlError::invalid_status, "movie playback status value"));
        }
    }
    if (completion != MovieControlCompletion::none) return finish(completion);
    if (state_ == MovieControlState::fading) {
        if (fade_step_ == fade_length_) return finish(MovieControlCompletion::skipped);
        const float base = context_.base_volume_;
        if (!std::isfinite(base)) return fail(named(MovieControlError::invalid_volume, "movie fade base volume"));
        ++fade_step_;
        // Source337670: increment first, reread base, then request the exact
        // native gain calculation. Do not silently choose host FP semantics.
        if (!playback_.apply_fade_volume({base, fade_step_, fade_length_}, failure))
            return fail(service_failure(failure, "movie fade volume"));
        // The poll that writes the last fade step still returns ongoing.
    } else if (state_ == MovieControlState::playing) {
        const auto presentation = playback_.try_present(failure);
        if (presentation == MoviePresentation::error)
            return fail(service_failure(failure, "movie frame presentation"));
        if (presentation != MoviePresentation::no_frame && presentation != MoviePresentation::presented)
            return fail(named(MovieControlError::invalid_status, "movie presentation result"));
        try {
            // Source callback runs after the attempt, even when no frame exists.
            if (context_.skip_ && context_.skip_() == 1) {
                state_ = MovieControlState::fading; fade_step_ = 0;
            }
        } catch (...) { return fail(named(MovieControlError::skip_callback, "movie skip callback")); }
    }
    return {MovieControlProgress::ongoing, MovieControlCompletion::none, {}, {}};
}
bool NativeMovieController::stop(MovieControlFailure& failure) noexcept {
    if (!on_owner(failure)) return false;
    if (state_ == MovieControlState::idle || state_ == MovieControlState::fading) return true;
    if (state_ == MovieControlState::failed) { failure = terminal_.failure; return false; }
    if (state_ == MovieControlState::preparing && !playback_.pause(false, failure)) {
        failure = fail(service_failure(failure, "movie stop resume")).failure; return false;
    }
    auto result = finish(MovieControlCompletion::caller_stop);
    failure = result.failure;
    return result.progress == MovieControlProgress::complete;
}
} // namespace mka::video
