#pragma once
#include "mka/movie_frame_eligibility.hpp"
#include "mka/movie_pcm_binding.hpp"
#include "mka/movie_playback.hpp"
#include "mka/movie_service_clock.hpp"
#include <variant>

namespace mka::video {
// The parser record that supplied the rate and the selected SDK output picture
// are separate identities. Their association is a required producer contract;
// this adapter never substitutes latest metadata or frame-rate estimates.
struct MovieOrdinaryDecision {
    std::string source_key;
    MoviePictureSource selected_picture, rate_record_picture;
    std::int64_t associated_sequence_rate_code;
    MovieClockRational ordinary_clock;
    MovieTimecodeRational adjusted_target;
    MovieOrdinaryDecision(std::string key, MoviePictureSource selected,
        MoviePictureSource rate_record, std::int64_t rate,
        MovieClockRational clock, MovieTimecodeRational target)
        : source_key(std::move(key)), selected_picture(selected),
          rate_record_picture(rate_record), associated_sequence_rate_code(rate),
          ordinary_clock(clock), adjusted_target(target) {}
};

// One actual source-bound mutable service owner, shared with the context.
// This is ordinary native lifetime/identity, never a guest handle/address.
class MovieEligibilityServiceBinding {
public:
    MovieEligibilityServiceBinding(const MovieEligibilityServiceBinding&)=delete;
    MovieEligibilityServiceBinding& operator=(const MovieEligibilityServiceBinding&)=delete;
    MovieEligibilityServiceBinding(MovieEligibilityServiceBinding&&)=delete;
    MovieEligibilityServiceBinding& operator=(MovieEligibilityServiceBinding&&)=delete;
    static std::shared_ptr<MovieEligibilityServiceBinding> create(std::string source_key,
        NativeMovieServiceClock&&,MovieControlFailure&) noexcept;
    const std::string& source_key() const noexcept { return source_key_; }
    NativeMovieServiceClock* service(MovieControlFailure&) noexcept;
private:
    MovieEligibilityServiceBinding(std::string key,NativeMovieServiceClock&& clock)
        :source_key_(std::move(key)),clock_(std::move(clock)),configured_frequency_(clock_.state().frequency) {}
    const std::string source_key_;
    NativeMovieServiceClock clock_;
    const std::int64_t configured_frequency_;
    const std::thread::id owner_=std::this_thread::get_id();
};
struct MovieSourceDirectDecision {
    std::string source_key;
    MoviePictureSource selected_picture;
};
struct MovieSourceRetainedDecision {
    std::string source_key;
    MoviePictureSource selected_picture;
    MovieTimecodeRational adjusted_target;
    std::shared_ptr<MovieEligibilityServiceBinding> service;
};
using MoviePolicyDecision=std::variant<MovieOrdinaryDecision,MovieSourceDirectDecision,MovieSourceRetainedDecision>;
enum class MovieEligibilityRoute {ordinary,source_direct,source_retained};
struct MoviePolicyRouteResult {
    MovieEligibilityRoute route;
    bool eligible;
    bool operator==(const MoviePolicyRouteResult&) const = default;
};

// Required owned source-specific lifecycle/timeline context. No method has a
// default ready/end/clock/metadata policy. Implementing this interface remains
// separate from proving its retail caller, source origins and event ordering.
class MoviePlaybackPolicyContext {
public:
    virtual ~MoviePlaybackPolicyContext() = default;
    virtual bool begin(const RetailMovieRequest&, std::int32_t, MovieControlFailure&) = 0;
    virtual bool pump_metadata(MovieControlFailure&) = 0;
    virtual bool observe_source(const MovieSourceObservationBatch&,MovieControlFailure&) = 0;
    virtual bool pause(bool, MovieControlFailure&) = 0;
    virtual bool ready(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual std::optional<MoviePolicyDecision> decision(const MovieVideoFrame&,
        const MoviePlaybackSnapshot&, MovieControlFailure&) = 0;
    virtual bool video_submitted(const MovieGpuMetadata&, MovieControlFailure&) = 0;
    virtual bool audio_accepted(const MoviePlaybackAudioAcceptance&, MovieControlFailure&) = 0;
    virtual bool end_admitted(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual bool stop(MovieControlFailure&) = 0;
};

// Actual PCM and typed source/ordinary eligibility behind the stream-player policy
// seam. Explicit immutable PCM/profile and timeline providers remain mandatory.
// This is single-use, owner-thread and non-reentrant, including provider calls.
class NativeMovieKernelPolicies final : public MoviePlaybackPolicies {
public:
    static std::unique_ptr<NativeMovieKernelPolicies> create(RetailMovieRequest,
        MoviePlaybackAudioPresence, std::unique_ptr<NativeMoviePcmBinding>,
        MovieFrameEligibilityConfig, std::unique_ptr<MoviePlaybackPolicyContext>,
        MovieControlFailure&) noexcept;
    // Explicit shared service binding for source-retained decisions. The
    // original overload deliberately supplies no service for ordinary callers.
    static std::unique_ptr<NativeMovieKernelPolicies> create(RetailMovieRequest,
        MoviePlaybackAudioPresence, std::unique_ptr<NativeMoviePcmBinding>,
        MovieFrameEligibilityConfig, std::unique_ptr<MoviePlaybackPolicyContext>,
        std::shared_ptr<MovieEligibilityServiceBinding>,MovieControlFailure&) noexcept;
    ~NativeMovieKernelPolicies() override;
    bool validate_observation_source(std::shared_ptr<const MovieSourceObservationIdentity>,MovieControlFailure&) noexcept override;
    bool observe_source(const MovieSourceObservationBatch&,MovieControlFailure&) noexcept override;
    bool validate_audio_format(const std::optional<sound::AudioStreamFormat>&,
                              MovieControlFailure&) noexcept override;
    bool begin(const RetailMovieRequest&, std::int32_t, MovieControlFailure&) noexcept override;
    bool pump_metadata(MovieControlFailure&) noexcept override;
    bool pause(bool, MovieControlFailure&) noexcept override;
    bool ready(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) noexcept override;
    bool eligible(const MovieVideoFrame&, const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) noexcept override;
    bool video_submitted(const MovieGpuMetadata&, MovieControlFailure&) noexcept override;
    bool prepare_audio(const MovieAudioFrame&, std::int32_t, std::vector<float>&, MovieControlFailure&) noexcept override;
    bool audio_accepted(const MoviePlaybackAudioAcceptance&, MovieControlFailure&) noexcept override;
    bool set_gain(std::int32_t, MovieControlFailure&) noexcept override;
    bool end_admitted(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) noexcept override;
    bool stop(MovieControlFailure&) noexcept override;
    // Owned scalar observations; caller must remain on the owner thread and
    // outside callbacks, just as for the stream owner's receipt view.
    // Most recent ORDINARY result, possibly older than last_route_result().
    // Direct/retained routes never claim a fresh ordinary numeric evaluation.
    const std::optional<MovieFrameEligibilityResult>& last_decision() const noexcept { return last_; }
    const std::optional<MoviePolicyRouteResult>& last_route_result() const noexcept { return route_; }
private:
    NativeMovieKernelPolicies(RetailMovieRequest, MoviePlaybackAudioPresence,
        std::unique_ptr<NativeMoviePcmBinding>, std::unique_ptr<NativeMovieFrameEligibility>,
        std::unique_ptr<MoviePlaybackPolicyContext>,std::shared_ptr<MovieEligibilityServiceBinding>);
    template<class F> bool invoke(MovieControlFailure&, bool needs_active, F&&) noexcept;
    RetailMovieRequest request_;
    MoviePlaybackAudioPresence audio_presence_;
    std::unique_ptr<NativeMoviePcmBinding> pcm_;
    std::unique_ptr<NativeMovieFrameEligibility> eligibility_;
    std::unique_ptr<MoviePlaybackPolicyContext> context_;
    std::shared_ptr<MovieEligibilityServiceBinding> service_;
    std::shared_ptr<const MovieSourceObservationIdentity> source_identity_;
    std::uint64_t observation_sequence_=0,observed_video_end_=0;
    std::optional<std::uint64_t> observed_prefix_;
    bool observed_program_end_=false;
    const std::thread::id owner_ = std::this_thread::get_id();
    std::optional<MovieFrameEligibilityResult> last_;
    std::optional<MoviePolicyRouteResult> route_;
    std::optional<MoviePictureSource> pending_picture_;
    std::int32_t gain_ = 0;
    bool in_call_ = false, format_joined_ = false, used_ = false, active_ = false, cleanup_needed_ = false, closed_ = false;
};
} // namespace mka::video
