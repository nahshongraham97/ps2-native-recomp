#include "mka/video_producer.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"video producer check %d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace {
struct TestResource final:mka::video::VideoFrameResource{
    std::uint64_t id;unsigned w,h;
    TestResource(std::uint64_t i,unsigned width,unsigned height):id(i),w(width),h(height){}
    std::uint64_t identity()const noexcept override{return id;}
    unsigned width()const noexcept override{return w;}unsigned height()const noexcept override{return h;}
};
}
int main(int argc,char** argv){
    using namespace mka::video;
    if(argc!=2){std::fprintf(stderr,"usage: test_video_producer <explicit pinned version8.state>\n");return 2;}
    std::ifstream file(argv[1],std::ios::binary);CHECK(file);
    std::array<char,20> header{};file.read(header.data(),header.size());
    CHECK(std::string(header.data(),8)=="PS2STATE"&&header[8]==8&&header[9]==0&&header[10]==0&&header[11]==0);
    std::array<std::uint8_t,320> raw{};file.seekg(1476ull+0x6a3fc0);file.read(reinterpret_cast<char*>(raw.data()),raw.size());CHECK(file);
    static constexpr std::uint64_t captured[40]={0x70000013,0,0x1000000000008012,0xe,
        0xa0d0000,0x4c,0,0x18,0x7fff00007fff0000,0x40,0x30000,0x47,0x10a0000e7,0x4e,1,0x45,0x6fc009fc00a,8,
        0xaa8028b60,6,0x61,0x14,0x16,0,0x3f80000000000000,1,0x3a00000039c00000,2,0,5,
        0x3ee040003f201800,2,0x1c003400,5,0x720000006c00,0x18,0xa005b,0x4c,0xa0000e7,0x4e};
    CHECK(std::memcmp(captured,raw.data(),320)==0); // Actual snapshot fixture; outer harness SHA-pins fullstate.
    const auto d=decode_final_resample(raw.data(),raw.size());CHECK(d);
    CHECK(d.plan.source_base==2912&&d.plan.source_stride==10&&d.plan.source_format==0);
    CHECK(d.plan.virtual_width==1024&&d.plan.virtual_height==1024&&d.plan.clamp_width==640&&d.plan.clamp_height==448);
    CHECK(d.plan.output_width==832&&d.plan.output_height==448&&d.plan.destination_dither);
    CHECK(d.plan.u0*1024==.375f&&d.plan.v0*1024==.5f&&d.plan.u1*1024==640.375f&&d.plan.v1*1024==448.5f);
    CHECK(!decode_final_resample(nullptr,320));CHECK(!decode_final_resample(raw.data(),319));
    auto invalid=raw;invalid[40]=0x4d;CHECK(!decode_final_resample(invalid.data(),320));
    invalid=raw;invalid[176]=0x17;CHECK(!decode_final_resample(invalid.data(),320)); // Different inherited-state interpretation not accepted.
    auto a=std::make_shared<TestResource>(0x1000000001ull,640,448);
    NativeVideoSource source{d.plan.source_base,d.plan.source_stride,d.plan.source_format,a};
    const auto plan=bind_present_source(d.plan,source,640,448);CHECK(plan);
    auto wrong=source;wrong.serialized_base+=32;CHECK(!bind_present_source(d.plan,wrong,640,448));
    wrong=source;wrong.resource=std::make_shared<TestResource>(2,639,448);CHECK(!bind_present_source(d.plan,wrong,640,448));
    // Distinct source association, derived from another explicit serialized
    // TEX0, proves the producer never special-cases FBP91/native identity.
    auto other_raw=raw;const unsigned next_base=d.plan.source_base+32;
    other_raw[144]=next_base&255;other_raw[145]=(other_raw[145]&0xc0)|((next_base>>8)&63);
    const auto other_decode=decode_final_resample(other_raw.data(),320);CHECK(other_decode);
    auto b=std::make_shared<TestResource>(0x2000000001ull,640,448);
    const auto other=bind_present_source(other_decode.plan,{next_base,10,0,b},640,448);CHECK(other);
    auto environment=std::make_shared<VideoEnvironment>();
    environment->variants={*plan,*other,*other,*plan};
    NativeVideoProducer producer;producer.pacing(2);
    CHECK(producer.enqueue(0x42,environment)==VideoError::unsupported_command);
    CHECK(producer.enqueue(0x40,environment)==VideoError::none);
    CHECK(producer.enqueue(0x41,environment)==VideoError::none);
    CHECK(producer.status().pending&&producer.status().queued==1&&!producer.status().in_flight);
    CHECK(!producer.on_host_frame(0).request);CHECK(!producer.on_host_frame(0).request);
    const auto first=producer.on_host_frame(0);CHECK(first.request&&first.request->buffer==0&&first.request->plan().source.resource->identity()==a->id);
    CHECK(producer.status().in_flight&&!producer.status().pending);
    CHECK(!producer.on_host_frame(1).request);CHECK(!producer.on_host_frame(1).request);
    CHECK(producer.complete(first.request->completion+1,true)==VideoError::stale_completion);
    CHECK(producer.complete(first.request->completion,true)==VideoError::none);
    CHECK(producer.status().pending&&producer.status().completed==1);
    // Caller mutation cannot replace an already queued immutable environment.
    environment->variants[3]=*other;
    const auto second=producer.on_host_frame(1);CHECK(second.request&&second.request->buffer==1&&second.request->variant==1);
    CHECK(second.request->plan().source.resource->identity()==a->id);
    CHECK(producer.complete(second.request->completion,false)==VideoError::presentation_failed);
    CHECK(producer.on_host_frame(0).error==VideoError::presentation_failed);
    CHECK(producer.enqueue(0x40,environment)==VideoError::presentation_failed);
    producer.reset();CHECK(producer.complete(second.request->completion,true)==VideoError::stale_completion);
    producer.pacing(0,true);CHECK(producer.enqueue(0x41,environment)==VideoError::none);
    CHECK(!producer.on_host_frame(0).request&&producer.status().host_frames_since_flip==0);
    producer.pacing(0,false);CHECK(producer.on_host_frame(2).error==VideoError::invalid_variant);
    const auto resumed=producer.on_host_frame(1);CHECK(resumed.request&&resumed.request->completion!=first.request->completion);
    CHECK(resumed.request->plan().source.resource->identity()==b->id);
    CHECK(producer.complete(resumed.request->completion,true)==VideoError::none);
    auto bad_environment=std::make_shared<VideoEnvironment>(*environment);bad_environment->variants[0].resample.u1=0;
    CHECK(producer.enqueue(0x40,bad_environment)==VideoError::invalid_source);
    std::puts("native video capturedchain decode, explicit source binding, immutable queue, pacing, fence and failure ordering PASS");
}
