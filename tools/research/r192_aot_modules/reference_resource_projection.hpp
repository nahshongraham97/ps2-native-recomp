#pragma once
// Explicit reference-only R192 machine projection. This is not the native
// algorithm API or an accepted native-port register/context representation.
// Runtime caller, producer ownership, code/data bounds, epoch/lifetime and
// actual scheduler/global-VU identity must be proved by a separate adapter.
#include "reference_resource_layout.hpp"
#include "resource_pipeline_continuation.hpp"
#include "ps2/cpu.hpp"
#include <limits>

namespace mka::emitted::reference_resources {
static_assert(sizeof(ps2::Cpu)==1456,"original v8 Cpu ABI required");
inline ps2::Cpu project(const ps2::Cpu&entry,std::uint32_t code_base,const Prepared&prepared,const Boundary&boundary){
    const auto&d=prepared.descriptor;const auto layout=derive_layout(d);layout.require_intact();
    if((code_base&15)||code_base>0x02000000u-4*(layout.total_words+4)||entry.pc!=code_base||entry.r[11].d[0]!=std::uint64_t(code_base)+4*layout.data_start_words)
        refuse("reference resource projection: original entry/data-base contract unbound");
    if(entry.f[12]!=d.scalar_step||entry.vu0.vf[1].w[0]!=d.vector_step||entry.r[8].d[0]>0x7fffffffu||entry.r[9].d[0]>0x7fffffffu)
        refuse("reference resource projection: original step/stride contract unbound");
    if(boundary.budget<std::numeric_limits<std::int32_t>::min()||boundary.budget>std::numeric_limits<std::int32_t>::max())
        refuse("reference resource projection: original int32 budget representation unbound");
    const auto count=entry.r[10].w[0];
    if(count>0x7fffffffu||entry.r[10].d[0]!=count||boundary.consumed>count||boundary.accepted>boundary.consumed||
       (boundary.phase!=Phase::suspended&&boundary.phase!=Phase::complete)||
       (boundary.phase==Phase::suspended&&std::uint64_t(boundary.consumed)+boundary.remaining_after_pending+1!=count)||
       (boundary.phase==Phase::complete&&boundary.consumed!=count))
        refuse("reference resource projection: typed boundary count/phase unproved");
    const auto&e=boundary.effects;ps2::Cpu out;std::memcpy(&out,&entry,sizeof out);
    auto scalar=[&](Slot slot,Bits value){if(slot.present()){if(slot.bank!=1)refuse("reference resource projection: scalar binding bank mismatch");out.f[slot.index]=value;}};
    auto vector=[&](Slot slot,const Vector&value){if(slot.present()){if(slot.bank!=0)refuse("reference resource projection: vector binding bank mismatch");std::memcpy(&out.vu0.vf[slot.index],value.data(),16);}};
    // All constant loads/multiplies and the shared SUBself prelude run even
    // for count0 or a first-guard suspension. Inputs were snapshotted once.
    scalar(layout.zero,0);
    for(unsigned i=0;i<d.effect_count;++i){const auto&s=layout.effects[i];
        if(s.constant.present()){if(s.constant.bank==0)vector(s.constant,prepared.vector_constants[i]);else scalar(s.constant,prepared.scalar_constants[i]);}
        scalar(s.upper,prepared.upper[i]);scalar(s.scale,prepared.scale[i]);scalar(s.start,d.effects[i].remap.start);scalar(s.extent,d.effects[i].remap.extent);
        if(e.remap_written)scalar(s.work,e.remap_word);
    }
    for(unsigned i=0;i<d.filter_count;++i)scalar(layout.filters[i],d.filters[i].limit);
    if(e.element_observed){
        for(unsigned i=0;i<d.attribute_count;++i){const auto&s=layout.resources[i];const auto&v=e.last_resources[i];
            if(s.bank==0)vector(s,v.vector);
            else if(s.bank==1)scalar(s,v.scalar);
            else if(s.bank==2)out.r[s.index].sd[0]=static_cast<std::int32_t>(v.scalar);
            else if(s.bank==3)out.r[s.index].d[0]=v.integer;
            else refuse("reference resource projection: resource bank unproved");
        }
        if(layout.retained.present())std::memcpy(&out.r[layout.retained.index],e.last_retained.data(),16);
    }
    if(e.scalar_accumulator_written)out.facc=e.scalar_accumulator;
    if(e.vector_accumulator_written)std::memcpy(&out.vu0.acc,e.vector_accumulator.data(),16);
    if(e.vector_written){out.vu0.mac=e.vector.last_mac;out.vu0.status=e.vector.project_status(entry.vu0.status);}
    out.fcr31=entry.fcr31|e.scalar_flags;
    if(e.condition_written)out.fcr31=(out.fcr31&~0x00800000u)|(e.condition?0x00800000u:0);
    // DADDU/ADDIU write widths and untouched upper128-bit halves follow the
    // original generated body; a full-register assignment would be wrong.
    out.r[2].d[0]=boundary.accepted;
    out.r[4].d[0]=entry.r[4].d[0]+std::uint64_t(boundary.consumed)*entry.r[8].d[0];
    out.r[5].d[0]=entry.r[5].d[0]+std::uint64_t(boundary.consumed)*entry.r[9].d[0];
    out.r[6].d[0]=entry.r[6].d[0]+std::uint64_t(boundary.accepted)*entry.r[8].d[0];
    out.r[7].d[0]=entry.r[7].d[0]+std::uint64_t(boundary.accepted)*entry.r[9].d[0];
    out.r[10].sd[0]=static_cast<std::int32_t>(boundary.phase==Phase::complete?0xffffffffu:boundary.remaining_after_pending);
    out.budget=static_cast<std::int32_t>(boundary.budget);
    out.pc=boundary.phase==Phase::complete?entry.r[31].w[0]:(code_base+4*layout.prelude_words)|ps2::kUnwind;
    return out;
}
}
