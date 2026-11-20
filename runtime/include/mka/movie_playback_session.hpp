#pragma once
#include "mka/movie_playback.hpp"

namespace mka::video {
enum class MovieSessionError { none, source, control, allocation, invalid_owner, wrong_thread, invalid_state, device };
struct MovieSessionFailure {
    MovieSessionError kind = MovieSessionError::none;
    const char* operation = "";
    MovieSourceFailure source;
    MovieControlFailure control, cleanup;
    sound::AudioDeviceFailure device;
};
// Request reference is borrowed only for the factory call; policies retain
// their own request values and shared source identity. Format is the actual
// already-open audio client, or explicit absence. No default policy exists.
using MovieSessionPolicyFactory = std::function<std::unique_ptr<MoviePlaybackPolicies>(
    const RetailMovieRequest&, std::shared_ptr<const MovieSourceObservationIdentity>,
    const std::optional<sound::AudioStreamFormat>&, MovieSessionFailure&)>;

// App-facing single-request owner. Context and its settings/skip mutations
// belong to this thread and must outlive the session. Initial base comes from
// this context; a supplied configuration value must match its exact float bits.
// GPU/context must remain usable on this thread through close/destruction.
// No real device activation, source-clock or lifecycle policy is selected here.
class NativeMoviePlaybackSession final {
public:
    static std::unique_ptr<NativeMoviePlaybackSession> open(
        const NativeMovieAssetStore&, const std::filesystem::path& library_directory,
        std::int64_t id, MovieViewSelection, std::int64_t game_language_code,
        MovieSessionPolicyFactory, std::unique_ptr<sound::NativeAudioDevice>,
        std::shared_ptr<NativeMovieGpuBackend>, NativeMovieColorTable,
        MoviePlaybackConfiguration, NativeMovieControlContext&, MovieSessionFailure&) noexcept;
    // Opens the associated source BEFORE deciding whether any endpoint is
    // needed. Explicit host config is required only for an actual audio source.
    // Never selects a default endpoint or silently changes explicit fields.
    static std::unique_ptr<NativeMoviePlaybackSession> open_with_native_audio(
        const NativeMovieAssetStore&, const std::filesystem::path& library_directory,
        std::int64_t id, MovieViewSelection, std::int64_t game_language_code,
        MovieSessionPolicyFactory, std::optional<sound::AudioDeviceConfig>,
        std::shared_ptr<NativeMovieGpuBackend>, NativeMovieColorTable,
        MoviePlaybackConfiguration, NativeMovieControlContext&, MovieSessionFailure&) noexcept;
    static std::unique_ptr<NativeMoviePlaybackSession> create_from_ports(
        std::unique_ptr<MoviePlaybackInput>, std::unique_ptr<MoviePlaybackPolicies>,
        std::unique_ptr<MoviePlaybackAudioOutput>, std::shared_ptr<NativeMovieGpuBackend>,
        NativeMovieColorTable, MoviePlaybackConfiguration,
        NativeMovieControlContext&, MovieSessionFailure&) noexcept;
    ~NativeMoviePlaybackSession();
    NativeMoviePlaybackSession(const NativeMoviePlaybackSession&) = delete;
    NativeMoviePlaybackSession& operator=(const NativeMoviePlaybackSession&) = delete;
    NativeMoviePlaybackSession(NativeMoviePlaybackSession&&) = delete;
    NativeMoviePlaybackSession& operator=(NativeMoviePlaybackSession&&) = delete;

    bool start(std::uint64_t fade_poll_count, std::function<std::int64_t()> skip_if_present,
               MovieSessionFailure&) noexcept;
    MovieControlResult poll() noexcept;
    // Retail controller stop: while fading it intentionally leaves fade active.
    bool stop(MovieSessionFailure&) noexcept;
    bool snapshot(MoviePlaybackSnapshot&, MovieSessionFailure&) noexcept;
    const RetailMovieRequest* request(MovieSessionFailure&) const noexcept;
    // Resource teardown, distinct from retail stop. Attempts stream cleanup
    // even after a controller failure; closes permanently even if cleanup fails.
    bool close(MovieSessionFailure&) noexcept;
private:
    static std::unique_ptr<NativeMoviePlaybackSession> assemble_source(
        std::unique_ptr<NativeRetailMovieSource>, MovieSessionPolicyFactory,
        std::unique_ptr<sound::NativeAudioDevice>, std::shared_ptr<NativeMovieGpuBackend>,
        NativeMovieColorTable, MoviePlaybackConfiguration, NativeMovieControlContext&,
        bool fill_actual_client, MovieSessionFailure&);
    struct CallGuard;
    NativeMoviePlaybackSession(std::shared_ptr<NativeMovieGpuBackend>, RetailMovieRequest,
        std::unique_ptr<NativeMovieStreamPlayback>, NativeMovieControlContext&);
    bool access(MovieSessionFailure&, bool allow_closed = false) const noexcept;
    bool close_owned(MovieSessionFailure&) noexcept;
    std::shared_ptr<NativeMovieGpuBackend> gpu_;
    RetailMovieRequest request_;
    std::unique_ptr<NativeMovieStreamPlayback> playback_;
    std::unique_ptr<NativeMovieController> controller_;
    NativeMovieControlContext& context_;
    const std::thread::id owner_ = std::this_thread::get_id();
    bool in_call_ = false, started_ = false, closed_ = false;
    std::optional<MovieControlResult> terminal_;
};
} // namespace mka::video
