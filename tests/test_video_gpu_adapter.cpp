#include "mka/video_gpu_adapter.hpp"
#include <cstdio>
#include <stdexcept>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"video adapter check %d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace {
using namespace mka::video;
struct Resource final:VideoFrameResource {
    std::uint64_t identity()const noexcept override{return 0x1000000001ull;}
    unsigned width()const noexcept override{return 640;}unsigned height()const noexcept override{return 448;}
};
struct Fence final:VideoGpuFence {explicit Fence(NativePresentRequest r):VideoGpuFence(std::move(r)){} };
struct Backend final:VideoGpuBackend {
    bool null_source=false,null_fence=false,submit_fail=false;unsigned submits=0,retired=0;
    VideoFenceResult result{VideoFenceState::pending,VideoGpuError::none};
    VideoSourceResult acquire(const VideoSourceRequest& r)override {
        if(r.serialized_base!=2912||r.serialized_stride!=10||r.required.width!=640)return {VideoGpuError::unsupported_source,{}};
        return {VideoGpuError::none,null_source?nullptr:std::make_shared<Resource>()};
    }
    VideoGpuSubmission submit(const NativePresentRequest& r,const HostVideoLayout& l,const VideoRect&)override {
        ++submits;
        if(l.sample.width!=832||l.logical_width!=640)return {VideoGpuError::invalid_layout,{}};
        if(submit_fail)return {VideoGpuError::submission_failed,{}};
        return {VideoGpuError::none,null_fence?nullptr:std::make_shared<Fence>(r)};
    }
    VideoFenceResult poll(const std::shared_ptr<VideoGpuFence>&)override{return result;}
    void retire(std::shared_ptr<VideoGpuFence> f)noexcept override{if(f)++retired;}
};
FinalResample chain(){
    // Synthetic data/order fixture based on the proven command grammar;
    // explicitly CT32 and DTHE0, not the captured retail16S/dither pass.
    constexpr std::uint64_t words[40]={0x70000013,0,0x1000000000008012,14,
        0xd0000,0x4c,0,0x18,0x7fff00007fff0000,0x40,0x30000,0x47,0x10a0000e7,0x4e,0,0x45,
        0x6fc009fc00a,8,0xaa8028b60,6,0x61,0x14,0x16,0,0x3f80000000000000,1,
        0x3a00000039c00000,2,0,5,0x3ee040003f201800,2,0x1c003400,5,0x720000006c00,0x18,0xa005b,0x4c,0xa0000e7,0x4e};
    std::array<std::uint8_t,320> bytes{};
    for(unsigned w=0;w<40;++w)for(unsigned b=0;b<8;++b)bytes[w*8+b]=std::uint8_t(words[w]>>(b*8));
    const auto d=decode_final_resample(bytes.data(),bytes.size());if(!d)throw std::runtime_error("invalid test chain");return d.plan;
}
}
int main(){
    using namespace mka::video;
    auto producer=std::make_shared<NativeVideoProducer>();auto backend=std::make_shared<Backend>();
    VideoGpuAdapter adapter(producer,backend);const auto raw=chain();
    const VideoEffects effects(0,1,VideoOutputPrecision::rgba8);
    const auto plan=adapter.acquire(raw,640,448,effects);CHECK(plan.plan&&plan.plan->effects&&plan.error==VideoGpuError::none);
    backend->null_source=true;CHECK(adapter.acquire(raw,640,448,effects).error==VideoGpuError::backend_contract);backend->null_source=false;
    auto env=std::make_shared<VideoEnvironment>();env->variants.fill(*plan.plan);
    HostVideoLayout layout{55,{0,0,832,448},640,448,1280,900,HostVideoScale::centered_nearest_integer_or_fit};
    const auto rect=centered_video_destination(layout);CHECK(rect&&rect->x==0&&rect->y==2&&rect->width==1280&&rect->height==896);
    CHECK(producer->enqueue(0x40,env)==VideoError::none);
    auto invalid=layout;invalid.scale=HostVideoScale::unspecified;
    CHECK(adapter.on_host_frame(0,invalid).error==VideoGpuError::invalid_layout);CHECK(producer->status().pending);
    CHECK(adapter.on_host_frame(0,layout).progress==VideoAdapterProgress::submitted);
    CHECK(producer->status().completed==0&&producer->status().in_flight);
    CHECK(adapter.poll().progress==VideoAdapterProgress::waiting);CHECK(producer->status().completed==0);
    backend->result={VideoFenceState::complete,VideoGpuError::none};CHECK(adapter.poll().progress==VideoAdapterProgress::completed);
    CHECK(producer->status().completed==1&&backend->retired==1);
    CHECK(producer->enqueue(0x41,env)==VideoError::none);backend->null_fence=true;
    CHECK(adapter.on_host_frame(1,layout).error==VideoGpuError::backend_contract);CHECK(producer->status().failed);
    producer->reset();backend->null_fence=false;
    CHECK(producer->enqueue(0x40,env)==VideoError::none);invalid=layout;invalid.sample.width=640;
    CHECK(adapter.on_host_frame(0,invalid).error==VideoGpuError::invalid_layout);CHECK(producer->status().failed);
    producer->reset();CHECK(producer->enqueue(0x40,env)==VideoError::none);
    CHECK(adapter.on_host_frame(0,layout).progress==VideoAdapterProgress::submitted);
    backend->result={VideoFenceState::failed,VideoGpuError::fence_failed};CHECK(adapter.poll().error==VideoGpuError::fence_failed);
    CHECK(producer->status().failed&&backend->retired==2);
    layout.window_width=1;layout.window_height=1;CHECK(centered_video_destination(layout));
    std::puts("[video-adapter] explicit layout, source contract, pending/complete/failure and producer ordering passed");return 0;
}
