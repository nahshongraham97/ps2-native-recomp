#pragma once
#include "mka/movie_clock_filter.hpp"
#include "mka/movie_source_observations.hpp"
#include <functional>
#include <thread>
namespace mka::video {
enum class MovieHostClockError {none,unsupported_host,invalid_source,invalid_frequency,
    invalid_reader,wrong_thread,reentry,foreign_source,invalid_state,query,regression,overflow,allocation};
struct MovieHostClockFailure {MovieHostClockError kind=MovieHostClockError::none;std::int64_t native_code=0;};
enum class MovieHostClockState {not_started,running,paused,stopped};
struct MovieHostClockReading {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> identity;
    MovieClockRational elapsed;
    MovieHostClockState state;
};
// Only a deterministic test seam. Production create uses Windows QPC directly.
using MovieHostTickReader=std::function<std::optional<std::int64_t>()>;
class NativeMovieHostClock final {
public:
    static std::unique_ptr<NativeMovieHostClock> create(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,MovieHostClockFailure&) noexcept;
    static std::unique_ptr<NativeMovieHostClock> create_with_reader(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,std::int64_t frequency,
        MovieHostTickReader,MovieHostClockFailure&) noexcept;
    ~NativeMovieHostClock();
    NativeMovieHostClock(const NativeMovieHostClock&)=delete;
    NativeMovieHostClock& operator=(const NativeMovieHostClock&)=delete;
    bool begin(std::string_view,std::shared_ptr<const MovieSourceObservationIdentity>,MovieHostClockFailure&) noexcept;
    bool pause(std::string_view,std::shared_ptr<const MovieSourceObservationIdentity>,MovieHostClockFailure&) noexcept;
    bool resume(std::string_view,std::shared_ptr<const MovieSourceObservationIdentity>,MovieHostClockFailure&) noexcept;
    bool stop(std::string_view,std::shared_ptr<const MovieSourceObservationIdentity>,MovieHostClockFailure&) noexcept;
    std::optional<MovieHostClockReading> read(std::string_view,std::shared_ptr<const MovieSourceObservationIdentity>,MovieHostClockFailure&) noexcept;
private:
    struct Impl;
    explicit NativeMovieHostClock(std::unique_ptr<Impl>);
    bool access(std::string_view,const std::shared_ptr<const MovieSourceObservationIdentity>&,MovieHostClockFailure&) const noexcept;
    std::optional<std::int64_t> tick(MovieHostClockFailure&) noexcept;
    std::unique_ptr<Impl> impl_;
};
} // namespace mka::video
