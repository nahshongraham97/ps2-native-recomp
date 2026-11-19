#pragma once
#include "mka/movie_playback.hpp"
#include <type_traits>
#include <utility>
namespace mka::video {
enum class MovieAudioProgressError { none, allocation, invalid_owner, invalid_config, unsupported_rate, foreign_source, source_rate_mismatch, invalid_receipt, invalid_status, epoch_mismatch, noncontiguous, overflow, regression };
struct MovieAudioProgressFailure { MovieAudioProgressError kind=MovieAudioProgressError::none; const char* operation=""; };
struct MovieAudioProgressConfig {
    std::string source_identity;
    std::uint64_t source_origin,source_rate,client_rate,epoch;
    MovieAudioProgressConfig(std::string identity,std::uint64_t origin,std::uint64_t source_hz,std::uint64_t client_hz,std::uint64_t reset_epoch)
        :source_identity(std::move(identity)),source_origin(origin),source_rate(source_hz),client_rate(client_hz),epoch(reset_epoch){}
};
static_assert(!std::is_default_constructible_v<MovieAudioProgressConfig>);
struct MovieAudioProgressSnapshot {
    std::string source_identity;
    std::uint64_t epoch=0,submitted=0,retired=0,pending=0,source_boundary=0,source_rate=0;
    bool paused=true,cancelled=false;
};
// Equal-rate, equal-cardinality client-buffer retirement only. No device clock,
// resampling, acoustic completion or implicit source origin.
class NativeMovieAudioProgress {
public:
    static std::unique_ptr<NativeMovieAudioProgress> create(MovieAudioProgressConfig,MovieAudioProgressFailure&);
    NativeMovieAudioProgress(NativeMovieAudioProgress&&) noexcept;
    NativeMovieAudioProgress& operator=(NativeMovieAudioProgress&&) noexcept;
    bool accept(std::string_view source_identity,std::uint64_t actual_source_rate,const MoviePlaybackAudioAcceptance&,MovieAudioProgressFailure&) noexcept;
    std::optional<MovieAudioProgressSnapshot> observe(const sound::AudioQueueStatus&,MovieAudioProgressFailure&);
    std::optional<MovieAudioProgressSnapshot> snapshot(MovieAudioProgressFailure&) const;
    bool pause(bool,MovieAudioProgressFailure&) noexcept;
    // Caller supplies a confirmed successful device Reset configuration. Old
    // pending data is cancelled at the last observed retirement, never consumed.
    std::optional<MovieAudioProgressSnapshot> reset(MovieAudioProgressConfig,const sound::AudioQueueStatus&,MovieAudioProgressFailure&);
private:
    explicit NativeMovieAudioProgress(MovieAudioProgressConfig);
    bool valid_=true,paused_=true;
    MovieAudioProgressConfig config_;
    std::uint64_t submitted_=0,retired_=0;
};
}
