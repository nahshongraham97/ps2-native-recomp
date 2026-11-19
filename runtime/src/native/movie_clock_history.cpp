#include "mka/movie_clock_history.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
namespace mka::video {
const char* movie_clock_history_error_name(MovieClockHistoryError e) noexcept {
    switch(e) {
    case MovieClockHistoryError::none:return "none";
    case MovieClockHistoryError::invalid_window:return "invalid_window";
    case MovieClockHistoryError::storage_size:return "storage_size";
    case MovieClockHistoryError::allocation:return "allocation";
    case MovieClockHistoryError::invalid_owner:return "invalid_owner";
    case MovieClockHistoryError::overflow:return "overflow";
    } return "unknown_error";
}
std::optional<NativeMovieClockHistory> NativeMovieClockHistory::create(std::size_t n,MovieClockHistoryFailure& f) noexcept {
    f={};
    if(!n) { f.kind=MovieClockHistoryError::invalid_window; return std::nullopt; }
    NativeMovieClockHistory result;
    if(n>result.values_.max_size()) { f.kind=MovieClockHistoryError::storage_size; return std::nullopt; }
    try { result.values_.assign(n,0); }
    catch(const std::length_error&) { f.kind=MovieClockHistoryError::storage_size; return std::nullopt; }
    catch(const std::bad_alloc&) { f.kind=MovieClockHistoryError::allocation; return std::nullopt; }
    result.active_=true;
    return result;
}
NativeMovieClockHistory::NativeMovieClockHistory(NativeMovieClockHistory&& other) noexcept
    :values_(std::move(other.values_)),cursor_(other.cursor_),active_(other.active_) {
    other.active_=false; other.cursor_=0;
}
NativeMovieClockHistory& NativeMovieClockHistory::operator=(NativeMovieClockHistory&& other) noexcept {
    if(this!=&other) {
        values_=std::move(other.values_); cursor_=other.cursor_; active_=other.active_;
        other.active_=false; other.cursor_=0;
    } return *this;
}
std::span<const std::int64_t> NativeMovieClockHistory::window() const noexcept {
    if(!active_) return {}; return values_;
}
std::optional<std::int64_t> NativeMovieClockHistory::mean(MovieClockHistoryFailure& f) const noexcept {
    f={}; if(!active_) { f.kind=MovieClockHistoryError::invalid_owner; return std::nullopt; }
    // On a <=64-bit size_t host, even SIZE_MAX signed64 terms fit signed128.
    static_assert(sizeof(std::size_t)<=8);
    __int128 sum=0;
    for(auto v:values_) sum+=static_cast<__int128>(v);
    return static_cast<std::int64_t>(sum/static_cast<__int128>(values_.size()));
}
std::optional<std::int64_t> NativeMovieClockHistory::insert(std::int64_t v,MovieClockHistoryFailure& f) noexcept {
    f={}; if(!active_) { f.kind=MovieClockHistoryError::invalid_owner; return std::nullopt; }
    values_[cursor_]=v;
    if(++cursor_==values_.size()) cursor_=0;
    return mean(f);
}
std::optional<std::int64_t> NativeMovieClockHistory::recenter(std::int64_t correction,MovieClockHistoryFailure& f) noexcept {
    f={}; if(!active_) { f.kind=MovieClockHistoryError::invalid_owner; return std::nullopt; }
    for(auto v:values_) {
        const __int128 next=static_cast<__int128>(v)-correction;
        if(next<std::numeric_limits<std::int64_t>::min() || next>std::numeric_limits<std::int64_t>::max()) {
            f.kind=MovieClockHistoryError::overflow; return std::nullopt;
        }
    }
    for(auto& v:values_) v=static_cast<std::int64_t>(static_cast<__int128>(v)-correction);
    return mean(f);
}
bool NativeMovieClockHistory::clear(MovieClockHistoryFailure& f) noexcept {
    f={}; if(!active_) { f.kind=MovieClockHistoryError::invalid_owner; return false; }
    std::fill(values_.begin(),values_.end(),0); cursor_=0; return true;
}
}
