#pragma once
#include "mka/movie_clock_history.hpp"
#include "mka/movie_clock_math.hpp"
#include <cstdint>
#include <optional>
#include <span>
namespace mka::video {
struct MovieClockRational { std::int64_t numerator=0,denominator=1; bool operator==(const MovieClockRational&) const = default; };
enum class MovieClockFilterMode { mode0, mode1 };
struct MovieClockFilterConfig {
    bool enabled=true,paused=false;
    MovieClockFilterMode mode=MovieClockFilterMode::mode1;
    std::size_t history_size=10;
    MovieClockRational quantum{16683,1000000},large_error{200000,1000000},first_offset{0,1000000},restart_offset{0,1000000};
    bool operator==(const MovieClockFilterConfig&) const = default;
    static MovieClockFilterConfig source_defaults() noexcept { return {}; }
};
enum class MovieClockFilterError { none,invalid_owner,invalid_config,invalid_rational,unsupported_mode,allocation,overflow,history,math };
struct MovieClockFilterFailure {
    MovieClockFilterError kind=MovieClockFilterError::none;
    MovieClockHistoryFailure history{};
    MovieClockMathFailure math{};
};
const char* movie_clock_filter_error_name(MovieClockFilterError) noexcept;
struct MovieClockFilterAnchor { std::int64_t timer=0,audio=0; bool operator==(const MovieClockFilterAnchor&) const = default; };
struct MovieClockFilterStatistics {
    std::uint64_t resets=0,inactive_reanchors=0,large_resets=0,positive_adjustments=0,negative_adjustments=0;
    std::int64_t inactive_min=0,inactive_max=0,active_min=0,active_max=0;
    bool operator==(const MovieClockFilterStatistics&) const = default;
};
struct MovieClockFilterState {
    bool active=false;
    std::optional<MovieClockFilterAnchor> anchor;
    std::int64_t audio_floor=0,mean_before=0,mean_after=0;
    MovieClockRational previous_audio{},previous_timer{},previous_output{};
    MovieClockFilterStatistics statistics{};
    bool operator==(const MovieClockFilterState&) const = default;
};
struct MovieClockFilterObservations {
    bool bypass=false,first_anchor=false,restart_anchor=false,inactive_reanchor=false,large_reset=false,inserted=false,corrected=false,output_clamped=false;
    std::optional<std::int64_t> correction;
};
struct MovieClockFilterUpdate { MovieClockRational clamped_audio,output;MovieClockFilterObservations observations; };
class NativeMovieClockFilter {
public:
    static std::optional<NativeMovieClockFilter> create(const MovieClockFilterConfig&,MovieClockFilterFailure&) noexcept;
    NativeMovieClockFilter(const NativeMovieClockFilter&)=delete;
    NativeMovieClockFilter& operator=(const NativeMovieClockFilter&)=delete;
    NativeMovieClockFilter(NativeMovieClockFilter&&) noexcept;
    NativeMovieClockFilter& operator=(NativeMovieClockFilter&&) noexcept;
    bool valid() const noexcept { return history_.valid(); }
    const MovieClockFilterConfig& config() const noexcept { return config_; }
    const MovieClockFilterState& state() const noexcept { return state_; }
    std::span<const std::int64_t> history() const noexcept { return history_.window(); }
    std::size_t history_cursor() const noexcept { return history_.cursor(); }
    std::optional<MovieClockFilterUpdate> update(MovieClockRational audio,MovieClockRational timer,MovieClockFilterFailure&) noexcept;
    bool set_enabled(bool,MovieClockFilterFailure&) noexcept;
    bool set_paused(bool,MovieClockFilterFailure&) noexcept;
    bool set_mode(MovieClockFilterMode,MovieClockFilterFailure&) noexcept;
    bool clear_history(MovieClockFilterFailure&) noexcept;
    // Returns true for valid mode1 no-op; changed reports the source branch.
    bool add_previous_output(MovieClockRational,bool& changed,MovieClockFilterFailure&) noexcept;
private:
    NativeMovieClockFilter(const MovieClockFilterConfig& c,NativeMovieClockHistory&& h) noexcept;
    MovieClockFilterConfig config_;
    MovieClockFilterState state_;
    NativeMovieClockHistory history_;
};
}
