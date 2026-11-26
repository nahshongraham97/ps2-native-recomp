#pragma once
// Reference-only physical binding metadata derived from original2F0040/
// 2F01E0 and the fixed producer callbacks. No machine values, opcode decoder,
// emitted operation stream or compiler. The native resource core is separate.
#include "semantic_resource_pipeline.hpp"
#include <array>
#include <span>

namespace mka::emitted::reference_resources {
using namespace mka::emitted::resources;
struct Slot {int bank=-1,index=-1;bool present()const{return bank>=0;}};
struct Node {Bits tag{};Slot slot;int stream=-1;Bits offset{};};
struct EffectSlots {Slot constant,upper,scale,start,extent,work;Bits constant_offset{};};
struct Layout {
    std::array<Slot,20> resources{};Slot retained,zero;
    std::array<EffectSlots,13> effects{};std::array<Slot,4> filters{};
    std::array<Node,128> nodes{};unsigned node_count{};
    // Order: loads32,multiply36,data40,main28,filters44,accepted48,tail52.
    std::array<unsigned,7> intended{},capacity{16,8,20,64,12,24,16};
    unsigned data_words{},data_start_words{},total_words{},prelude_words{};
    bool intact()const{for(unsigned i=0;i<7;++i)if(intended[i]>capacity[i])return false;return true;}
    void require_intact()const{
        if(!intact())refuse("reference resource layout: source-supported partial emission remains unadmitted");
    }
};
inline Layout derive_layout(std::span<const Attribute>attributes,std::span<const Effect>effects,std::span<const Filter>filters){
    if(attributes.empty()||attributes.size()>20||effects.size()>13||filters.size()>4)
        refuse("reference resource layout: source count domain unproved");
    Layout p{};std::array<std::uint32_t,3>occupied{0,0xfff00000u,0};
    auto fixed=[&](Bits tag,int bank,int reg,int stream=-1,Bits offset=0){
        if(p.node_count>=128)refuse("reference resource layout: source node exhaustion remains unadmitted");
        occupied[bank<2?bank:2]|=1u<<reg;p.nodes[p.node_count++]={tag,{bank,reg},stream,offset};return Slot{bank,reg};
    };
    constexpr int registrations[][2]={{10,2},{2,2},{11,2},{0,2},{0,0},{4,2},{6,2},{8,2},{5,2},{7,2},{9,2},{12,1},{1,0}};
    for(auto&row:registrations)fixed(0xffffffffu,row[1],row[0]);
    auto allocate=[&](Bits tag,int bank,int stream=-1,Bits offset=0){
        auto mask=occupied[bank<2?bank:2];int reg=0;while(reg<32&&(mask&(1u<<reg)))++reg;
        if(reg==32)refuse("reference resource layout: source register exhaustion remains unadmitted");
        return fixed(tag,bank,reg,stream,offset);
    };
    auto find=[&](Bits tag){for(unsigned i=0;i<attributes.size();++i)if(attributes[i].tag==tag)return i;refuse("reference resource layout: resource absent");};
    unsigned retained=0;
    for(unsigned i=0;i<attributes.size();++i){const auto&a=attributes[i];const auto cls=format(a.tag);
        if(a.stream>=2||a.offset>32767||a.offset%width(cls))refuse("reference resource layout: stream/offset/alignment domain unproved");
        for(unsigned j=0;j<i;++j)if(attributes[j].tag==a.tag)refuse("reference resource layout: duplicate-tag source alias remains unadmitted");
        p.resources[i]=allocate(a.tag,cls<=2?0:cls-2,a.stream,a.offset);
    }
    for(const auto&a:attributes)if(a.tag==259){p.retained=allocate(358,2,a.stream,a.offset);retained=1;}
    unsigned used=0,intended_data=0,V=0,S=0,R=0,ops=0;bool zero=false;
    auto append=[&](){++intended_data;if(used>=20)return 0xffffffffu;return 4*used++;};
    auto constant=[&](int bank){const auto offset=append();return allocate(0,bank,2,offset);};
    auto require_zero=[&](){if(!zero){p.zero=allocate(0xffffffffu,1);zero=true;}};
    for(unsigned i=0;i<effects.size();++i){const auto&e=effects[i];auto&t=p.effects[i];
        if(e.id==21||e.id==22||e.id==24)continue;
        const auto di=find(e.destination_tag),dc=format(attributes[di].tag);
        if(e.id==1){if(dc!=3)refuse("reference resource layout: source ADD bank remains unadmitted");++ops;}
        else if(e.id==2||e.id==3||e.id==25){const auto sc=format(attributes[find(e.source_tag)].tag);
            if(!((sc==1&&dc<=2)||(sc==3&&dc==3)))refuse("reference resource layout: source MADD bank remains unadmitted");
            if(sc==3)require_zero();ops+=2;
        }else if(e.id==5||e.id==6||e.id==26){
            if(dc==1){++V;const unsigned aligned=(used+3)&~3u;if(aligned<=20)used=aligned;intended_data=(intended_data+3)&~3u;
                const auto first=append();append();append();t.constant=allocate(0,0,2,first);t.constant_offset=first;
            }else if(dc==3){++S;const auto offset=append();t.constant=allocate(0,1,2,offset);t.constant_offset=offset;}
            else refuse("reference resource layout: source constant class remains unadmitted");
            ++ops;
        }else if(e.id==17){
            if(e.destination_tag!=257||format(attributes[find(e.source_tag)].tag)!=3||++R>1)
                refuse("reference resource layout: source remap domain remains unadmitted");
            require_zero();t.upper=constant(1);t.scale=constant(1);t.start=constant(1);t.extent=constant(1);t.work=allocate(0xffffffffu,1);
        }else refuse("reference resource layout: unsupported original effect");
    }
    for(unsigned i=0;i<filters.size();++i){if(filters[i].id!=2||format(attributes[find(filters[i].source_tag)].tag)!=3)
            refuse("reference resource layout: source filter control remains unadmitted");p.filters[i]=constant(1);}
    const auto L=unsigned(attributes.size()),F=unsigned(filters.size());
    p.intended={V+S+4*R+F+unsigned(zero),V+S,intended_data,6+L+retained+ops,3*F,1+L+retained+6*R,8+R};
    unsigned code=0;for(unsigned i:{0u,1u,3u,4u,5u,6u})code+=p.intended[i];
    p.data_start_words=V?(code+3)&~3u:code;p.data_words=used;p.total_words=p.data_start_words+used;
    p.prelude_words=p.intended[0]+p.intended[1]+2;return p;
}
inline Layout derive_layout(const Descriptor&d){
    if(d.attribute_count>20||d.effect_count>13||d.filter_count>1)refuse("reference resource layout: native descriptor count unproved");
    return derive_layout({d.attributes.data(),d.attribute_count},{d.effects.data(),d.effect_count},{d.filters.data(),d.filter_count});
}
}
