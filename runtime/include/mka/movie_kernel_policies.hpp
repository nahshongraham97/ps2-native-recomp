#pragma once
#include "mka/movie_frame_eligibility.hpp"
#include "mka/movie_pcm_binding.hpp"
#include "mka/movie_playback.hpp"

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

// Required owned source-specific lifecycle/timeline context. No method has a
// default ready/end/clock/metadata policy. Implementing this interface remains
// separate from proving its retail caller, source origins and event ordering.
class MoviePlaybackPolicyContext {
public:
    virtual ~MoviePlaybackPolicyContext() = default;
    virtual bool begin(const RetailMovieRequest&, std::int32_t, MovieControlFailure&) = 0;
    virtual bool pump_metadata(MovieControlFailure&) = 0;
    virtual bool pause(bool, MovieControlFailure&) = 0;
    virtual bool ready(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual std::optional<MovieOrdinaryDecision> decision(const MovieVideoFrame&,
        const MoviePlaybackSnapshot&, MovieControlFailure&) = 0;
    virtual bool video_submitted(const MovieGpuMetadata&, MovieControlFailure&) = 0;
    virtual bool audio_accepted(const MoviePlaybackAudioAcceptance&, MovieControlFailure&) = 0;
    virtual bool end_admitted(const MoviePlaybackSnapshot&, bool&, MovieControlFailure&) = 0;
    virtual bool stop(MovieControlFailure&) = 0;
};

// Actual PCM and ordinary eligibility kernels behind the stream-player policy
// seam. Explicit immutable PCM/profile and timeline providers remain mandatory.
// This is single-use, owner-thread and non-reentrant, including provider calls.
class NativeMovieKernelPolicies final : public MoviePlaybackPolicies {
public:
    static std::unique_ptr<NativeMovieKernelPolicies> create(RetailMovieRequest,
        MoviePlaybackAudioPresence, std::unique_ptr<NativeMoviePcmBinding>,
        MovieFrameEligibilityConfig, std::unique_ptr<MoviePlaybackPolicyContext>,
        MovieControlFailure&) noexcept;
    ~NativeMovieKernelPolicies() override;
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
    const std::optional<MovieFrameEligibilityResult>& last_decision() const noexcept { return last_; }
private:
    NativeMovieKernelPolicies(RetailMovieRequest, MoviePlaybackAudioPresence,
        std::unique_ptr<NativeMoviePcmBinding>, std::unique_ptr<NativeMovieFrameEligibility>,
        std::unique_ptr<MoviePlaybackPolicyContext>);
    template<class F> bool invoke(MovieControlFailure&, bool needs_active, F&&) noexcept;
    RetailMovieRequest request_;
    MoviePlaybackAudioPresence audio_presence_;
    std::unique_ptr<NativeMoviePcmBinding> pcm_;
    std::unique_ptr<NativeMovieFrameEligibility> eligibility_;
    std::unique_ptr<MoviePlaybackPolicyContext> context_;
    const std::thread::id owner_ = std::this_thread::get_id();
    std::optional<MovieFrameEligibilityResult> last_;
    std::optional<MoviePictureSource> pending_picture_;
    std::int32_t gain_ = 0;
    bool in_call_ = false, format_joined_ = false, used_ = false, active_ = false, cleanup_needed_ = false, closed_ = false;
};
} // namespace mka::video
