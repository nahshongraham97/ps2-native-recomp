#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <thread>
#include <utility>

namespace mka::video {

enum class MovieControlError {
    none, wrong_thread, invalid_state, invalid_key, invalid_volume,
    unsupported_fp_environment, native_service, invalid_status, skip_callback
};
struct MovieControlFailure {
    MovieControlError kind = MovieControlError::none;
    // The label belongs to static storage. No borrowed decoder error strings.
    const char* operation = "";
    std::int64_t native_code = 0;
};
enum class MoviePlaybackStatus { stopped, preparing, playing, ended, error };
enum class MoviePresentation { no_frame, presented, error };

// Native player contract for the lifted retail control sequence. Implementors
// own native decoding, completed audio progress, video resources and callbacks.
// Decoder EOF alone must never produce ended. No guest pointer/register API.
// This interface has no default-success implementation or emulation fallback.
class NativeMoviePlayback {
public:
    virtual ~NativeMoviePlayback() = default;
    virtual bool pause(bool, MovieControlFailure&) noexcept = 0;
    virtual bool start(std::string_view exact_catalog_key, MovieControlFailure&) noexcept = 0;
    virtual bool pump_metadata(MovieControlFailure&) noexcept = 0;
    virtual bool pump_streams(MovieControlFailure&) noexcept = 0;
    // Consumes a real asynchronous failure, including while fading.
    virtual bool take_error(MovieControlFailure&) noexcept = 0;
    virtual bool status(MoviePlaybackStatus&, MovieControlFailure&) noexcept = 0;
    // A borrowed decoder frame may be acknowledged only after native ownership
    // has been acquired. Presented is submission, not swap/GPU/audio completion.
    virtual MoviePresentation try_present(MovieControlFailure&) noexcept = 0;
    // Explicit source scalar, before the source3371F0 tenth-dB conversion.
    // Implementors must preserve that conversion; this is not linear PCM gain.
    virtual bool set_volume_scalar(float, MovieControlFailure&) noexcept = 0;
    virtual bool stop(MovieControlFailure&) noexcept = 0;
};

class NativeMovieControlContext {
public:
    explicit NativeMovieControlContext(float current_base_volume)
        : base_volume_(current_base_volume) {}
    void set_base_volume(float value) noexcept { base_volume_ = value; }
    // Retail336410 only replaces the shared callback for a non-null input.
    void install_skip_if_present(std::function<std::int64_t()> callback) {
        if (callback) skip_ = std::move(callback);
    }
private:
    friend class NativeMovieController;
    float base_volume_;
    std::function<std::int64_t()> skip_;
};

enum class MovieControlState { idle, preparing, playing, fading, failed };
enum class MovieControlProgress { ongoing, complete, error };
enum class MovieControlCompletion { none, backend_stopped, backend_ended, skipped, caller_stop };
struct MovieControlResult {
    MovieControlProgress progress = MovieControlProgress::error;
    MovieControlCompletion completion = MovieControlCompletion::none;
    MovieControlFailure failure, cleanup_failure;
};

// Retail337380/337670/337500 control, with native objects and explicit pumps.
// Both references must outlive this controller. Calls/context mutation belong
// to the creating thread. The native factory must apply the source-proven
// initial volume before start; this component begins at source337380. It does
// not substitute unity volume. The backend's owner explicitly closes resources;
// this controller never destroys or moves an audio/GL device across threads.
// It does not select a clock, cadence, layout, backend or default fade length.
class NativeMovieController {
public:
    NativeMovieController(NativeMoviePlayback&, NativeMovieControlContext&);
    NativeMovieController(const NativeMovieController&) = delete;
    NativeMovieController& operator=(const NativeMovieController&) = delete;
    bool start(std::string_view exact_catalog_key, std::uint64_t fade_poll_count,
               MovieControlFailure&) noexcept;
    MovieControlResult poll() noexcept;
    bool stop(MovieControlFailure&) noexcept;
    MovieControlState state() const noexcept { return state_; }
private:
    bool on_owner(MovieControlFailure&) const noexcept;
    MovieControlResult fail(MovieControlFailure) noexcept;
    MovieControlResult finish(MovieControlCompletion) noexcept;
    NativeMoviePlayback& playback_;
    NativeMovieControlContext& context_;
    const std::thread::id owner_;
    MovieControlState state_ = MovieControlState::idle;
    std::uint64_t fade_length_ = 0, fade_step_ = 0;
    MovieControlResult terminal_;
};
const char* movie_control_error_name(MovieControlError) noexcept;
} // namespace mka::video
