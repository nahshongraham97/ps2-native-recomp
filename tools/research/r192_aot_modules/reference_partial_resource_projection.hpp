#pragma once
// Reference-only machine observation for the tested partial-output policy.
// Isolated original body plus budget-only guards, not actual scheduler/owner
// admission. Never use this to restore untouched global state after interleave.
#include "partial_resource_continuation.hpp"
#include "ps2/cpu.hpp"
#include <limits>

namespace mka::emitted::partial_resources {
static_assert(sizeof(ps2::Cpu)==1456,"original v8 Cpu ABI required");
inline ps2::Cpu project(const ps2::Cpu&entry,std::uint32_t code_base,const Prepared&prepared,const resources::Boundary&boundary){
    using reference_resources::Slot;
    const auto&pd=prepared.descriptor;const auto&d=pd.body;
    const auto layout=reference_resources::derive_layout({d.attributes.data(),d.attribute_count},{d.effects.data(),d.effect_count},{pd.filters.data(),pd.filter_count});
    unsigned code=0;for(unsigned i:{0u,1u,3u,4u,5u,6u})code+=std::min(layout.intended[i],layout.capacity[i]);
    bool vector_constants=false;for(unsigned i=0;i<d.effect_count;++i)vector_constants|=layout.effects[i].constant.bank==0;
    const unsigned data_start=vector_constants?(code+3)&~3u:code;
    if((code_base&15)||code_base>0x02000000u-4*(data_start+layout.data_words+4)||entry.pc!=code_base||entry.r[11].d[0]!=std::uint64_t(code_base)+4*data_start)
        refuse("partial resource projection: original entry/data-base contract unbound");
    if(entry.f[12]!=d.scalar_step||entry.vu0.vf[1].w[0]!=d.vector_step||entry.r[8].d[0]>0x7fffffffu||entry.r[9].d[0]>0x7fffffffu)
        refuse("partial resource projection: original step/stride contract unbound");
    const auto count=entry.r[10].w[0];using resources::Phase;
    if(count>0x7fffffffu||entry.r[10].d[0]!=count||boundary.consumed>count||boundary.accepted>boundary.consumed||
       (boundary.phase!=Phase::suspended&&boundary.phase!=Phase::complete)||
       (boundary.phase==Phase::suspended&&std::uint64_t(boundary.consumed)+boundary.remaining_after_pending+1!=count)||
       (boundary.phase==Phase::complete&&boundary.consumed!=count))refuse("partial resource projection: boundary count/phase unproved");
    if(boundary.budget<std::numeric_limits<std::int32_t>::min()||boundary.budget>std::numeric_limits<std::int32_t>::max())
        refuse("partial resource projection: original int32 budget unbound");
    const auto&e=boundary.effects;ps2::Cpu out;std::memcpy(&out,&entry,sizeof out);
    auto scalar=[&](Slot s,Bits value){if(s.present()){if(s.bank!=1)refuse("partial resource projection: scalar bank");out.f[s.index]=value;}};
    auto vector=[&](Slot s,const Vector&v){if(s.present()){if(s.bank!=0)refuse("partial resource projection: vector bank");std::memcpy(&out.vu0.vf[s.index],v.data(),16);}};
    scalar(layout.zero,0);
    for(unsigned i=0;i<d.effect_count;++i){const auto&s=layout.effects[i];
        if(s.constant.present()){if(s.constant.bank==0)vector(s.constant,prepared.body.vector_constants[i]);else scalar(s.constant,prepared.body.scalar_constants[i]);}
        scalar(s.upper,prepared.body.upper[i]);scalar(s.scale,prepared.body.scale[i]);scalar(s.start,d.effects[i].remap.start);scalar(s.extent,d.effects[i].remap.extent);
        if(e.remap_written)scalar(s.work,e.remap_word);
    }
    // An omitted load does not write its FPR. Its explicit native input must
    // equal that original caller word; mismatches are not projected as writes.
    std::array<bool,4>filter_loaded{};unsigned load_number=0;
    for(unsigned n=13;n<layout.node_count;++n){const auto&node=layout.nodes[n];
        if(node.stream!=2&&!same(node.slot,layout.zero))continue;
        const bool loaded=load_number++<16;
        for(unsigned i=0;i<pd.filter_count;++i)
            if(same(node.slot,layout.filters[i]))filter_loaded[i]=loaded;
    }
    for(unsigned i=0;i<pd.filter_count;++i){
        if(filter_loaded[i])scalar(layout.filters[i],prepared.filter_limits[i]);
        else if(entry.f[layout.filters[i].index]!=prepared.filter_limits[i])
            refuse("partial resource projection: omitted filter caller word mismatch");
    }
    if(e.element_observed){for(unsigned i=0;i<d.attribute_count;++i){const auto&s=layout.resources[i];const auto&v=e.last_resources[i];
        if(s.bank==0)vector(s,v.vector);else if(s.bank==1)scalar(s,v.scalar);
        else if(s.bank==2)out.r[s.index].sd[0]=static_cast<std::int32_t>(v.scalar);
        else if(s.bank==3)out.r[s.index].d[0]=v.integer;else refuse("partial resource projection: resource bank");}
        if(layout.retained.present())std::memcpy(&out.r[layout.retained.index],e.last_retained.data(),16);}
    if(e.scalar_accumulator_written)out.facc=e.scalar_accumulator;
    if(e.vector_accumulator_written)std::memcpy(&out.vu0.acc,e.vector_accumulator.data(),16);
    if(e.vector_written){out.vu0.mac=e.vector.last_mac;out.vu0.status=e.vector.project_status(entry.vu0.status);}
    out.fcr31=entry.fcr31|e.scalar_flags;if(e.condition_written)out.fcr31=(out.fcr31&~0x00800000u)|(e.condition?0x00800000u:0);
    out.r[2].d[0]=boundary.accepted;
    out.r[4].d[0]=entry.r[4].d[0]+std::uint64_t(boundary.consumed)*entry.r[8].d[0];
    out.r[5].d[0]=entry.r[5].d[0]+std::uint64_t(boundary.consumed)*entry.r[9].d[0];
    out.r[6].d[0]=entry.r[6].d[0]+std::uint64_t(boundary.accepted)*entry.r[8].d[0];
    out.r[7].d[0]=entry.r[7].d[0]+std::uint64_t(boundary.accepted)*entry.r[9].d[0];
    out.r[10].sd[0]=static_cast<std::int32_t>(boundary.phase==Phase::complete?0xffffffffu:boundary.remaining_after_pending);
    out.budget=static_cast<std::int32_t>(boundary.budget);
    out.pc=boundary.phase==Phase::complete?entry.r[31].w[0]:(code_base+4*prepared.prelude)|ps2::kUnwind;
    return out;
}
}
