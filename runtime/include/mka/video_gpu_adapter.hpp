#pragma once
#include "mka/video_producer.hpp"

namespace mka::video {
enum class VideoGpuError {none,invalid_layout,invalid_plan,unsupported_effect,
    unsupported_source,wrong_thread,stale_resource,allocation_failure,
    submission_failed,fence_failed,backend_contract,producer_failed};
enum class HostVideoScale {unspecified,centered_nearest_integer_or_fit,centered_nearest_fit};
struct VideoRect {std::uint32_t x=0,y=0,width=0,height=0;};
struct HostVideoLayout {
    std::uint64_t destination=0; // opaque native destination identity, not an HWND
    VideoRect sample; // full final-pass output, never an inferred content crop
    std::uint32_t logical_width=0,logical_height=0;
    std::uint32_t window_width=0,window_height=0;
    HostVideoScale scale=HostVideoScale::unspecified;
};
std::optional<VideoRect> centered_video_destination(const HostVideoLayout&);
struct VideoSourceRequest {
    std::uint32_t serialized_base=0,serialized_stride=0,serialized_format=0;
    VideoRect required; // exact clamp footprint; virtual unreferenced pixels are not required
    std::uint32_t logical_width=0,logical_height=0;
};
struct VideoSourceResult {VideoGpuError error=VideoGpuError::none;
    std::shared_ptr<const VideoFrameResource> resource;};
class VideoGpuFence {
public:
    virtual ~VideoGpuFence()=default;
    // A fence pins the immutable environment/source even if the adapter is
    // destroyed. Backends retire it on their owning render thread/session.
    const NativePresentRequest request;
protected:
    explicit VideoGpuFence(NativePresentRequest r):request(std::move(r)){}
};
struct VideoGpuSubmission {VideoGpuError error=VideoGpuError::none;std::shared_ptr<VideoGpuFence> fence;};
enum class VideoFenceState {pending,complete,failed};
struct VideoFenceResult {VideoFenceState state=VideoFenceState::failed;VideoGpuError error=VideoGpuError::fence_failed;};
class VideoGpuBackend {
public:
    virtual ~VideoGpuBackend()=default;
    // Must acquire actual owned GPU pixels on the render thread. No CPU
    // readback, inferred resource alias, default source or fake allocation.
    virtual VideoSourceResult acquire(const VideoSourceRequest&)=0;
    // Preserve raw resample ST/filter/clamp/effects; reject unsupported effects.
    // The full sample rect and logical display extent are separate contracts.
    virtual VideoGpuSubmission submit(const NativePresentRequest&,const HostVideoLayout&,const VideoRect& destination)=0;
    virtual VideoFenceResult poll(const std::shared_ptr<VideoGpuFence>&)=0;
    // Transfers retirement ownership; may defer deletion until the actual GPU
    // fence completes or the owning context is revoked. Never signals success.
    virtual void retire(std::shared_ptr<VideoGpuFence>)noexcept=0;
};
struct VideoPlanResult {VideoGpuError error=VideoGpuError::none;std::optional<NativePresentPlan> plan;};
enum class VideoAdapterProgress {idle,waiting,submitted,completed,failed};
struct VideoAdapterResult {VideoAdapterProgress progress=VideoAdapterProgress::idle;
    VideoGpuError error=VideoGpuError::none;VideoError producer_error=VideoError::none;};
class VideoGpuAdapter {
public:
    VideoGpuAdapter(std::shared_ptr<NativeVideoProducer>,std::shared_ptr<VideoGpuBackend>);
    ~VideoGpuAdapter();
    VideoGpuAdapter(const VideoGpuAdapter&)=delete;
    VideoGpuAdapter& operator=(const VideoGpuAdapter&)=delete;
    VideoPlanResult acquire(const FinalResample&,std::uint32_t logical_width,std::uint32_t logical_height,const VideoEffects&);
    VideoAdapterResult on_host_frame(std::uint32_t variant,const HostVideoLayout&);
    VideoAdapterResult poll();
private:
    VideoAdapterResult fail(VideoGpuError,VideoError=VideoError::none);
    std::shared_ptr<NativeVideoProducer> producer_;
    std::shared_ptr<VideoGpuBackend> backend_;
    std::shared_ptr<VideoGpuFence> fence_;
    std::mutex mutex_;
};
// This adapter supplies no default backend. The GL implementation is acquired
// explicitly from native_gl::video_backend; compiled retail ingress remains
// separate work, as do unsupported destination effects and owner framing.
} // namespace mka::video
