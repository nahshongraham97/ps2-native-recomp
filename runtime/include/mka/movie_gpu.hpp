#pragma once
#include "mka/movie_color.hpp"
#include "mka/video_frame.hpp"

namespace mka::video {
enum class MovieGpuError {none,invalid_frame,invalid_plan,invalid_layout,unsupported_effect,
    host_capacity,unavailable,wrong_thread,stale_resource,upload_failed,present_failed,fence_failed,admission_closed,pending};
struct MovieGpuMetadata {
    MovieColorRole conversion_role=MovieColorRole::unspecified;
    MovieTimestamp source_pts,timestamp;
    std::int32_t sample_aspect_numerator=0,sample_aspect_denominator=0;
    std::int32_t color_range=0,color_space=0,color_primaries=0,color_transfer=0;
    MovieCodecTiming codec_timing;
};
class MovieGpuFrame:public VideoFrameResource {
public:
    virtual const MovieGpuMetadata& metadata()const noexcept=0;
};
struct MovieGpuUpload {MovieGpuError error=MovieGpuError::unavailable;std::shared_ptr<const MovieGpuFrame> frame;};
struct MovieRect {std::uint32_t x=0,y=0,width=0,height=0;};
enum class MovieFilter {unspecified,nearest,linear};
// Converted bytes are native RGBA8 UNORM. No GS alpha scaling, implicit
// blending, lookup-table conversion or transfer function is applied again.
enum class MovieAlpha {unspecified,replace_rgba8};
struct NativeMoviePresentPlan {
    std::shared_ptr<const MovieGpuFrame> frame;
    MovieRect sample; // exact complete frame; unsupported crops reject
    MovieFilter filter=MovieFilter::unspecified;
    MovieAlpha alpha=MovieAlpha::unspecified;
};
enum class MovieAspectMode {unspecified,source_sample_aspect,explicit_display_aspect};
enum class MovieWindowPolicy {unspecified,centered_fit};
struct MovieHostLayout {
    std::uint64_t destination=0;
    std::uint32_t window_width=0,window_height=0;
    MovieAspectMode aspect=MovieAspectMode::unspecified;
    // Mandatory positive exact ratio only for explicit_display_aspect. This
    // is supplied by the caller's verified policy, never inferred from640/PAR.
    std::uint32_t display_aspect_numerator=0,display_aspect_denominator=0;
    MovieWindowPolicy policy=MovieWindowPolicy::unspecified;
};
MovieGpuError validate_movie_upload(const MovieRgbaFrame&)noexcept;
MovieGpuError validate_movie_present(const NativeMoviePresentPlan&)noexcept;
std::optional<MovieRect> movie_destination(const NativeMoviePresentPlan&,const MovieHostLayout&)noexcept;
class MovieGpuFence {
public:
    virtual ~MovieGpuFence()=default;
    const NativeMoviePresentPlan plan;
protected:
    explicit MovieGpuFence(NativeMoviePresentPlan p):plan(std::move(p)){}
};
enum class MovieFenceState {pending,complete,failed};
struct MovieFenceResult {MovieFenceState state=MovieFenceState::failed;MovieGpuError error=MovieGpuError::fence_failed;};
struct MovieGpuSubmission {MovieGpuError error=MovieGpuError::unavailable;std::shared_ptr<MovieGpuFence> fence;};
class NativeMovieGpuBackend {
public:
    virtual ~NativeMovieGpuBackend()=default;
    // Copies client bytes into an owned native texture on this context's
    // ordered command stream. CPU frame may be destroyed on return; GPU
    // completion is established separately by the presentation fence.
    virtual MovieGpuUpload upload(const MovieRgbaFrame&)=0;
    // Submission is not display completion, swap, an A/V clock or retail
    // playback-state success. Host owns swap and source-proven scheduling.
    virtual MovieGpuSubmission present(const NativeMoviePresentPlan&,const MovieHostLayout&)=0;
    virtual MovieFenceResult poll(const std::shared_ptr<MovieGpuFence>&)=0;
    // Retire any returned fence, including on failed submission. Its internal
    // source ownership survives until actual completion or context revocation.
    virtual void retire(std::shared_ptr<MovieGpuFence>)noexcept=0;
};
} // namespace mka::video
