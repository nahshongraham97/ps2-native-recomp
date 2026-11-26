#pragma once
// R192 reference bridge only. The native algorithm/continuation do not own Cpu
// or a register bank. These are the exact known-family source live-outs.
#include "semantic_continuation.hpp"
#include "ps2/generated.hpp"
#include <cstring>

namespace mka::emitted::reference {
struct FrameInputs {
    std::uint64_t input_a,input_b,output_a,output_b,stride_a,stride_b;
    std::uint32_t scalar_control_status,vector_status;
    static FrameInputs capture(const ps2::Cpu&c){
        return {c.r[4].d[0],c.r[5].d[0],c.r[6].d[0],c.r[7].d[0],c.r[8].d[0],c.r[9].d[0],c.fcr31,c.vu0.status};
    }
};
inline void project(ps2::Cpu&c,const FrameInputs&inputs,const semantic::ClipContinuation&continuation){
    const auto&result=continuation.result();const auto&s=result.scratch;
    if(!result.vector_effects_admitted)semantic::refuse("reference projection: family effects unproved");
    if(s.element_observed)std::memcpy(&c.r[1],s.retained_middle.data(),16);
    ps2::set32(c.r[2],result.accepted);
    c.r[4].d[0]=inputs.input_a+continuation.consumed()*inputs.stride_a;
    c.r[5].d[0]=inputs.input_b+continuation.consumed()*inputs.stride_b;
    c.r[6].d[0]=inputs.output_a+result.accepted*inputs.stride_a;
    c.r[7].d[0]=inputs.output_b+result.accepted*inputs.stride_b;
    ps2::set32(c.r[10],continuation.remaining()-1u);
    c.f[3]=s.prepared.scalar_product;c.f[4]=s.prepared.zero;
    c.f[5]=s.prepared.remap_upper;c.f[6]=s.prepared.remap_scale;
    c.f[7]=s.prepared.remap_start;c.f[8]=s.prepared.remap_extent;c.f[10]=s.prepared.filter_limit;
    if(s.element_observed){c.f[0]=s.resource259;c.f[1]=s.resource258;c.f[2]=s.resource769;}
    if(s.accepted_observed){c.f[9]=s.remap_word;c.facc=s.remap_accumulator;}
    const auto&v=result.vector;
    std::memcpy(&c.vu0.vf[4],v.constant_product.data(),16);
    if(v.element_observed){std::memcpy(&c.vu0.vf[2],v.last_resource256.data(),16);std::memcpy(&c.vu0.vf[3],v.last_resource257.data(),16);}
    c.vu0.mac=v.last_mac;c.vu0.status=v.project_status(inputs.vector_status);
    c.fcr31=semantic::no_yield::project_scalar(inputs.scalar_control_status,result.scalar);
    c.budget=continuation.budget();
    // PC/base remain owned by the original dispatch/call/unwind machinery.
}
inline bool parked_state_matches(const ps2::Cpu&c,const FrameInputs&inputs,const semantic::ClipContinuation&continuation){
    // Ephemeral reference comparison, not persisted native state. A changed
    // shared VU/known scratch frame is refused until its interleaving is proved.
    ps2::Cpu expected=c;project(expected,inputs,continuation);expected.budget=c.budget;
    return !std::memcmp(&expected,&c,sizeof c);
}
}
