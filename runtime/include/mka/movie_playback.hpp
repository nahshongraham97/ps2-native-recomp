#pragma once
#include "mka/audio_device.hpp"
#include "mka/movie_audio_clock_anchor.hpp"
#include "mka/movie_audio_progress.hpp"
#include "mka/movie_color.hpp"
#include "mka/movie_control.hpp"
#include "mka/movie_gpu.hpp"
#include "mka/movie_source.hpp"
#include <deque>
#include <span>

namespace mka::video {

// Narrow test seam; the native adapter below owns the already-open retail
// source. No implementation here searches/reopens an asset or supplies frames.
class MoviePlaybackInput {
public:
    virtual ~MoviePlaybackInput() = default;
    virtual const RetailMovieRequest& request() const noexcept = 0;
    virtual std::shared_ptr<const MovieSourceObservationIdentity> observation_identity() const noexcept = 0;
    virtual MovieReadResult next(MovieControlFailure&) = 0;
};
std::unique_ptr<MoviePlaybackInput> own_movie_playback_source(
    std::unique_ptr<NativeRetailMovieSource>, MovieControlFailure&);

enum class MovieAudioSubmitStatus { accepted, full, error };
struct MovieAudioSubmitResult {
    MovieAudioSubmitStatus status = MovieAudioSubmitStatus::error;
    std::optional<sound::AudioSubmission> submission;
};
class MoviePlaybackAudioOutput {
public:
    virtual ~MoviePlaybackAudioOutput() = default;
    virtual std::optional<sound::AudioStreamFormat> format(MovieControlFailure&) = 0;
    virtual std::optional<sound::AudioQueueStatus> queue_status(MovieControlFailure&) = 0;
    virtual std::optional<sound::AudioClockReading> clock(MovieControlFailure&) = 0;
    virtual MovieAudioSubmitResult queue(std::span<const float>, MovieControlFailure&) = 0;
    // Idempotent requested pause. Unpause with no prequeued PCM defers Start
    // until the owner calls this again after successful acceptance. No Reset.
    virtual bool pause(bool, MovieControlFailure&) = 0;
    virtual bool stop_and_reset(MovieControlFailure&) = 0;
};
// Consumes the explicit, already-open native device. No endpoint/default/COM
// selection or audio activation during construction. Calls stay on its owner.
std::unique_ptr<MoviePlaybackAudioOutput> own_movie_playback_audio(
    std::unique_ptr<sound::NativeAudioDevice>, MovieControlFailure&);

enum class MoviePlaybackAudioPresence { unspecified, absent, required };
struct MoviePlaybackConfiguration {
    MoviePlaybackAudioPresence audio = MoviePlaybackAudioPresence::unspecified;
    std::uint32_t client_sample_rate = 0, client_channels = 0, client_channel_mask = 0;
    MovieHostLayout layout;
    MovieFilter filter = MovieFilter::unspecified;
    // Explicit host queue budget, not a PS2 memory/capacity limit. A decoded
    // event can temporarily exceed the budget; further reads then backpressure.
    std::size_t queued_byte_budget = 0;
    std::optional<float> initial_base_volume;
};
struct MoviePlaybackSnapshot {
    bool decoder_eof = false, paused = true;
    std::uint64_t decoded_video_frames = 0, decoded_audio_frames = 0;
    std::size_t queued_video = 0, queued_audio = 0, outstanding_fences = 0;
    std::optional<sound::AudioQueueStatus> audio_queue;
    std::optional<sound::AudioClockReading> device_clock;
    // Decoder-relative source-frame INDEX retirement, not a game-clock origin,
    // PTS, master-clock choice or acoustic completion. Current and cancelled
    // old-epoch observations remain separate across an explicit reset.
    std::optional<MovieAudioProgressSnapshot> source_retirement;
    std::optional<MovieAudioProgressSnapshot> cancelled_source_retirement;
    std::optional<MovieAnchoredAudioClockSample> audio_time,cancelled_audio_time;
    std::optional<MovieAudioClockAnchorFailure> audio_time_failure;
};
// These remain REQUIRED, separately source-bound business policies. This
// composition supplies no default PTS/ordinal, stereo, unity, ready or ended
// policy. An interface call is not evidence that the actual retail policy was
// implemented. The same-asset metadata context must exist before begin().
class MoviePlaybackPolicies {
public:
    virtual ~MoviePlaybackPolicies() = default;
    virtual bool validate_observation_source(std::shared_ptr<const MovieSourceObservationIdentity>,
                                            MovieControlFailure&) = 0;
    // Ordered actual-input/codec observations, before the returned frame or
    // source EOF is queued. A real context must bind their source semantics.
    virtual bool observe_source(const MovieSourceObservationBatch&,MovieControlFailure&) = 0;
    // Construction-time join to the ACTUAL opened client format. Equal sample
    // counts alone cannot authenticate channel/speaker order. Absence explicit.
    virtual bool validate_audio_format(const std::optional<sound::AudioStreamFormat>&,
                                      MovieControlFailure&) = 0;
    virtual bool begin(const RetailMovieRequest&, std::int32_t initial_tenth_db, MovieControlFailure&) = 0;
    virtual bool pump_metadata(MovieControlFailure&) = 0;
    virtual bool pause(bool, MovieControlFailure&) = 0;
    virtual bool ready(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual bool eligible(const MovieVideoFrame&, const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual bool video_submitted(const MovieGpuMetadata&, MovieControlFailure&) = 0;
    // Admitted STATELESS signal transform, with immutable source/profile and
    // equal source/client rate/cardinality. May be called again on ORIGINAL
    // samples after a gain change; it must not advance resampler/filter/source
    // history or reapply gain to previously prepared PCM. Instrumentation is
    // allowed. Stateful/changed-profile routes need a separately proved binding.
    virtual bool prepare_audio(const MovieAudioFrame&, std::int32_t tenth_db,
                               std::vector<float>& client_pcm, MovieControlFailure&) = 0;
    virtual bool audio_accepted(const MoviePlaybackAudioAcceptance&, MovieControlFailure&) = 0;
    // Updates owned coefficient state for future mixing; not system/device
    // volume, rewriting submitted PCM or applying another gain to output.
    virtual bool set_gain(std::int32_t tenth_db, MovieControlFailure&) = 0;
    // Called only after EOF, empty decoded queues and completed/retired fences.
    // Device padding/clock and sink ledger still require an explicit drain rule.
    virtual bool end_admitted(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual bool stop(MovieControlFailure&) = 0;
};

// Concrete serialized source/queue/GPU/audio composition. This is a single-use
// movie source; a second request requires a newly opened native owner. Public
// operations are serialized and non-reentrant, including from policy/backend
// callbacks: nested operations fail before any service/state effect. Callbacks
// use supplied snapshots; object destruction cannot occur during an operation.
// The borrowed receipt view remains observational and follows normal span
// lifetime rules. GPU must
// outlive the session; all calls/destruction require the creating thread and
// current original native context. No window/swap/COM/clock policy is invented.
class NativeMovieStreamPlayback final : public NativeMoviePlayback {
public:
    static std::unique_ptr<NativeMovieStreamPlayback> create(
        std::unique_ptr<MoviePlaybackInput>, std::unique_ptr<MoviePlaybackPolicies>,
        std::unique_ptr<MoviePlaybackAudioOutput>, NativeMovieGpuBackend&,
        NativeMovieColorTable, MoviePlaybackConfiguration, MovieControlFailure&);
    ~NativeMovieStreamPlayback() override;
    NativeMovieStreamPlayback(const NativeMovieStreamPlayback&) = delete;
    NativeMovieStreamPlayback& operator=(const NativeMovieStreamPlayback&) = delete;
    bool pause(bool, MovieControlFailure&) noexcept override;
    bool start(std::string_view, MovieControlFailure&) noexcept override;
    bool pump_metadata(MovieControlFailure&) noexcept override;
    bool pump_streams(MovieControlFailure&) noexcept override;
    bool take_error(MovieControlFailure&) noexcept override;
    bool status(MoviePlaybackStatus&, MovieControlFailure&) noexcept override;
    MoviePresentation try_present(MovieControlFailure&) noexcept override;
    bool apply_fade_volume(const MovieFadeStep&, MovieControlFailure&) noexcept override;
    bool stop(MovieControlFailure&) noexcept override;
    bool snapshot(MoviePlaybackSnapshot&, MovieControlFailure&) noexcept;
    std::span<const MoviePlaybackAudioAcceptance> audio_acceptances() const noexcept { return accepted_; }
private:
    struct CallGuard;
    struct QueuedVideo {
        MovieVideoFrame source;
        std::shared_ptr<const MovieGpuFrame> uploaded;
    };
    struct QueuedAudio {
        MovieAudioFrame source;
        std::vector<float> client;
        std::uint64_t first_source = 0;
        std::size_t accepted_samples = 0;
        bool prepared = false;
        std::int32_t prepared_gain = 0;
    };
    NativeMovieStreamPlayback(std::unique_ptr<MoviePlaybackInput>,
        std::unique_ptr<MoviePlaybackPolicies>, std::unique_ptr<MoviePlaybackAudioOutput>,
        NativeMovieGpuBackend&, NativeMovieColorTable, MoviePlaybackConfiguration, std::int32_t);
    bool owner(MovieControlFailure&) const noexcept;
    bool fail(MovieControlFailure&, const char*) noexcept;
    bool poll_fences(MovieControlFailure&);
    bool snapshot_owned(MoviePlaybackSnapshot&, MovieControlFailure&);
    bool submit_audio(MovieControlFailure&, bool& blocked);
    std::size_t queued_bytes() const noexcept;
    std::unique_ptr<MoviePlaybackInput> input_;
    std::unique_ptr<MoviePlaybackPolicies> policies_;
    std::unique_ptr<MoviePlaybackAudioOutput> audio_;
    NativeMovieGpuBackend& gpu_;
    NativeMovieColorTable color_;
    MoviePlaybackConfiguration config_;
    const std::thread::id owner_ = std::this_thread::get_id();
    std::deque<QueuedVideo> video_;
    std::deque<QueuedAudio> pcm_;
    std::vector<std::shared_ptr<MovieGpuFence>> fences_;
    std::vector<MoviePlaybackAudioAcceptance> accepted_;
    std::optional<sound::AudioQueueStatus> next_audio_;
    std::unique_ptr<NativeMovieAudioProgress> source_progress_;
    std::optional<MovieAudioProgressSnapshot> cancelled_progress_;
    std::unique_ptr<NativeMovieAudioClockAnchor> audio_time_;
    std::optional<MovieAnchoredAudioClockSample> cancelled_audio_time_,last_audio_time_;
    std::optional<MovieAudioClockAnchorFailure> audio_time_failure_;
    std::optional<std::uint64_t> source_audio_rate_;
    std::uint64_t decoded_video_ = 0, decoded_audio_ = 0, source_audio_cursor_ = 0;
    std::int32_t gain_;
    bool used_ = false, active_ = false, paused_ = true, eof_ = false, error_pending_ = false, cleanup_needed_ = false;
    bool in_call_ = false;
    MovieControlFailure terminal_;
};
} // namespace mka::video
