#pragma once
#include "mka/movie_service.hpp"
#include "mka/movie_host_clock.hpp"
#include "mka/movie_audio_clock_anchor.hpp"
namespace mka::video {
enum class MovieServiceHostClockPolicy {elapsed_service,anchored_audio};
enum class MovieServiceHostClockError {none,invalid_owner,wrong_thread,reentry,invalid_state,
    foreign_source,unavailable,invalid_sample,baseline_changed,regression,overflow,host,service,allocation};
struct MovieServiceHostClockFailure {
    MovieServiceHostClockError kind=MovieServiceHostClockError::none;
    MovieHostClockFailure host;
    MovieServiceFailure service,cleanup;
};
struct MovieServiceHostClockSnapshot {
    bool begun=false,stopped=false,failed=false;
    MovieClockRational elapsed{0,1};
    std::int64_t service_counter=0;
};
// Borrowed service must outlive binding on this thread. Call update between
// actual service polls; no pumping, readiness or business completion is added.
class NativeMovieServiceHostClock final {
public:
    static std::unique_ptr<NativeMovieServiceHostClock> create(NativeMovieService&,
        MovieServiceRequestIdentity,MovieServiceHostClockPolicy,bool service_query_admitted,
        MovieServiceHostClockFailure&) noexcept;
    static std::unique_ptr<NativeMovieServiceHostClock> create_with_clock(NativeMovieService&,
        MovieServiceRequestIdentity,MovieServiceHostClockPolicy,bool service_query_admitted,
        std::unique_ptr<NativeMovieHostClock>,MovieServiceHostClockFailure&) noexcept;
    ~NativeMovieServiceHostClock();
    NativeMovieServiceHostClock(const NativeMovieServiceHostClock&)=delete;
    NativeMovieServiceHostClock& operator=(const NativeMovieServiceHostClock&)=delete;
    bool begin(MovieServiceHostClockFailure&) noexcept;
    bool update(const std::optional<MovieAnchoredAudioClockSample>&,MovieServiceHostClockFailure&) noexcept;
    bool update_from_output(MovieServiceHostClockFailure&) noexcept;
    std::optional<MovieServiceHostClockSnapshot> snapshot(MovieServiceHostClockFailure&) const noexcept;
private:
    NativeMovieServiceHostClock(NativeMovieService&,MovieServiceRequestIdentity,MovieServiceHostClockPolicy,bool,std::unique_ptr<NativeMovieHostClock>);
    bool access(MovieServiceHostClockFailure&) const noexcept;
    bool terminate(MovieServiceHostClockFailure&) noexcept;
    bool update_owned(const std::optional<MovieAnchoredAudioClockSample>&,MovieServiceHostClockFailure&) noexcept;
    NativeMovieService& service_;
    MovieServiceRequestIdentity identity_;
    MovieServiceHostClockPolicy policy_;
    bool admitted_,busy_=false,paused_=true;
    std::thread::id owner_=std::this_thread::get_id();
    std::unique_ptr<NativeMovieHostClock> clock_;
    MovieServiceHostClockSnapshot state_;
    std::int64_t frequency_=0,baseline_=0,expected_=0;
    __int128 projected_=0;
    std::shared_ptr<const MovieNativeAudioClockAnchor> anchor_;
    std::optional<MovieClockRational> last_audio_;
};
} // namespace mka::video
