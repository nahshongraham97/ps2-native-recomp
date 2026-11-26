#pragma once
// Bounded semantic backend for the authenticated five-resource layout.
// Tags are original resource identifiers, not inferred game/class names.
// This is a native data/effect pipeline: no Cpu, guest PC/register bank,
// executable-word decoder, translator, code cache or opcode operation stream.
#include "ps2/fpu.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace mka::emitted::semantic {
using Bits = std::uint32_t;
using Block = std::array<Bits,4>;
struct Effect {
    std::uint16_t id{};
    std::uint32_t destination_tag{},source_tag{};
    // Whole128-bit constant fetch must retain the fourth word, including
    // original constant-block adjacency. It is not assumed to be zero.
    Block constant{};
    struct Remap {std::int32_t upper{},lower{};Bits start{},extent{};} remap;
};
struct Pipeline {
    std::array<Effect,13> effects{};
    std::uint32_t effect_count{};
    std::uint32_t filter_count{},filter_id{},filter_tag{};
    Bits filter_limit{},step{};
};
struct Streams {
    std::span<std::byte> input_a,input_b,output_a,output_b;
    std::size_t stride_a{},stride_b{};
    std::uint32_t count{};
};
struct Writes {
    void* context{};
    void(*observe)(void*,const std::byte*,std::span<const std::byte>){};
};
struct ScalarEffects {
    Bits raised_flags{};
    bool condition_written{},condition{};
};
struct Result {std::uint32_t accepted{};ScalarEffects scalar;};

[[noreturn]] inline void refuse(const char* reason){throw std::runtime_error(reason);}
inline void extent(std::span<std::byte> data,std::size_t stride,std::uint32_t count,std::size_t width){
    if(!count)return;
    if(data.size()<width || (stride&&std::size_t(count-1)>(data.size()-width)/stride))
        refuse("semantic pipeline: stream extent unproved");
}
inline bool binding(const Effect& e){
    switch(e.id){
    case 1:return e.destination_tag==769;
    case 2:return e.destination_tag==256&&e.source_tag==256;
    case 3:case 25:return e.destination_tag==769&&e.source_tag==258;
    case 5:return e.destination_tag==256;
    case 6:return e.destination_tag==258;
    case 26:return e.destination_tag==256||e.destination_tag==258;
    case 17:return e.destination_tag==257&&e.source_tag==769;
    case 21:case 22:case 24:return e.destination_tag==769;
    default:refuse("semantic pipeline: unsupported original effect ID");
    }
}
inline void validate(const Pipeline& p){
    if(p.effect_count>13)refuse("semantic pipeline: effect count outside proved13-record producer bound");
    unsigned remaps=0;
    for(unsigned i=0;i<p.effect_count;++i){if(!binding(p.effects[i]))refuse("semantic pipeline: resource binding outside proved family");remaps+=p.effects[i].id==17;}
    if(remaps>1)refuse("semantic pipeline: duplicate17 original context refusal");
    // Only0 or1 filters are currently source+complete-program witnessed.
    // The original append routine has no local capacity guard; never infer
    // arbitrary cardinality from the physical space between record fields.
    if(p.filter_count>1)refuse("semantic pipeline: filter cardinality unproved");
    const bool captured=p.effect_count==4&&p.effects[0].id==1&&p.effects[1].id==5&&p.effects[2].id==6&&p.effects[3].id==17;
    if(p.filter_count && (!captured||p.filter_id!=2||p.filter_tag!=769))refuse("semantic pipeline: filter binding outside proved family");
    bool repeat=p.effect_count==13;
    for(unsigned i=0;i<p.effect_count&&repeat;++i)repeat=p.effects[i].id==1;
    if(!(p.effect_count<=1||captured||repeat))refuse("semantic pipeline: effect composition unproved");
}
inline Bits scalar(const std::byte* address){Bits value;std::memcpy(&value,address,4);return value;}
inline Block block(const std::byte* address){Block value;std::memcpy(value.data(),address,16);return value;}
inline void write(std::byte* destination,const void* source,std::size_t size,const Writes& writes){
    const auto bytes=std::span(static_cast<const std::byte*>(source),size);
    if(writes.observe)writes.observe(writes.context,destination,bytes);
    std::memcpy(destination,source,size);
}

// All effect constants are resolved before the first element, matching the
// original emitted prelude. The complete logical resource snapshot is loaded
// before writes; overlapping byte fields remain separate typed values.
inline Result run(Streams streams,Pipeline pipeline,Writes writes={}){
    validate(pipeline);
    if(streams.count && (streams.count>0x7fffffffu||streams.stride_a>0x7fffffffu||streams.stride_b>0x7fffffffu||
       streams.stride_a<48||(streams.stride_a&15u)))refuse("semantic pipeline: count/stride domain unproved");
    if(streams.count && ((reinterpret_cast<std::uintptr_t>(streams.input_a.data())&15u)||(reinterpret_cast<std::uintptr_t>(streams.output_a.data())&15u)))
        refuse("semantic pipeline: aligned resource blocks required");
    extent(streams.input_a,streams.stride_a,streams.count,48);extent(streams.output_a,streams.stride_a,streams.count,48);
    extent(streams.input_b,streams.stride_b,streams.count,4);extent(streams.output_b,streams.stride_b,streams.count,4);
    Result result{};auto& scalar_flags=result.scalar.raised_flags;
    struct Prepared {Block vector;Bits scalar,upper,scale,start,extent;};
    std::array<Prepared,13> prepared{};
    for(unsigned i=0;i<pipeline.effect_count;++i){const auto&e=pipeline.effects[i];auto&v=prepared[i];
        if(e.id==5||(e.id==26&&e.destination_tag==256))for(unsigned lane=0;lane<4;++lane)v.vector[lane]=ps2::fp::mul(e.constant[lane],pipeline.step);
        if(e.id==6||(e.id==26&&e.destination_tag==258))v.scalar=ps2::fp::ee::mul(scalar_flags,e.constant[0],pipeline.step);
        if(e.id==17){v.upper=ps2::fp::cvt_s_w(static_cast<Bits>(e.remap.upper));
            const Bits difference=static_cast<Bits>(e.remap.upper)-static_cast<Bits>(e.remap.lower);
            // These operations belong to the ORIGINAL BUILDER, not to the
            // emitted callee. Do not leak their flags into callee effects.
            Bits producer_flags{};
            v.scale=ps2::fp::ee::div(producer_flags,ps2::fp::ee::add(producer_flags,0xbd4ccccdu,ps2::fp::cvt_s_w(difference)),e.remap.extent);
            v.start=e.remap.start;v.extent=e.remap.extent;
        }
    }
    // The callee's scalar constant*step prelude still executes at count0;
    // all stream accesses are skipped, and an existing condition is retained.
    if(!streams.count)return result;
    for(unsigned index=0;index<streams.count;++index){
        const auto*a=streams.input_a.data()+std::size_t(index)*streams.stride_a;
        const auto*b=streams.input_b.data()+std::size_t(index)*streams.stride_b;
        Block tag256=block(a),tag257=block(a+32),retained_middle=block(a+16);
        Bits tag259=scalar(a+16),tag258=scalar(a+12),tag769=scalar(b),remapped{},remap_input{};bool has_remap=false;unsigned remap_at=0;
        for(unsigned i=0;i<pipeline.effect_count;++i){const auto&e=pipeline.effects[i];const auto&v=prepared[i];
            switch(e.id){
            case 1:tag769=ps2::fp::ee::add(scalar_flags,tag769,pipeline.step);break;
            case 2:for(auto&lane:tag256)lane=ps2::fp::add(ps2::fp::add(lane,0),ps2::fp::mul(lane,pipeline.step));break;
            case 3:case 25:tag769=ps2::fp::ee::madd(scalar_flags,ps2::fp::ee::add(scalar_flags,tag769,0),tag258,pipeline.step);break;
            case 5:case 6:case 26:
                if(e.destination_tag==256)for(unsigned lane=0;lane<4;++lane)tag256[lane]=ps2::fp::add(tag256[lane],v.vector[lane]);
                else tag258=ps2::fp::ee::add(scalar_flags,tag258,v.scalar);
                break;
            case 17:remap_input=tag769;remap_at=i;has_remap=true;break;
            case 21:case 22:case 24:break;
            default:refuse("semantic pipeline: effect admission invariant");
            }
        }
        if(pipeline.filter_count){result.scalar.condition_written=true;result.scalar.condition=ps2::fp::key(tag769)<=ps2::fp::key(pipeline.filter_limit);if(!result.scalar.condition)continue;}
        // In every admitted configuration17 is last/alone. Original CMP and
        // BC1F skip its arithmetic (and its flags) for rejected elements.
        if(has_remap){const auto&v=prepared[remap_at];Bits value=ps2::fp::ee::sub(scalar_flags,remap_input,v.start);
            value=ps2::fp::max(value,0);value=ps2::fp::min(value,v.extent);
            remapped=ps2::fp::cvt_w_s(ps2::fp::ee::msub(scalar_flags,ps2::fp::ee::add(scalar_flags,v.upper,0),value,v.scale));}
        auto*out_a=streams.output_a.data()+std::size_t(result.accepted)*streams.stride_a;
        auto*out_b=streams.output_b.data()+std::size_t(result.accepted)*streams.stride_b;
        // Retain the observable transient stores, not merely final bytes.
        write(out_a+16,retained_middle.data(),16,writes);write(out_a,tag256.data(),16,writes);
        write(out_a+16,&tag259,4,writes);write(out_a+32,tag257.data(),16,writes);
        write(out_a+12,&tag258,4,writes);write(out_b,&tag769,4,writes);
        if(has_remap)write(out_a+44,&remapped,4,writes);
        ++result.accepted;
    }
    return result;
}
}
