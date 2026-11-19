#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>

namespace mka::video {
enum class VideoError {none,unsupported_chain,invalid_source,invalid_environment,
    unsupported_command,invalid_variant,allocation_failure,token_exhausted,
    stale_completion,presentation_failed};
struct FinalResample {
    std::array<std::uint8_t,320> raw{};
    std::uint64_t destination_frame=0,source_tex0=0,clamp=0,filter=0;
    std::uint64_t restore_xyoffset=0,restore_frame=0,restore_zbuf=0;
    std::uint32_t source_base=0,source_stride=0,source_format=0;
    std::uint32_t virtual_width=0,virtual_height=0;
    std::uint32_t clamp_x=0,clamp_y=0,clamp_width=0,clamp_height=0;
    std::uint32_t output_width=0,output_height=0;
    float u0=0,v0=0,u1=0,v1=0; // original normalized ST, including source bias
    bool destination_dither=false;
};
struct FinalResampleDecode {FinalResample plan;VideoError error=VideoError::none;
    explicit operator bool()const{return error==VideoError::none;}};
// Proven complete20-qword final-output subset only, including explicit PRIM16.
// Unknown passes/registers are rejected; this never executes or skips them.
FinalResampleDecode decode_final_resample(const std::uint8_t*,std::size_t);

class VideoFrameResource {
public:
    virtual ~VideoFrameResource()=default;
    virtual std::uint64_t identity()const noexcept=0;
    virtual std::uint32_t width()const noexcept=0;
    virtual std::uint32_t height()const noexcept=0;
};
struct NativeVideoSource {
    // Explicit adapter association; these serialized fields are compatibility
    // data, never allocation addresses or limits on native resource ownership.
    std::uint32_t serialized_base=0,serialized_stride=0,serialized_format=0;
    std::shared_ptr<const VideoFrameResource> resource;
};
struct NativePresentPlan {FinalResample resample;NativeVideoSource source;
    std::uint32_t logical_width=0,logical_height=0;};
std::optional<NativePresentPlan> bind_present_source(const FinalResample&,NativeVideoSource,
    std::uint32_t logical_width,std::uint32_t logical_height);
struct VideoEnvironment {std::array<NativePresentPlan,4> variants;};
struct NativePresentRequest {std::uint64_t completion=0;std::uint32_t buffer=0,variant=0;
    std::shared_ptr<const VideoEnvironment> environment;
    const NativePresentPlan& plan()const{return environment->variants[buffer+variant*2];}};
struct FrameResult {VideoError error=VideoError::none;std::optional<NativePresentRequest> request;};
struct ProducerStatus {std::size_t queued=0;bool pending=false,in_flight=false,failed=false;
    std::uint64_t host_frames_since_flip=0,completed=0;};
class NativeVideoProducer {
public:
    // Native ingress for the game-owned video commands40/41. No INTC, DMA,
    // CSR, bus, chip time or scanout memory is implemented here.
    VideoError enqueue(std::uint32_t command,std::shared_ptr<const VideoEnvironment>);
    void pacing(std::uint64_t minimum_host_frames,bool paused=false);
    FrameResult on_host_frame(std::uint32_t variant);
    VideoError complete(std::uint64_t completion,bool succeeded);
    void reset();
    ProducerStatus status()const;
private:
    struct Item {std::uint32_t buffer;std::shared_ptr<const VideoEnvironment> environment;};
    void pump();
    mutable std::mutex mutex_;
    std::deque<Item> queue_;
    std::optional<Item> pending_;
    std::optional<NativePresentRequest> in_flight_;
    std::uint64_t minimum_frames_=0,frames_=0,completed_=0;
    bool paused_=false,failed_=false;
};
// This data/order core does NOT establish a compiled retail function entry
// binding, GPU execution, owner-verified framing or permission to omit any
// retained resample/filter/postprocess operation.
} // namespace mka::video
