#pragma once
#include "mka/movie_kernel_policies.hpp"
#include "mka/movie_admission_feed.hpp"
#include <map>
namespace mka::video {
enum class MovieNativeTargetRoute {descriptor,published_position};
enum class MovieNativeDescriptorRoute {gop,pes_position};
enum class MovieNativeCompletion {all,audio,video,either};
enum class MovieNativeCommand {owner_start,video_start,audio_start,video_finish,audio_finish,owner_stop};
enum class MovieNativeContextError {none,invalid_config,invalid_owner,foreign_source,invalid_order,
    missing_admission,missing_baseline,missing_rate,missing_position,missing_clock,invalid_rational,
    invalid_receipt,duplicate_output,overflow,allocation,host_capacity,timecode,filter,service};
struct MovieNativeContextConfiguration {
    RetailMovieRequest request;
    // Null explicitly requests deferred binding by the actual source factory.
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    std::shared_ptr<MovieEligibilityServiceBinding> service;
    MovieClockFilterConfig filter;
    MovieNativeTargetRoute target_route;
    MovieNativeCompletion completion;
    bool video_enabled,audio_enabled,preserve_queued_temporal;
    std::int64_t target_offset;
    std::uint64_t audio_source_origin,audio_reset_epoch,audio_source_rate;
    MovieNativeContextConfiguration(RetailMovieRequest r,std::shared_ptr<const MovieSourceObservationIdentity> id,
        std::shared_ptr<MovieEligibilityServiceBinding> clock,MovieClockFilterConfig f,
        MovieNativeTargetRoute target,MovieNativeCompletion end,bool video,bool audio,bool temporal,
        std::int64_t offset,std::uint64_t origin,std::uint64_t epoch,std::uint64_t hz)
        :request(std::move(r)),identity(std::move(id)),service(std::move(clock)),filter(f),target_route(target),
         completion(end),video_enabled(video),audio_enabled(audio),preserve_queued_temporal(temporal),
         target_offset(offset),audio_source_origin(origin),audio_reset_epoch(epoch),audio_source_rate(hz){}

};
struct MovieNativeHeaderAdmission {
    std::uint64_t admission_id,admitted_header_es_byte;
    std::shared_ptr<const MovieAdmissionCurrent> current;
    std::optional<MovieNativeDescriptorRoute> descriptor_route;
    bool clear_temporal,latch_eligibility_rate,latch_first_baseline;
    std::shared_ptr<const MovieOuterHeaderBatch> parsed_batch;
};
struct MovieNativeOutputPublication {
    std::uint64_t work_admission_id,timing_admission_id;
    MoviePictureSource selected_picture;
    std::optional<std::uint64_t> published_position;
};
struct MovieNativeContextSnapshot {
    bool bound=false,begun=false,paused=true,closed=false;
    bool owner_started=false,video_started=false,audio_started=false;
    bool video_finished=false,audio_finished=false,stop_acknowledged=false,program_end_observed=false;
    std::uint64_t observation_sequence=0,admissions=0,outputs_pending=0,submitted=0;
    std::uint64_t accepted_audio_frames=0,next_source_audio_frame=0,next_client_frame=0;
    std::shared_ptr<const MovieCodecInputReceipt> last_input_receipt;
    std::optional<MovieParserRateRecord> eligibility_rate;
    std::optional<MovieTimecodeRational> first_baseline,queued_baseline;
    std::uint64_t position_origin=0;
    std::optional<MovieClockRational> cached_clock,retained_audio_clock;
    MovieServiceClockState service;
};
// Concrete source-owned business state. Ingress is native data/command input,
// not callbacks into another provider. Raw observations are never admission.
class NativeMoviePlaybackContext final : public MoviePlaybackPolicyContext {
public:
    NativeMoviePlaybackContext(const NativeMoviePlaybackContext&)=delete;
    NativeMoviePlaybackContext& operator=(const NativeMoviePlaybackContext&)=delete;
    NativeMoviePlaybackContext(NativeMoviePlaybackContext&&)=delete;
    NativeMoviePlaybackContext& operator=(NativeMoviePlaybackContext&&)=delete;
    static std::unique_ptr<NativeMoviePlaybackContext> create(MovieNativeContextConfiguration,
        MovieControlFailure&) noexcept;
    bool bind_source(std::shared_ptr<const MovieSourceObservationIdentity>,MovieControlFailure&) noexcept override;
    bool admit_header(const MovieNativeHeaderAdmission&,MovieControlFailure&) noexcept;
    bool validate_work(const MovieAdmissionCurrent&,MovieControlFailure&) const noexcept;
    bool publish_output(const MovieNativeOutputPublication&,MovieControlFailure&) noexcept;
    bool advance_position_origin(std::uint64_t actual_published_position,MovieControlFailure&) noexcept;
    bool maintenance(MovieServiceMaintenanceGates,MovieControlFailure&) noexcept;
    bool increment_service(MovieClockRational,MovieControlFailure&) noexcept;
    bool increment_service_ticks(std::int64_t expected_counter,__int128 nonnegative_ticks,MovieControlFailure&) noexcept;
    bool sample_service(bool query_admitted,MovieControlFailure&) noexcept;
    // query_admitted and active are actual source control inputs; inactive
    // retains the prior pair, unavailable explicitly selects retained route.
    bool sample_audio(MovieClockRational source_audio,MovieClockRational source_timer,
        bool query_admitted,bool active,MovieControlFailure&) noexcept;
    bool select_direct_control(MovieControlFailure&) noexcept;
    bool command(MovieNativeCommand,MovieControlFailure&) noexcept;
    std::optional<MovieNativeContextSnapshot> snapshot(MovieControlFailure&) const noexcept;
    bool begin(const RetailMovieRequest&,std::int32_t,MovieControlFailure&) noexcept override;
    bool pump_metadata(MovieControlFailure&) noexcept override;
    bool observe_source(const MovieSourceObservationBatch&,MovieControlFailure&) noexcept override;
    bool pause(bool,MovieControlFailure&) noexcept override;
    bool ready(const MoviePlaybackSnapshot&,bool&,MovieControlFailure&) noexcept override;
    std::optional<MoviePolicyDecision> decision(const MovieVideoFrame&,const MoviePlaybackSnapshot&,MovieControlFailure&) noexcept override;
    bool video_submitted(const MovieGpuMetadata&,MovieControlFailure&) noexcept override;
    bool audio_accepted(const MoviePlaybackAudioAcceptance&,MovieControlFailure&) noexcept override;
    bool end_admitted(const MoviePlaybackSnapshot&,bool&,MovieControlFailure&) noexcept override;
    bool stop(MovieControlFailure&) noexcept override;
private:
    struct Admission {MovieNativeHeaderAdmission input;std::optional<MovieTimecodeDescriptor> descriptor;std::optional<MovieTimecodeRational> time;std::uint8_t work_rate;};
    struct Output {MoviePictureSource selected;MovieTimecodeRational target;};
    NativeMoviePlaybackContext(MovieNativeContextConfiguration,NativeMovieClockFilter&&) noexcept;
    bool enter(MovieControlFailure&,bool active=true) const noexcept;
    bool fail(MovieControlFailure&,MovieNativeContextError,const char*,std::int64_t=0) const noexcept;
    std::optional<MovieHeaderUnit> checked_unit(const std::shared_ptr<const MovieCurrentHeaderUnit>&,
        std::uint64_t admitted,std::uint32_t code,MovieControlFailure&) const noexcept;
    enum class ClockRoute {missing,ordinary,direct,retained};
    MovieNativeContextConfiguration config_;
    NativeMovieClockFilter filter_;
    MovieNativeContextSnapshot state_;
    std::map<std::uint64_t,Admission> admissions_;
    std::map<std::uint64_t,Output> outputs_;
    struct Decoded {MoviePictureSource source;bool submitted=false;};
    std::map<std::uint64_t,Decoded> decoded_;
    std::map<std::uint64_t,MovieVideoPrefixObserved> prefixes_;
    std::optional<std::uint64_t> last_prefix_;
    std::uint64_t observed_es_end_=0;
    std::vector<std::uint8_t> source_bytes_;
    ClockRoute route_=ClockRoute::missing;
    std::optional<MoviePictureSource> pending_decision_;
    mutable bool in_call_=false;
    const std::thread::id owner_=std::this_thread::get_id();
};
} // namespace mka::video
