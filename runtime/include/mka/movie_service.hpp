#pragma once
#include "mka/movie_native_context.hpp"
#include "mka/movie_output_position.hpp"
#include "mka/movie_frame_work.hpp"
#include "mka/movie_playback_session.hpp"
namespace mka::video {
struct MovieServicePolicyConfiguration {
    MoviePlaybackAudioPresence audio;
    std::optional<MoviePcmBindingConfiguration> pcm;
    MovieFrameEligibilityConfig eligibility;
    MovieServiceClockConfig service_clock;
    MovieClockFilterConfig filter;
    MovieNativeTargetRoute target_route;
    MovieNativeCompletion completion;
    bool video_enabled,audio_enabled,preserve_queued_temporal;
    std::int64_t target_offset;
    std::uint64_t audio_source_origin,audio_reset_epoch,audio_source_rate;
    MovieOutputPositionProfile position_profile;
    MovieServicePolicyConfiguration(MoviePlaybackAudioPresence a,std::optional<MoviePcmBindingConfiguration> p,
        MovieFrameEligibilityConfig e,MovieServiceClockConfig clock,MovieClockFilterConfig f,
        MovieNativeTargetRoute target,MovieNativeCompletion end,bool video,bool audio_enabled_value,
        bool temporal,std::int64_t offset,std::uint64_t origin,std::uint64_t epoch,std::uint64_t hz,
        MovieOutputPositionProfile position)
        :audio(a),pcm(std::move(p)),eligibility(e),service_clock(clock),filter(f),target_route(target),
         completion(end),video_enabled(video),audio_enabled(audio_enabled_value),preserve_queued_temporal(temporal),
         target_offset(offset),audio_source_origin(origin),audio_reset_epoch(epoch),audio_source_rate(hz),position_profile(position){}
};
// Passive, explicit verified source/profile data. No policy implementation or
// fallback is returned; the service builds concrete native components itself.
using MovieServiceProfileFactory=std::function<std::optional<MovieServicePolicyConfiguration>(
    const RetailMovieRequest&,std::shared_ptr<const MovieSourceObservationIdentity>,
    const std::optional<sound::AudioStreamFormat>&,MovieSessionFailure&)>;
// Acquisition only. Must invoke the supplied concrete policy factory once.
// Production uses NativeMoviePlaybackSession::open; deterministic fixtures may
// use its existing create_from_ports seam. No guest/raw-path fallback is here.
using MovieServiceSessionFactory=std::function<std::unique_ptr<NativeMoviePlaybackSession>(
    const RetailMovieRequest&,std::int64_t game_language_code,MovieSessionPolicyFactory,
    NativeMovieControlContext&,MovieSessionFailure&)>;
enum class MovieServiceError {none,wrong_thread,invalid_state,invalid_request,foreign_request,
    missing_factory,profile,session,context,position,allocation,overflow,source_feed,native_work};
struct MovieServiceFailure {
    MovieServiceError kind=MovieServiceError::none;
    const char* operation="";
    MovieSessionFailure session,cleanup;
    MovieControlFailure control;
    MovieOutputPositionFailure position;
    MovieAdmissionFeedFailure feed;
    MovieControlFailure observation;
    MovieFrameWorkFailure native_work;
};
struct MovieServiceRequestIdentity {
    std::uint64_t generation=0;
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> source;
};
enum class MovieServiceActivity {idle,starting,preparing,work,stopping,closed,complete,error};
struct MovieServiceSnapshot {
    MovieServiceActivity activity=MovieServiceActivity::idle;
    std::uint64_t generation=0,polls=0;
    bool active=false;
    float base_volume=0;
    std::optional<RetailMovieRequest> request;
    std::optional<MovieNativeContextSnapshot> context;
    std::optional<MovieOutputPositionSnapshot> position;
    std::optional<MovieControlResult> terminal;
    // No pumping; actual owned Session/Stream observation. Retained after close
    // refers to the last pre-teardown query, not a current device observation.
    std::optional<MoviePlaybackSnapshot> playback;
    bool playback_retained=false;
    std::optional<MovieControlFailure> playback_snapshot_failure;
    std::optional<MovieFrameWorkSnapshot> native_work;
};
struct MovieServicePort;
class NativeMovieService final {
public:
    static std::unique_ptr<NativeMovieService> create(float explicit_base_volume,
        MovieServiceProfileFactory,MovieServiceSessionFactory,MovieServiceFailure&) noexcept;
    ~NativeMovieService();
    NativeMovieService(const NativeMovieService&)=delete;
    NativeMovieService& operator=(const NativeMovieService&)=delete;
    // App-owned setting: next construction and source controller fade rereads.
    // It does not retroactively rewrite submitted/current PCM.
    bool set_base_volume(float,MovieServiceFailure&) noexcept;
    bool install_skip_if_present(std::function<std::int64_t()>,MovieServiceFailure&) noexcept;
    bool start(std::int64_t retail_id,MovieViewSelection,std::int64_t game_language_code,
        std::uint64_t source_fade_poll_count,MovieServiceFailure&) noexcept;
    MovieControlResult poll(MovieServiceFailure&) noexcept;
    // Source retail stop may remain pending/no-op while fading; poll observes
    // actual completion. close is separate unconditional resource cancellation.
    bool stop(MovieServiceFailure&) noexcept;
    bool close(MovieServiceFailure&) noexcept;
    std::optional<MovieServiceRequestIdentity> request_identity(MovieServiceFailure&) const noexcept;
    std::optional<MovieServiceSnapshot> snapshot(MovieServiceFailure&) const noexcept;
    // Backend coordinator only; the retail caller does not parse MPEG.
    // Operations consume the SAME feed automatically supplied by actual Stream
    // observations. Need-more/blocked candidates do not imply admission.
    std::optional<MovieAdmissionCandidate> parse_candidate(const MovieServiceRequestIdentity&,MovieServiceFailure&) noexcept;
    std::optional<MoviePictureBodyExtent> picture_body_extent(const MovieServiceRequestIdentity&,MovieServiceFailure&) noexcept;
    std::optional<MovieHeaderBoundaryProposal> boundary_proposal(const MovieServiceRequestIdentity&,MovieServiceFailure&) noexcept;
    bool skip_source_bytes(const MovieServiceRequestIdentity&,std::uint64_t actual_bytes,MovieServiceFailure&) noexcept;
    std::optional<MovieAdmissionFeedSnapshot> source_feed_snapshot(const MovieServiceRequestIdentity&,MovieServiceFailure&) const noexcept;
    bool admit_header(const MovieServiceRequestIdentity&,const MovieNativeHeaderAdmission&,MovieServiceFailure&) noexcept;
    std::optional<MovieOutputPositionUpdate> complete_work(const MovieServiceRequestIdentity&,
        const MovieOutputPositionWork&,std::optional<std::uint64_t> timing_admission_id,
        const std::optional<MoviePictureSource>& accepted_selected,MovieServiceFailure&) noexcept;
    std::optional<MovieOutputPositionUpdate> flush_work(const MovieServiceRequestIdentity&,
        const MovieOutputPositionWork&,std::optional<std::uint64_t> timing_admission_id,
        const std::optional<MoviePictureSource>& accepted_selected,MovieServiceFailure&) noexcept;
    bool advance_position_origin(const MovieServiceRequestIdentity&,std::uint64_t actual_position,MovieServiceFailure&) noexcept;
    bool maintenance(const MovieServiceRequestIdentity&,MovieServiceMaintenanceGates,MovieServiceFailure&) noexcept;
    bool increment_service(const MovieServiceRequestIdentity&,MovieClockRational,MovieServiceFailure&) noexcept;
    bool increment_service_ticks(const MovieServiceRequestIdentity&,std::int64_t expected_counter,__int128 nonnegative_ticks,MovieServiceFailure&) noexcept;
    bool begin_native_work(const MovieServiceRequestIdentity&,std::uint64_t outer_admission_id,
        std::uint64_t timing_admission_id,MovieServiceFailure&) noexcept;
    std::optional<MovieFrameWorkResult> step_native_work(const MovieServiceRequestIdentity&,MovieServiceFailure&) noexcept;
    std::optional<MovieFrameWorkResult> flush_native_work(const MovieServiceRequestIdentity&,MovieServiceFailure&) noexcept;
    bool sample_service(const MovieServiceRequestIdentity&,bool query_admitted,MovieServiceFailure&) noexcept;
    bool sample_audio(const MovieServiceRequestIdentity&,MovieClockRational,MovieClockRational,
        bool query_admitted,bool active,MovieServiceFailure&) noexcept;
    bool select_direct_control(const MovieServiceRequestIdentity&,MovieServiceFailure&) noexcept;
    bool command(const MovieServiceRequestIdentity&,MovieNativeCommand,MovieServiceFailure&) noexcept;
private:
    NativeMovieService(float,MovieServiceProfileFactory,MovieServiceSessionFactory);
    bool access(MovieServiceFailure&) const noexcept;
    bool event_access(const MovieServiceRequestIdentity&,MovieServiceFailure&) const noexcept;
    bool close_owned(MovieServiceFailure&) noexcept;
    void terminal_failure(MovieServiceFailure&) noexcept;
    std::optional<MovieOutputPositionUpdate> work_owned(const MovieOutputPositionWork&,
        std::optional<std::uint64_t>,const std::optional<MoviePictureSource>&,bool,MovieServiceFailure&) noexcept;
    NativeMovieControlContext settings_;
    MovieServiceProfileFactory profile_factory_;
    MovieServiceSessionFactory session_factory_;
    std::unique_ptr<NativeMoviePlaybackSession> session_;
    std::shared_ptr<MovieServicePort> port_;
    MovieServiceSnapshot state_;
    const std::thread::id owner_=std::this_thread::get_id();
    mutable bool in_call_=false;
};
} // namespace mka::video
