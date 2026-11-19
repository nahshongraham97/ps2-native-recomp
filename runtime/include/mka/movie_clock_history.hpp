#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace mka::video {
enum class MovieClockHistoryError { none, invalid_window, storage_size, allocation, invalid_owner, overflow };
struct MovieClockHistoryFailure { MovieClockHistoryError kind=MovieClockHistoryError::none; };
const char* movie_clock_history_error_name(MovieClockHistoryError) noexcept;
// Ordinary residual history only. No outer cached means, anchors or clock policy.
class NativeMovieClockHistory {
public:
    static std::optional<NativeMovieClockHistory> create(std::size_t,MovieClockHistoryFailure&) noexcept;
    NativeMovieClockHistory(const NativeMovieClockHistory&)=delete;
    NativeMovieClockHistory& operator=(const NativeMovieClockHistory&)=delete;
    NativeMovieClockHistory(NativeMovieClockHistory&&) noexcept;
    NativeMovieClockHistory& operator=(NativeMovieClockHistory&&) noexcept;
    std::optional<NativeMovieClockHistory> clone(MovieClockHistoryFailure&) const noexcept;
    bool valid() const noexcept { return active_; }
    std::optional<std::int64_t> insert(std::int64_t,MovieClockHistoryFailure&) noexcept;
    std::optional<std::int64_t> recenter(std::int64_t,MovieClockHistoryFailure&) noexcept;
    std::optional<std::int64_t> mean(MovieClockHistoryFailure&) const noexcept;
    bool clear(MovieClockHistoryFailure&) noexcept;
    // Views are valid only while this owner remains valid and unmoved.
    std::span<const std::int64_t> window() const noexcept;
    std::size_t cursor() const noexcept { return active_ ? cursor_ : 0; }
private:
    NativeMovieClockHistory()=default;
    std::vector<std::int64_t> values_;
    std::size_t cursor_=0;
    bool active_=false;
};
}
