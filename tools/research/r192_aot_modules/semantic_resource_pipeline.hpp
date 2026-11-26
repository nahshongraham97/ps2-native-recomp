#pragma once
// Original producer's resource operations on typed native spans. This core
// has no Cpu, guest PC/register bank, executable-word decoder or scheduler.
// Unseen reference scratch/timing/continuation configurations are NOT admitted
// by an output-equivalence result from this component.
#include "ps2/fpu.hpp"
#include "semantic_vector_effects.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace mka::emitted::resources {
using Bits=std::uint32_t;
using Vector=std::array<Bits,4>;
struct Attribute {Bits tag{},stream{},offset{};};
struct Effect {
    Bits id{},destination_tag{},source_tag{};
    // All four original fetched words are supplied, including adjacent data.
    Vector constant{};
    struct {std::int32_t upper{},lower{};Bits start{},extent{};} remap;
};
struct Filter {Bits id{},source_tag{},limit{};};
struct Descriptor {
    std::array<Attribute,20> attributes{};unsigned attribute_count{};
    std::array<Effect,13> effects{};unsigned effect_count{};
    std::array<Filter,1> filters{};unsigned filter_count{};
    Bits scalar_step{},vector_step{};
};
struct Stream {std::span<std::byte> input,output;std::size_t stride{};};
struct Streams {std::array<Stream,2> streams;std::uint32_t count{};};
struct Writes {void* context{};void(*observe)(void*,const std::byte*,std::span<const std::byte>){};};
// These are logical original resources and algorithm side outputs, never an
// indexed machine register bank. A separate reference-only binding must prove
// each consumer/physical location before projecting them into a saved context.
struct Value {Vector vector{};Bits scalar{};std::uint64_t integer{};};
using VectorFlags=mka::emitted::semantic::VectorEffects;
struct Result {
    std::uint32_t accepted{},scalar_flags{};bool condition_written{},condition{};
    std::array<Value,20> last_resources{};Vector last_retained{},vector_accumulator{};
    bool element_observed{},vector_written{},vector_accumulator_written{},scalar_accumulator_written{},remap_written{};
    Bits scalar_accumulator{},remap_word{};VectorFlags vector;
};
[[noreturn]] inline void refuse(const char*s){throw std::runtime_error(s);}
inline unsigned format(Bits tag){
    switch(tag){
    case 0x300:case 0x303:case 0x304:case 0x100:case 0x104:case 0x165:case 0x400:case 0x201:return 1;
    case 0x101:return 2;
    case 0x301:case 0x305:case 0x306:case 0x102:case 0x103:case 0x402:case 0x403:return 3;
    case 0x302:case 0x307:case 0x308:return 4;
    case 0x309:return 5;
    default:refuse("resource pipeline: unknown original resource tag");
    }
}
inline unsigned width(unsigned cls){return cls<=2?16:cls==5?8:4;}
inline unsigned index(const Descriptor&d,Bits tag){
    for(unsigned i=0;i<d.attribute_count;++i)if(d.attributes[i].tag==tag)return i;
    refuse("resource pipeline: required original resource absent");
}
inline void extent(std::span<std::byte>s,std::size_t stride,std::uint32_t count,std::size_t offset,std::size_t bytes){
    if(!count)return;
    if(offset>s.size()||bytes>s.size()-offset||
       (stride&&std::size_t(count-1)>(s.size()-offset-bytes)/stride))
        refuse("resource pipeline: stream extent or arithmetic unproved");
}
inline void validate(const Descriptor&d,const Streams&s){
    if(!d.attribute_count||d.attribute_count>20||d.effect_count>13||d.filter_count>1)
        refuse("resource pipeline: descriptor count outside bounded domain");
    if(s.count>0x7fffffffu)refuse("resource pipeline: signed iteration count unproved");
    unsigned remaps=0,scalar_resources=0,vector_resources=0,scalar_constants=0,vector_constants=0;
    bool scalar_zero=false;
    for(unsigned i=0;i<d.attribute_count;++i){const auto&a=d.attributes[i];const auto cls=format(a.tag),bytes=width(cls);
        scalar_resources+=cls==3;vector_resources+=cls<=2;
        if(a.stream>=2||a.offset>32767||a.offset%bytes)
            refuse("resource pipeline: stream, signed16 offset or alignment unproved");
        for(unsigned j=0;j<i;++j)if(d.attributes[j].tag==a.tag)
            refuse("resource pipeline: duplicate logical tag alias unproved");
        const auto&stream=s.streams[a.stream];
        if(stream.stride>0x7fffffffu||stream.stride%bytes)
            refuse("resource pipeline: stride alignment or signed extension unproved");
        if(s.count&&((reinterpret_cast<std::uintptr_t>(stream.input.data())|reinterpret_cast<std::uintptr_t>(stream.output.data()))&(bytes-1)))
            refuse("resource pipeline: native span alignment unproved");
        extent(stream.input,stream.stride,s.count,a.offset,bytes);extent(stream.output,stream.stride,s.count,a.offset,bytes);
        if(a.tag==259){
            if(s.count&&((reinterpret_cast<std::uintptr_t>(stream.input.data())|reinterpret_cast<std::uintptr_t>(stream.output.data())|stream.stride)&15))
                refuse("resource pipeline: tag259 transient block alignment unproved");
            extent(stream.input,stream.stride,s.count,a.offset&~15u,16);extent(stream.output,stream.stride,s.count,a.offset&~15u,16);
        }
    }
    for(unsigned i=0;i<d.effect_count;++i){const auto&e=d.effects[i];
        if(e.id!=1&&e.id!=2&&e.id!=3&&e.id!=25&&e.id!=5&&e.id!=6&&e.id!=26&&e.id!=17&&e.id!=21&&e.id!=22&&e.id!=24)
            refuse("resource pipeline: unsupported original effect ID");
        if(e.id==21||e.id==22||e.id==24)continue; // actual no-word callback
        const auto dest=index(d,e.destination_tag),dc=format(d.attributes[dest].tag);
        if(e.id==1){if(dc!=3)refuse("resource pipeline: ADD step requires scalar destination");}
        else if(e.id==2||e.id==3||e.id==25){const auto source=index(d,e.source_tag),sc=format(d.attributes[source].tag);
            if(!((sc==1&&dc<=2)||(sc==3&&dc==3)))refuse("resource pipeline: source-selected MADD banks incompatible");
            scalar_zero|=sc==3;}
        else if(e.id==5||e.id==6||e.id==26){if(dc!=1&&dc!=3)refuse("resource pipeline: constant destination class unsupported by original callback");
            scalar_constants+=dc==3;vector_constants+=dc==1;}
        else if(e.id==17){if(e.destination_tag!=257||format(d.attributes[index(d,e.source_tag)].tag)!=3)
                refuse("resource pipeline: remap source or destination outside proved domain");
            scalar_zero=true;if(++remaps>1)refuse("resource pipeline: duplicate17 original context refusal");}
        else refuse("resource pipeline: unsupported original effect ID");
    }
    for(unsigned i=0;i<d.filter_count;++i)if(d.filters[i].id!=2||format(d.attributes[index(d,d.filters[i].source_tag)].tag)!=3)
        refuse("resource pipeline: filter ID or source bank unproved");
    // Original2F03B0 reserves F20..F31; fixed F12 consumes one of F0..F19.
    // Remap has four constant scalars and one working scalar; the shared
    // scalar zero is allocated once. This rejects exhaustion before writes.
    if(1+scalar_resources+scalar_constants+unsigned(scalar_zero)+5*remaps+d.filter_count>20||
       2+vector_resources+vector_constants>32)
        refuse("resource pipeline: original scalar or vector allocation capacity unproved");
}
inline void write(std::byte*to,const void*from,std::size_t size,const Writes&w){
    if(w.observe)w.observe(w.context,to,{static_cast<const std::byte*>(from),size});
    std::memcpy(to,from,size);
}
struct Prepared {Descriptor descriptor;std::array<Vector,13> vector_constants{};std::array<Bits,13> scalar_constants{},upper{},scale{};std::uint32_t prelude_flags{};VectorFlags vector;bool vector_written{};};
inline Prepared prepare(const Descriptor&d,const Streams&s){
    validate(d,s);Prepared p{};p.descriptor=d;
    for(unsigned i=0;i<d.effect_count;++i){const auto&e=d.effects[i];
        if(e.id==5||e.id==6||e.id==26){if(format(e.destination_tag)==1){
                for(unsigned lane=0;lane<4;++lane)p.vector_constants[i][lane]=ps2::fp::mul(e.constant[lane],d.vector_step);
                p.vector.arithmetic_result(p.vector_constants[i]);p.vector_written=true;
            }else p.scalar_constants[i]=ps2::fp::ee::mul(p.prelude_flags,e.constant[0],d.scalar_step);}
        if(e.id==17){p.upper[i]=ps2::fp::cvt_s_w(static_cast<Bits>(e.remap.upper));std::uint32_t producer_flags{};
            p.scale[i]=ps2::fp::ee::div(producer_flags,ps2::fp::ee::add(producer_flags,0xbd4ccccdu,
                ps2::fp::cvt_s_w(static_cast<Bits>(e.remap.upper)-static_cast<Bits>(e.remap.lower))),e.remap.extent);}
    }
    return p;
}
inline Result run(const Streams&s,const Prepared&p,Writes writes={}){
    const auto&d=p.descriptor;validate(d,s);Result result{};result.scalar_flags=p.prelude_flags;result.vector=p.vector;result.vector_written=p.vector_written;
    for(unsigned element=0;element<s.count;++element){
        std::array<Value,20> values{};Vector retained{};bool has_retained=false;unsigned retained_attribute=0;
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
                    for(unsigned lane=0;lane<4;++lane)to.vector[lane]=ps2::fp::add(to.vector[lane],p.vector_constants[i][lane]);
                    result.vector.arithmetic_result(to.vector);result.vector_written=true;
                }else to.scalar=ps2::fp::ee::add(result.scalar_flags,to.scalar,p.scalar_constants[i]);
            }else if(e.id==17)remap=static_cast<int>(i);
        }
        result.last_resources=values;result.last_retained=retained;result.element_observed=true;
        bool accepted=true;
        for(unsigned i=0;i<d.filter_count;++i){const auto&f=d.filters[i];result.condition_written=true;
            result.condition=ps2::fp::key(values[index(d,f.source_tag)].scalar)<=ps2::fp::key(f.limit);
            if(!result.condition){accepted=false;break;}}
        if(!accepted)continue;
        Bits remapped{};
        if(remap>=0){const auto&e=d.effects[remap];Bits value=values[index(d,e.source_tag)].scalar;
            value=ps2::fp::ee::sub(result.scalar_flags,value,e.remap.start);value=ps2::fp::max(value,0);value=ps2::fp::min(value,e.remap.extent);
            result.scalar_accumulator=ps2::fp::ee::add(result.scalar_flags,p.upper[remap],0);result.scalar_accumulator_written=true;
            remapped=ps2::fp::cvt_w_s(ps2::fp::ee::msub(result.scalar_flags,result.scalar_accumulator,value,p.scale[remap]));
            result.remap_word=remapped;result.remap_written=true;}
        if(has_retained){const auto&a=d.attributes[retained_attribute];const auto&stream=s.streams[a.stream];
            write(stream.output.data()+std::size_t(result.accepted)*stream.stride+(a.offset&~15u),retained.data(),16,writes);}
        for(unsigned i=0;i<d.attribute_count;++i){const auto&a=d.attributes[i];const auto&stream=s.streams[a.stream];
            auto*to=stream.output.data()+std::size_t(result.accepted)*stream.stride+a.offset;const auto cls=format(a.tag);
            if(cls<=2)write(to,values[i].vector.data(),16,writes);
            else if(cls==5)write(to,&values[i].integer,8,writes);
            else write(to,&values[i].scalar,4,writes);}
        if(remap>=0){const auto&a=d.attributes[index(d,257)];const auto&stream=s.streams[a.stream];
            write(stream.output.data()+std::size_t(result.accepted)*stream.stride+a.offset+12,&remapped,4,writes);}
        ++result.accepted;
    }
    return result;
}
inline Result run(const Streams&s,const Descriptor&d,Writes w={}){return run(s,prepare(d,s),w);}

struct Timing {std::uint64_t prelude{},before_filter{},accepted{};};
inline Timing timing(const Descriptor&d){
    if(d.attribute_count>20||d.effect_count>13||d.filter_count>1)
        refuse("resource pipeline: timing descriptor count unproved");
    Timing t{2,4+d.attribute_count+3*d.filter_count,3+d.attribute_count};bool zero=false;
    // Both original stream PREFs are budgeted even though the AOT omits
    // their host statements. Input advances cost2 and output advances cost2.
    for(unsigned i=0;i<d.attribute_count;++i)if(d.attributes[i].tag==259){++t.before_filter;++t.accepted;}
    for(unsigned i=0;i<d.effect_count;++i){const auto&e=d.effects[i];
        if(e.id==1)++t.before_filter;
        else if(e.id==2||e.id==3||e.id==25){t.before_filter+=2;zero|=format(e.source_tag)==3;}
        else if(e.id==5||e.id==6||e.id==26){t.prelude+=2;++t.before_filter;}
        else if(e.id==17){zero=true;t.prelude+=4;t.accepted+=7;}
        else if(e.id!=21&&e.id!=22&&e.id!=24)refuse("resource pipeline: timing effect unsupported");
    }
    t.prelude+=unsigned(zero)+d.filter_count;return t;
}
inline std::uint64_t cost(const Timing&t,std::uint32_t count,std::uint32_t accepted){
    if(accepted>count||count>0x7fffffffu)refuse("resource pipeline: timing count unproved");
    // Counts and all coefficients above are source-bounded; guard+return
    // cost4 plus cost2 for each true iteration guard are kept explicit.
    if(t.prelude>128||t.before_filter>128||t.accepted>128)
        refuse("resource pipeline: timing coefficient unproved");
    return t.prelude+4+std::uint64_t(count)*(t.before_filter+2)+std::uint64_t(accepted)*t.accepted;
}
struct TimedResult {Result result;std::int64_t remaining_budget{};};
inline TimedResult run_no_yield(const Streams&s,const Prepared&p,std::int64_t budget,Writes w={}){
    validate(p.descriptor,s);const auto t=timing(p.descriptor);const auto worst=cost(t,s.count,s.count);
    if(budget<0||std::uint64_t(budget)<worst)
        refuse("resource pipeline: insufficient budget for proved no-yield domain");
    auto result=run(s,p,w);return {result,budget-static_cast<std::int64_t>(cost(t,s.count,result.accepted))};
}
}
