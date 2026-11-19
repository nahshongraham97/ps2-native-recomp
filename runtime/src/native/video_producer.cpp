#include "mka/video_producer.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::video {
namespace {
std::atomic<std::uint64_t> next_completion{0};
bool valid(const NativePresentPlan& p){
    const auto& d=p.resample;const auto& s=p.source;
    const auto decoded=decode_final_resample(d.raw.data(),d.raw.size());if(!decoded)return false;
    const auto& original=decoded.plan;
    if(d.destination_frame!=original.destination_frame || d.source_tex0!=original.source_tex0 || d.clamp!=original.clamp || d.filter!=original.filter ||
       d.restore_xyoffset!=original.restore_xyoffset || d.restore_frame!=original.restore_frame || d.restore_zbuf!=original.restore_zbuf ||
       d.source_base!=original.source_base || d.source_stride!=original.source_stride || d.source_format!=original.source_format ||
       d.virtual_width!=original.virtual_width || d.virtual_height!=original.virtual_height || d.clamp_x!=original.clamp_x || d.clamp_y!=original.clamp_y ||
       d.clamp_width!=original.clamp_width || d.clamp_height!=original.clamp_height || d.output_width!=original.output_width || d.output_height!=original.output_height ||
       d.u0!=original.u0 || d.v0!=original.v0 || d.u1!=original.u1 || d.v1!=original.v1 || d.destination_dither!=original.destination_dither)return false;
    return s.resource&&p.logical_width&&p.logical_height&&
        s.serialized_base==d.source_base&&s.serialized_stride==d.source_stride&&s.serialized_format==d.source_format&&
        std::uint64_t(d.clamp_x)+d.clamp_width<=s.resource->width()&&
        std::uint64_t(d.clamp_y)+d.clamp_height<=s.resource->height()&&
        p.logical_width<=s.resource->width()&&p.logical_height<=s.resource->height()&&
        d.clamp_width&&d.clamp_height&&d.output_width&&d.output_height;
}
std::optional<std::uint64_t> token(){
    auto old=next_completion.load(std::memory_order_relaxed);
    do{if(old==std::numeric_limits<std::uint64_t>::max())return std::nullopt;}
    while(!next_completion.compare_exchange_weak(old,old+1,std::memory_order_relaxed));
    return old+1;
}
}
FinalResampleDecode decode_final_resample(const std::uint8_t* bytes,std::size_t size){
    FinalResampleDecode out;
    auto fail=[&](){out.error=VideoError::unsupported_chain;return out;};
    if(!bytes||size!=320)return fail();
    const auto u64=[&](unsigned at){std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(bytes[at+i])<<(i*8);return v;};
    if(u64(0)!=0x70000013 || u64(8)!=0 || u64(16)!=0x1000000000008012ull || u64(24)!=14)return fail();
    static constexpr unsigned registers[18]={0x4c,0x18,0x40,0x47,0x4e,0x45,8,6,0x14,0,1,2,5,2,5,0x18,0x4c,0x4e};
    std::uint64_t data[18]{};
    for(unsigned i=0;i<18;++i){if(u64(40+i*16)!=registers[i])return fail();data[i]=u64(32+i*16);}
    // Explicit source replacement/linear sprite subset from original001B7710.
    // Keep dither/destination format as metadata; this core does not perform
    // platform quantization or turn an unsupported effect into a no-op.
    if(data[1]!=0 || data[2]!=0x7fff00007fff0000ull || data[3]!=0x30000 ||
       !(data[4]&(1ull<<32)) || data[5]>1 || (data[6]&15)!=10 ||
       data[8]!=0x61 || data[9]!=0x16 || data[10]!=0x3f80000000000000ull || data[12]!=0)return fail();
    auto& d=out.plan;
    d.destination_frame=data[0];d.source_tex0=data[7];d.clamp=data[6];d.filter=data[8];d.destination_dither=data[5]!=0;
    d.restore_xyoffset=data[15];d.restore_frame=data[16];d.restore_zbuf=data[17];
    d.source_base=data[7]&16383;d.source_stride=(data[7]>>14)&63;d.source_format=(data[7]>>20)&63;
    if(!d.source_stride || d.source_format!=0 || ((data[7]>>34)&1) || ((data[7]>>35)&3)!=1)return fail();
    d.virtual_width=1u<<((data[7]>>26)&15);d.virtual_height=1u<<((data[7]>>30)&15);
    d.clamp_x=(data[6]>>4)&1023;d.clamp_y=(data[6]>>24)&1023;
    const unsigned maxx=(data[6]>>14)&1023,maxy=(data[6]>>34)&1023;
    if(maxx<d.clamp_x||maxy<d.clamp_y||maxx>=d.virtual_width||maxy>=d.virtual_height)return fail();
    d.clamp_width=maxx-d.clamp_x+1;d.clamp_height=maxy-d.clamp_y+1;
    if((data[14]>>32)!=0 || (data[14]&0xf000f)!=0)return fail();
    d.output_width=(data[14]&65535)/16;d.output_height=((data[14]>>16)&65535)/16;
    const auto scalar=[](std::uint32_t bits){float value;std::memcpy(&value,&bits,4);return value;};
    d.u0=scalar(data[11]);d.v0=scalar(data[11]>>32);d.u1=scalar(data[13]);d.v1=scalar(data[13]>>32);
    if(!d.output_width||!d.output_height||!std::isfinite(d.u0)||!std::isfinite(d.v0)||!std::isfinite(d.u1)||!std::isfinite(d.v1)||
       d.u1<=d.u0||d.v1<=d.v0)return fail();
    std::memcpy(d.raw.data(),bytes,320);return out;
}
std::optional<NativePresentPlan> bind_present_source(const FinalResample& decoded,NativeVideoSource source,unsigned w,unsigned h){
    const auto canonical=decode_final_resample(decoded.raw.data(),decoded.raw.size());if(!canonical)return std::nullopt;
    NativePresentPlan result{canonical.plan,std::move(source),w,h};if(!valid(result))return std::nullopt;return result;
}
void NativeVideoProducer::pump(){if(!failed_&&!pending_&&!in_flight_&&!queue_.empty()){pending_=std::move(queue_.front());queue_.pop_front();}}
VideoError NativeVideoProducer::enqueue(unsigned command,std::shared_ptr<const VideoEnvironment> environment){
    if(command!=0x40&&command!=0x41)return VideoError::unsupported_command;
    if(!environment)return VideoError::invalid_environment;
    for(const auto& p:environment->variants)if(!valid(p))return VideoError::invalid_source;
    std::lock_guard<std::mutex> hold(mutex_);if(failed_)return VideoError::presentation_failed;
    try{auto owned=std::make_shared<const VideoEnvironment>(*environment);
        queue_.push_back({command==0x41?1u:0u,std::move(owned)});pump();return VideoError::none;}
    catch(const std::bad_alloc&){return VideoError::allocation_failure;}catch(const std::length_error&){return VideoError::allocation_failure;}
}
void NativeVideoProducer::pacing(std::uint64_t minimum,bool paused){std::lock_guard<std::mutex> hold(mutex_);minimum_frames_=minimum;paused_=paused;}
FrameResult NativeVideoProducer::on_host_frame(unsigned variant){
    if(variant>1)return {VideoError::invalid_variant,{}};
    std::lock_guard<std::mutex> hold(mutex_);if(failed_)return {VideoError::presentation_failed,{}};
    if(paused_)return {};
    if(frames_<minimum_frames_){++frames_;return {};}
    if(!pending_||in_flight_)return {};
    const auto id=token();if(!id)return {VideoError::token_exhausted,{}};
    NativePresentRequest request{*id,pending_->buffer,variant,pending_->environment};
    in_flight_=request;pending_.reset();frames_=1;return {VideoError::none,std::move(request)};
}
VideoError NativeVideoProducer::complete(std::uint64_t id,bool succeeded){
    std::lock_guard<std::mutex> hold(mutex_);
    if(!in_flight_||in_flight_->completion!=id)return VideoError::stale_completion;
    if(!succeeded){failed_=true;return VideoError::presentation_failed;}
    if(failed_)return VideoError::presentation_failed;
    if(completed_==std::numeric_limits<std::uint64_t>::max())return VideoError::token_exhausted;
    ++completed_;in_flight_.reset();pump();return VideoError::none;
}
void NativeVideoProducer::reset(){std::lock_guard<std::mutex> hold(mutex_);queue_.clear();pending_.reset();in_flight_.reset();frames_=completed_=0;failed_=paused_=false;minimum_frames_=0;}
ProducerStatus NativeVideoProducer::status()const{std::lock_guard<std::mutex> hold(mutex_);return {queue_.size(),bool(pending_),bool(in_flight_),failed_,frames_,completed_};}
} // namespace mka::video
