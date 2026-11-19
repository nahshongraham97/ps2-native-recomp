#pragma once
#include "mka/movie_clock_filter.hpp"
#include <cstdint>
#include <optional>
namespace mka::video {
enum class MovieServiceClockError { none,invalid_owner,invalid_frequency,invalid_rational,unsupported_mode,unsupported_rate,overflow };
struct MovieServiceClockFailure { MovieServiceClockError kind=MovieServiceClockError::none; };
const char* movie_service_clock_error_name(MovieServiceClockError) noexcept;
enum class MovieServiceSourceMode { mode0,mode1,mode2 };
struct MovieServiceClockConfig { std::int64_t frequency,step;bool operator==(const MovieServiceClockConfig&) const = default; };
std::optional<MovieServiceClockConfig> movie_service_source_config(MovieServiceSourceMode,MovieServiceClockFailure&) noexcept;
struct MovieServiceClockState {
    std::int64_t frequency=1,step=0,counter=0;
    std::optional<std::int64_t> retained;
    bool operator==(const MovieServiceClockState&) const = default;
};
struct MovieServiceMaintenanceGates { bool state_admits=false,first_inhibit_clear=false,second_inhibit_clear=false,retained_admits=false; };
struct MovieServiceMaintenance { bool counter_advanced=false,retained_advanced=false; };
// Successful read with no value means query unavailable, never a numeric clock.
struct MovieServiceClockRead { std::optional<MovieClockRational> value; };
// Successful projection with no value is source code0's missing-rate result.
struct MovieServiceRateProjection { std::optional<std::int64_t> value; };
class NativeMovieServiceClock {
public:
    static std::optional<NativeMovieServiceClock> create(MovieServiceClockConfig,MovieServiceClockFailure&) noexcept;
    NativeMovieServiceClock(const NativeMovieServiceClock&)=delete;
    NativeMovieServiceClock& operator=(const NativeMovieServiceClock&)=delete;
    NativeMovieServiceClock(NativeMovieServiceClock&&) noexcept;
    NativeMovieServiceClock& operator=(NativeMovieServiceClock&&) noexcept;
    bool valid() const noexcept { return valid_; }
    const MovieServiceClockState& state() const noexcept { return state_; }
    std::optional<MovieServiceMaintenance> maintenance(MovieServiceMaintenanceGates,MovieServiceClockFailure&) noexcept;
    std::optional<MovieServiceClockRead> read(bool query_admitted,MovieServiceClockFailure&) const noexcept;
    bool set_step(std::int64_t,MovieServiceClockFailure&) noexcept;
    bool assign_retained(std::int64_t,MovieServiceClockFailure&) noexcept;
    bool invalidate_retained(MovieServiceClockFailure&) noexcept;
    bool increment(MovieClockRational,MovieServiceClockFailure&) noexcept;
    bool increment_rate(std::int64_t source_code,MovieServiceClockFailure&) noexcept;
    std::optional<MovieServiceRateProjection> project_rate(std::int64_t source_code,MovieServiceClockFailure&) const noexcept;
    std::optional<bool> eligible(MovieClockRational target,MovieServiceClockFailure&) noexcept;
    std::optional<bool> retained_less_than(std::int64_t threshold,MovieServiceClockFailure&) const noexcept;
private:
    explicit NativeMovieServiceClock(MovieServiceClockConfig c) noexcept:state_{c.frequency,c.step,0,std::nullopt},valid_(true) {}
    MovieServiceClockState state_;
    bool valid_=false;
};
}
