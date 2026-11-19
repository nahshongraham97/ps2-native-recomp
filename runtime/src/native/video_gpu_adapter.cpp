#include "mka/video_gpu_adapter.hpp"
#include <algorithm>
#include <stdexcept>

namespace mka::video {
std::optional<VideoRect> centered_video_destination(const HostVideoLayout& l){
    if(!l.logical_width||!l.logical_height||!l.window_width||!l.window_height||
       (l.scale!=HostVideoScale::centered_nearest_integer_or_fit&&l.scale!=HostVideoScale::centered_nearest_fit))return std::nullopt;
    std::uint32_t w=0,h=0;
    if(l.scale==HostVideoScale::centered_nearest_integer_or_fit&&l.window_width>=l.logical_width&&l.window_height>=l.logical_height){
        const auto scale=std::min(l.window_width/l.logical_width,l.window_height/l.logical_height);
        w=l.logical_width*scale;h=l.logical_height*scale;
    }else if(std::uint64_t(l.window_width)*l.logical_height<=std::uint64_t(l.window_height)*l.logical_width){
        w=l.window_width;h=std::max<std::uint64_t>(1,(std::uint64_t(l.logical_height)*w+l.logical_width/2)/l.logical_width);
    }else{
        h=l.window_height;w=std::max<std::uint64_t>(1,(std::uint64_t(l.logical_width)*h+l.logical_height/2)/l.logical_height);
    }
    if(w>l.window_width||h>l.window_height)return std::nullopt;
    return VideoRect{(l.window_width-w)/2,(l.window_height-h)/2,w,h};
}
VideoGpuAdapter::VideoGpuAdapter(std::shared_ptr<NativeVideoProducer> p,std::shared_ptr<VideoGpuBackend> b):producer_(std::move(p)),backend_(std::move(b)){
    if(!producer_||!backend_)throw std::invalid_argument("native video adapter requires an owned producer and actual backend");
}
VideoGpuAdapter::~VideoGpuAdapter(){
    if(fence_){producer_->complete(fence_->request.completion,false);backend_->retire(std::move(fence_));}
}
VideoPlanResult VideoGpuAdapter::acquire(const FinalResample& input,unsigned w,unsigned h,const VideoEffects& effects){
    const auto decoded=decode_final_resample(input.raw.data(),input.raw.size());
    if(!decoded||!w||!h)return {VideoGpuError::invalid_plan,{}};
    const auto& p=decoded.plan;
    std::lock_guard<std::mutex> lock(mutex_);
    auto source=backend_->acquire({p.source_base,p.source_stride,p.source_format,{p.clamp_x,p.clamp_y,p.clamp_width,p.clamp_height},w,h});
    if(source.error!=VideoGpuError::none)return {source.error,{}};
    if(!source.resource)return {VideoGpuError::backend_contract,{}};
    auto bound=bind_present_source(p,{p.source_base,p.source_stride,p.source_format,std::move(source.resource)},w,h);
    if(!bound)return {VideoGpuError::backend_contract,{}};
    bound->effects=effects;
    return {VideoGpuError::none,std::move(bound)};
}
VideoAdapterResult VideoGpuAdapter::fail(VideoGpuError error,VideoError producer_error){
    if(fence_){producer_->complete(fence_->request.completion,false);backend_->retire(std::move(fence_));}
    return {VideoAdapterProgress::failed,error,producer_error};
}
VideoAdapterResult VideoGpuAdapter::on_host_frame(unsigned variant,const HostVideoLayout& layout){
    const auto destination=centered_video_destination(layout);
    if(!destination)return {VideoAdapterProgress::failed,VideoGpuError::invalid_layout};
    std::lock_guard<std::mutex> lock(mutex_);
    const auto next=producer_->on_host_frame(variant);
    if(next.error!=VideoError::none)return fail(VideoGpuError::producer_failed,next.error);
    if(!next.request)return {fence_?VideoAdapterProgress::waiting:VideoAdapterProgress::idle};
    const auto& request=*next.request;
    if(fence_){producer_->complete(request.completion,false);return fail(VideoGpuError::backend_contract);}
    const auto& plan=request.plan();
    if(layout.sample.x||layout.sample.y||layout.sample.width!=plan.resample.output_width||layout.sample.height!=plan.resample.output_height||
       layout.logical_width!=plan.logical_width||layout.logical_height!=plan.logical_height){
        producer_->complete(request.completion,false);return {VideoAdapterProgress::failed,VideoGpuError::invalid_layout};
    }
    auto submission=backend_->submit(request,layout,*destination);
    if(submission.error!=VideoGpuError::none){
        producer_->complete(request.completion,false);
        if(submission.fence)backend_->retire(std::move(submission.fence));
        return {VideoAdapterProgress::failed,submission.error};
    }
    if(!submission.fence || submission.fence->request.completion!=request.completion || submission.fence->request.environment!=request.environment){
        producer_->complete(request.completion,false);
        if(submission.fence)backend_->retire(std::move(submission.fence));
        return {VideoAdapterProgress::failed,VideoGpuError::backend_contract};
    }
    fence_=std::move(submission.fence);return {VideoAdapterProgress::submitted};
}
VideoAdapterResult VideoGpuAdapter::poll(){
    std::lock_guard<std::mutex> lock(mutex_);if(!fence_)return {};
    const auto result=backend_->poll(fence_);
    if(result.error!=VideoGpuError::none || result.state==VideoFenceState::failed)
        return fail(result.error==VideoGpuError::none?VideoGpuError::backend_contract:result.error);
    if(result.state==VideoFenceState::pending)return {VideoAdapterProgress::waiting};
    if(result.state!=VideoFenceState::complete)return fail(VideoGpuError::backend_contract);
    const auto completed=producer_->complete(fence_->request.completion,true);
    backend_->retire(std::move(fence_));
    if(completed!=VideoError::none)return {VideoAdapterProgress::failed,VideoGpuError::producer_failed,completed};
    return {VideoAdapterProgress::completed};
}
} // namespace mka::video
