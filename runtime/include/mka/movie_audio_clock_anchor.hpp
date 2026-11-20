#pragma once
#include "mka/audio_device.hpp"
#include "mka/movie_clock_filter.hpp"
#include "mka/movie_source_observations.hpp"
#include <thread>
#include <string_view>
namespace mka::video {
struct MovieNativeAudioClockAnchor {};
struct MovieAnchoredAudioClockSample {
    std::string source_key;
    std::shared_ptr<const MovieSourceObservationIdentity> source;
    std::shared_ptr<const MovieNativeAudioClockAnchor> anchor;
    MovieClockRational elapsed;
    bool query_admitted,active;
    std::optional<sound::AudioClockReading> output_clock;
    bool cancelled=false;
};
enum class MovieAudioClockAnchorError {none,invalid_source,invalid_status,invalid_origin,epoch,
    frequency,regression,overflow,query,allocation,wrong_thread,reentry};
struct MovieAudioClockAnchorFailure {MovieAudioClockAnchorError kind=MovieAudioClockAnchorError::none;};
// Concrete stream-origin conversion. Inputs must be actual observations from
// the exclusively owned native output; no QPC/padding-to-played substitution.
class NativeMovieAudioClockAnchor final {
public:
    static std::unique_ptr<NativeMovieAudioClockAnchor> create(std::string_view,
        std::shared_ptr<const MovieSourceObservationIdentity>,const sound::AudioQueueStatus&,
        const sound::AudioClockReading&,MovieAudioClockAnchorFailure&) noexcept;
    ~NativeMovieAudioClockAnchor();
    NativeMovieAudioClockAnchor(const NativeMovieAudioClockAnchor&)=delete;
    NativeMovieAudioClockAnchor& operator=(const NativeMovieAudioClockAnchor&)=delete;
    std::optional<MovieAnchoredAudioClockSample> observe(const sound::AudioQueueStatus&,
        const sound::AudioClockReading&,MovieAudioClockAnchorFailure&) noexcept;
    // Only after confirmed native stopped Reset: new zero origin + next epoch.
    // Returns old cancelled sample; old source/anchor/clock facts remain owned.
    std::optional<MovieAnchoredAudioClockSample> reset(const sound::AudioQueueStatus&,
        const sound::AudioClockReading&,MovieAudioClockAnchorFailure&) noexcept;
private:
    NativeMovieAudioClockAnchor(std::string,std::shared_ptr<const MovieSourceObservationIdentity>,
        std::shared_ptr<const MovieNativeAudioClockAnchor>,sound::AudioClockReading);
    std::string key_;std::shared_ptr<const MovieSourceObservationIdentity> source_;
    std::shared_ptr<const MovieNativeAudioClockAnchor> anchor_;
    sound::AudioClockReading last_;bool active_=false,busy_=false;
    std::thread::id owner_=std::this_thread::get_id();
};
} // namespace mka::video
