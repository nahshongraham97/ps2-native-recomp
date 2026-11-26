#pragma once
// Typed source-capacity semantics for the original producer's partial output.
// Original load words and omitted filter inputs are explicit proved bindings.
// No Cpu, register bank, code buffer, opcode decoder or runtime compiler.
#include "reference_resource_layout.hpp"
#include <optional>

namespace mka::emitted::partial_resources {
using namespace mka::emitted::resources;
struct Descriptor {
    resources::Descriptor body;
    std::array<Filter,4> filters{};unsigned filter_count{};
};
struct Bindings {
    // Full fetched words, not the nominal three-word vector descriptor.
    // A failed vector append can fetch an earlier aligned block; its owner
    // must prove that input outside this native algorithm.
    std::array<std::optional<Vector>,13> constant_words;
    std::array<std::optional<Bits>,4> omitted_filter_limits;
};
struct Prepared {
    Descriptor descriptor;resources::Prepared body;
    std::array<Bits,4> filter_limits{};
    unsigned prelude{},main{},accepted{},remaps{},stores{};
    std::array<bool,13> multiplied{};
};
inline bool same(reference_resources::Slot a,reference_resources::Slot b){
    return a.present()&&a.bank==b.bank&&a.index==b.index;
}
inline Prepared prepare(const Descriptor&d,const Streams&s,const Bindings&inputs){
    if(d.body.filter_count||d.filter_count>4)refuse("partial resource pipeline: filter transport unproved");
    resources::validate(d.body,s);
    const auto layout=reference_resources::derive_layout(
        {d.body.attributes.data(),d.body.attribute_count},
        {d.body.effects.data(),d.body.effect_count},{d.filters.data(),d.filter_count});
    // Other source-positive truncations require their own native policy.
    if(layout.intended[3]>64||layout.intended[4]>12||layout.intended[6]>16)
        refuse("partial resource pipeline: additional source truncation unproved");
    Prepared p{};p.descriptor=d;p.body.descriptor=d.body;
    p.prelude=std::min(layout.intended[0],16u)+std::min(layout.intended[1],8u)+2;
    p.main=layout.intended[3]-2;
    p.accepted=std::min(layout.intended[5],24u);
    std::array<bool,13>loaded{};std::array<bool,4>filter_loaded{};
    unsigned load_number=0;
    // Original node construction order determines the constant-load prefix.
    // This is source layout metadata, never a runtime machine-value bank.
    for(unsigned n=13;n<layout.node_count;++n){const auto&node=layout.nodes[n];
        const bool zero=same(node.slot,layout.zero);
        if(node.stream!=2&&!zero)continue;
        const bool emits=load_number++<16;
        for(unsigned i=0;i<d.body.effect_count;++i){const auto&slot=layout.effects[i];
            if(same(node.slot,slot.constant))loaded[i]=emits;
            if((same(node.slot,slot.upper)||same(node.slot,slot.scale)||same(node.slot,slot.start)||same(node.slot,slot.extent))&&!emits)
                refuse("partial resource pipeline: omitted remap input unproved");}
        for(unsigned i=0;i<d.filter_count;++i)if(same(node.slot,layout.filters[i]))filter_loaded[i]=emits;
        if(zero&&!emits)refuse("partial resource pipeline: omitted shared zero unproved");
    }
    unsigned multiply_number=0;
    for(unsigned i=0;i<d.body.effect_count;++i){const auto&e=d.body.effects[i];
        if(e.id==5||e.id==6||e.id==26){
            if(!loaded[i]||!inputs.constant_words[i])
                refuse("partial resource pipeline: original full constant fetch unbound");
            const auto&raw=*inputs.constant_words[i];const bool multiply=multiply_number++<8;p.multiplied[i]=multiply;
            if(format(e.destination_tag)==1){
                auto&v=p.body.vector_constants[i];v=raw;
                if(multiply){for(unsigned lane=0;lane<4;++lane)v[lane]=ps2::fp::mul(v[lane],d.body.vector_step);
                    p.body.vector.arithmetic_result(v);p.body.vector_written=true;}
            }else p.body.scalar_constants[i]=multiply?
                ps2::fp::ee::mul(p.body.prelude_flags,raw[0],d.body.scalar_step):raw[0];
        }else if(e.id==17){++p.remaps;
            p.body.upper[i]=ps2::fp::cvt_s_w(static_cast<Bits>(e.remap.upper));std::uint32_t producer_flags{};
            p.body.scale[i]=ps2::fp::ee::div(producer_flags,ps2::fp::ee::add(producer_flags,0xbd4ccccdu,
                ps2::fp::cvt_s_w(static_cast<Bits>(e.remap.upper)-static_cast<Bits>(e.remap.lower))),e.remap.extent);
        }
    }
    for(unsigned i=0;i<d.filter_count;++i){
        if(filter_loaded[i])p.filter_limits[i]=d.filters[i].limit;
        else{if(!inputs.omitted_filter_limits[i])refuse("partial resource pipeline: omitted filter caller input unbound");
            p.filter_limits[i]=*inputs.omitted_filter_limits[i];}
    }
    const unsigned retained=layout.retained.present()?1:0;
    // Source accepted block: retained quad, six remap operations, count++,
    // then the ordered attribute-store prefix. Tail remap store is separate.
    const unsigned prefix=retained+6*p.remaps+1;
    if(p.accepted<prefix)refuse("partial resource pipeline: truncated accepted prefix unproved");
    p.stores=std::min(d.body.attribute_count,p.accepted-prefix);
    return p;
}
struct Result {resources::Result effects;std::uint64_t filter_visits{},cost{};};
inline Result run(const Streams&s,const Prepared&p,Writes writes={}){
    const auto&d=p.descriptor.body;resources::validate(d,s);Result out{};auto&result=out.effects;
    result.scalar_flags=p.body.prelude_flags;result.vector=p.body.vector;result.vector_written=p.body.vector_written;
    for(unsigned element=0;element<s.count;++element){
        std::array<Value,20>values{};Vector retained{};bool has_retained=false;unsigned retained_attribute=0;
        for(unsigned i=0;i<d.attribute_count;++i){const auto&a=d.attributes[i];const auto&stream=s.streams[a.stream];
            const auto*from=stream.input.data()+std::size_t(element)*stream.stride+a.offset;const auto cls=format(a.tag);
            if(cls<=2)std::memcpy(values[i].vector.data(),from,16);
            else if(cls==5)std::memcpy(&values[i].integer,from,8);
            else std::memcpy(&values[i].scalar,from,4);
            if(a.tag==259){std::memcpy(retained.data(),stream.input.data()+std::size_t(element)*stream.stride+(a.offset&~15u),16);has_retained=true;retained_attribute=i;}
        }
        int remap=-1;
        for(unsigned i=0;i<d.effect_count;++i){const auto&e=d.effects[i];
            if(e.id==21||e.id==22||e.id==24)continue;
            auto&to=values[index(d,e.destination_tag)];
            if(e.id==1)to.scalar=ps2::fp::ee::add(result.scalar_flags,to.scalar,d.scalar_step);
            else if(e.id==2||e.id==3||e.id==25){const auto&from=values[index(d,e.source_tag)];
                if(format(e.source_tag)==1){const Vector source=from.vector,old=to.vector;
                    for(unsigned lane=0;lane<4;++lane)result.vector_accumulator[lane]=ps2::fp::add(old[lane],0);
                    result.vector.arithmetic_result(result.vector_accumulator);result.vector_accumulator_written=true;
                    for(unsigned lane=0;lane<4;++lane)to.vector[lane]=ps2::fp::add(result.vector_accumulator[lane],ps2::fp::mul(source[lane],d.vector_step));
                    result.vector.arithmetic_result(to.vector);result.vector_written=true;
                }else{result.scalar_accumulator=ps2::fp::ee::add(result.scalar_flags,to.scalar,0);result.scalar_accumulator_written=true;
                    to.scalar=ps2::fp::ee::madd(result.scalar_flags,result.scalar_accumulator,from.scalar,d.scalar_step);}
            }else if(e.id==5||e.id==6||e.id==26){if(format(e.destination_tag)==1){
                    for(unsigned lane=0;lane<4;++lane)to.vector[lane]=ps2::fp::add(to.vector[lane],p.body.vector_constants[i][lane]);
                    result.vector.arithmetic_result(to.vector);result.vector_written=true;
                }else to.scalar=ps2::fp::ee::add(result.scalar_flags,to.scalar,p.body.scalar_constants[i]);
            }else if(e.id==17)remap=static_cast<int>(i);
        }
        result.last_resources=values;result.last_retained=retained;result.element_observed=true;
        bool accepted=true;
        for(unsigned i=0;i<p.descriptor.filter_count;++i){++out.filter_visits;const auto&f=p.descriptor.filters[i];result.condition_written=true;
            result.condition=ps2::fp::key(values[index(d,f.source_tag)].scalar)<=ps2::fp::key(p.filter_limits[i]);
            if(!result.condition){accepted=false;break;}}
        if(!accepted)continue;
        Bits remapped{};
        if(remap>=0){const auto&e=d.effects[remap];Bits value=values[index(d,e.source_tag)].scalar;
            value=ps2::fp::ee::sub(result.scalar_flags,value,e.remap.start);value=ps2::fp::max(value,0);value=ps2::fp::min(value,e.remap.extent);
            result.scalar_accumulator=ps2::fp::ee::add(result.scalar_flags,p.body.upper[remap],0);result.scalar_accumulator_written=true;
            remapped=ps2::fp::cvt_w_s(ps2::fp::ee::msub(result.scalar_flags,result.scalar_accumulator,value,p.body.scale[remap]));
            result.remap_word=remapped;result.remap_written=true;}
        if(has_retained){const auto&a=d.attributes[retained_attribute];const auto&stream=s.streams[a.stream];
            write(stream.output.data()+std::size_t(result.accepted)*stream.stride+(a.offset&~15u),retained.data(),16,writes);}
        for(unsigned i=0;i<p.stores;++i){const auto&a=d.attributes[i];const auto&stream=s.streams[a.stream];
            auto*to=stream.output.data()+std::size_t(result.accepted)*stream.stride+a.offset;const auto cls=format(a.tag);
            if(cls<=2)write(to,values[i].vector.data(),16,writes);
            else if(cls==5)write(to,&values[i].integer,8,writes);
            else write(to,&values[i].scalar,4,writes);}
        if(remap>=0){const auto&a=d.attributes[index(d,257)];const auto&stream=s.streams[a.stream];
            write(stream.output.data()+std::size_t(result.accepted)*stream.stride+a.offset+12,&remapped,4,writes);}
        ++result.accepted;
    }
    out.cost=p.prelude+4+std::uint64_t(s.count)*(p.main+2)+3*out.filter_visits+
        std::uint64_t(result.accepted)*(p.accepted+2+p.remaps);
    return out;
}
inline Result run_no_yield(const Streams&s,const Prepared&p,std::int64_t budget,Writes w={}){
    const auto worst=p.prelude+4+std::uint64_t(s.count)*(p.main+2+3*p.descriptor.filter_count+p.accepted+2+p.remaps);
    if(budget<0||std::uint64_t(budget)<worst)refuse("partial resource pipeline: insufficient budget before writes");
    return run(s,p,w);
}
}
