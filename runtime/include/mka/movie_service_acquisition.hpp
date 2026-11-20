#pragma once
#include "mka/movie_service.hpp"
namespace mka::video {
// Passive exact-key host settings. Neither media presence nor source PCM
// profile is inferred here; the existing service profile factory remains required.
// Unspecified presence is resolved from the actual owned source by Session;
// unused generic host endpoint settings do not activate video-only audio.
struct MovieServiceAcquisitionSetting {
    std::string logical_key;
    MoviePlaybackConfiguration playback;
    std::optional<sound::AudioDeviceConfig> audio;
};
enum class MovieServiceAcquisitionError {none,invalid_owner,wrong_thread,reentry,
    invalid_request,missing_setting,invalid_setting,device,session,allocation};
struct MovieServiceAcquisitionFailure {
    MovieServiceAcquisitionError kind=MovieServiceAcquisitionError::none;
    const char* operation="";
    MovieCatalogFailure catalog;
    sound::AudioDeviceFailure device;
    MovieSessionFailure session;
};
// Owns acquisition resources, not the application coordinator. The caller must
// retain STA COM on this creating thread through every acquired Session's audio
// destruction. GPU operations/destruction likewise remain on this thread.
class NativeMovieServiceAcquisition final:public std::enable_shared_from_this<NativeMovieServiceAcquisition> {
public:
    static std::shared_ptr<NativeMovieServiceAcquisition> create(NativeMovieAssetStore,
        std::filesystem::path library_directory,std::shared_ptr<NativeMovieGpuBackend>,
        NativeMovieColorTable,std::vector<MovieServiceAcquisitionSetting>,MovieServiceAcquisitionFailure&) noexcept;
    ~NativeMovieServiceAcquisition();
    NativeMovieServiceAcquisition(const NativeMovieServiceAcquisition&)=delete;
    NativeMovieServiceAcquisition& operator=(const NativeMovieServiceAcquisition&)=delete;
    MovieServiceSessionFactory session_factory(MovieServiceAcquisitionFailure&) noexcept;
    std::unique_ptr<NativeMoviePlaybackSession> acquire(const RetailMovieRequest&,
        std::int64_t language,MovieSessionPolicyFactory,NativeMovieControlContext&,
        MovieServiceAcquisitionFailure&) noexcept;
private:
    NativeMovieServiceAcquisition(NativeMovieAssetStore,std::filesystem::path,
        std::shared_ptr<NativeMovieGpuBackend>,NativeMovieColorTable,std::vector<MovieServiceAcquisitionSetting>);
    bool access(MovieServiceAcquisitionFailure&) const noexcept;
    NativeMovieAssetStore store_;
    std::filesystem::path libraries_;
    std::shared_ptr<NativeMovieGpuBackend> gpu_;
    NativeMovieColorTable color_;
    std::vector<MovieServiceAcquisitionSetting> settings_;
    std::thread::id owner_=std::this_thread::get_id();
    bool busy_=false;
};
} // namespace mka::video
