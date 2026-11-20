#pragma once
#include "mka/movie_native_context.hpp"
#include "mka/movie_descriptor_rewrite.hpp"
#include "mka/movie_timestamp_association.hpp"
#include "mka/movie_frame_work.hpp"
#include "mka/movie_host_clock.hpp"
#include <map>
namespace mka::video {
struct MovieBackendDriverSnapshot {
    bool begun=false,worker_stopped=false,zero_history_established=false;
    std::uint32_t admission_mask=0xc0;
    std::uint64_t batches=0,admissions=0,processed=0,published=0;
    std::optional<std::uint64_t> previous_published_gop;
};
// Service-owned orchestration of native owners, not a guest scheduler/device.
// The clock is Windows QPC in production; deterministic clocks are test ports.
// Source/config mutations outside the documented normal frame route fail by
// name. Decoder algorithms and their content verification remain independent.
class NativeMovieBackendDriver final {
public:
    static std::unique_ptr<NativeMovieBackendDriver> create(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,NativeMovieAdmissionFeed&,
        NativeMovieFrameWorkCoordinator&,NativeMovieOutputPosition&,NativeMoviePlaybackContext&,
        bool video_enabled,bool audio_enabled,std::unique_ptr<NativeMovieHostClock>,MovieControlFailure&) noexcept;
    ~NativeMovieBackendDriver();
    NativeMovieBackendDriver(const NativeMovieBackendDriver&)=delete;
    NativeMovieBackendDriver& operator=(const NativeMovieBackendDriver&)=delete;
    // Called only after actual Session/Stream start succeeds.
    bool begin_after_start(MovieControlFailure&) noexcept;
    bool pause(bool,MovieControlFailure&) noexcept;
    bool pump(const MoviePlaybackBackendObservation&,MoviePlaybackBackendResult&,MovieControlFailure&) noexcept;
    bool video_submitted(const MovieGpuMetadata&,MovieControlFailure&) noexcept;
    const MovieBackendDriverSnapshot& state() const noexcept{return state_;}
private:
    NativeMovieBackendDriver(std::string,std::shared_ptr<const MovieSourceObservationIdentity>,
        NativeMovieAdmissionFeed&,NativeMovieFrameWorkCoordinator&,NativeMovieOutputPosition&,
        NativeMoviePlaybackContext&,bool,bool,std::unique_ptr<NativeMovieHostClock>,
        std::unique_ptr<NativeMovieTimestampAssociation>);
    bool access(MovieControlFailure&) const noexcept;
    bool admit(const std::shared_ptr<const MovieOuterHeaderBatch>&,MovieControlFailure&);
    bool publish(const MovieFrameWorkResult&,MovieControlFailure&);
    bool update_clock(const MoviePlaybackSnapshot&,MovieControlFailure&);
    bool finish(const MoviePlaybackBackendObservation&,MoviePlaybackBackendResult&,MovieControlFailure&);
    std::string key_;
    std::shared_ptr<const MovieSourceObservationIdentity> identity_;
    NativeMovieAdmissionFeed& feed_;
    NativeMovieFrameWorkCoordinator& work_;
    NativeMovieOutputPosition& position_;
    NativeMoviePlaybackContext& context_;
    const bool video_,audio_;
    std::unique_ptr<NativeMovieHostClock> clock_;
    std::unique_ptr<NativeMovieTimestampAssociation> timestamps_;
    NativeMovieDescriptorRewrite rewrite_;
    std::map<std::uint64_t,MoviePictureSource> pending_;
    MovieBackendDriverSnapshot state_;
    std::int64_t frequency_=0,baseline_=0,expected_=0;
    __int128 projected_=0;
    std::shared_ptr<const MovieNativeAudioClockAnchor> audio_anchor_;
    std::optional<MovieClockRational> last_audio_;
    bool paused_=true,busy_=false;
    const std::thread::id owner_=std::this_thread::get_id();
};
} // namespace mka::video
