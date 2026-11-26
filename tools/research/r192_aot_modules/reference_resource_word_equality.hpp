#pragma once
// Reference-only equality against the original producer's packing equations.
// This never emits, decodes or executes an instruction stream. A successful
// comparison is code identity only, not owner/lifetime or numerical admission.
#include "reference_resource_layout.hpp"
#include <algorithm>

namespace mka::emitted::reference_resources {
struct WordEquality {
    bool equal{};
    unsigned compared{},code_words{},data_start_words{},data_words{},total_words{};
    unsigned mismatch_index{};Bits expected{},actual{};
};
inline WordEquality resource_word_equality(std::span<const Attribute> attributes,
        std::span<const Effect> effects,std::span<const Filter> filters,
        std::span<const Bits> storage,unsigned data_start,unsigned total_words){
    const auto p=derive_layout(attributes,effects,filters);
    // These remain the explicit bounded descriptor domain of derive_layout.
    // In particular duplicate tags/stream2/unaligned offsets are NOT admitted.
    std::array<unsigned,7> length{},start{};
    unsigned code=0;
    for(unsigned i:{0u,1u,3u,4u,5u,6u}){
        length[i]=std::min(p.intended[i],p.capacity[i]);start[i]=code;code+=length[i];
    }
    const bool vector_data=std::any_of(effects.begin(),effects.end(),[&](const Effect&e){
        return (e.id==5||e.id==6||e.id==26)&&format(e.destination_tag)==1;
    });
    const unsigned pool=vector_data?(code+3)&~3u:code;
    WordEquality result{};result.code_words=code;result.data_start_words=pool;
    result.data_words=p.data_words;result.total_words=pool+p.data_words;
    if(data_start!=pool||total_words!=result.total_words||storage.size()<total_words)return result;
    bool same=true;std::array<unsigned,7> used{};
    auto compare=[&](unsigned block,Bits expected){
        // Original append failure leaves the used count unchanged. A skipped
        // append is not a compare and creates no invented executable word.
        if(used[block]>=p.capacity[block])return;
        const unsigned at=start[block]+used[block]++;
        if(at>=storage.size())refuse("resource word equality: comparison bounds");
        ++result.compared;
        if(storage[at]!=expected&&same){same=false;result.mismatch_index=at;result.expected=expected;result.actual=storage[at];}
    };
    auto reg=[](Slot s){if(!s.present()||s.index<0||s.index>31)refuse("resource word equality: register unproved");return unsigned(s.index);};
    auto fpu=[&](unsigned fn,Slot d,Slot s,Slot t){
        if(d.bank!=s.bank||s.bank!=t.bank||d.bank<0||d.bank>1)refuse("resource word equality: arithmetic banks unproved");
        return (d.bank==0?0x4be00000u:0x46000000u)|(reg(t)<<16)|(reg(s)<<11)|(reg(d)<<6)|fn;
    };
    auto accumulator=[&](Slot d,Slot zero){
        if(d.bank!=zero.bank||d.bank<0||d.bank>1)refuse("resource word equality: accumulator banks unproved");
        return (d.bank==0?0x4be0003cu:0x46000018u)|(reg(zero)<<16)|(reg(d)<<11);
    };
    auto memory=[&](Slot s,unsigned base,Bits offset,bool store){
        constexpr Bits loads[]={0xd8000000u,0xc4000000u,0x8c000000u,0xdc000000u};
        constexpr Bits stores[]={0xf8000000u,0xe4000000u,0xac000000u,0xfc000000u};
        if(s.bank<0||s.bank>3||base>31)refuse("resource word equality: transfer bank unproved");
        return (store?stores[s.bank]:loads[s.bank])|(base<<21)|(reg(s)<<16)|(offset&65535u);
    };
    auto find=[&](Bits tag){for(unsigned i=0;i<attributes.size();++i)if(attributes[i].tag==tag)return i;refuse("resource word equality: resource absent");};
    auto branch=[&](unsigned from,unsigned target){
        const auto delta=std::int64_t(target)-std::int64_t(from)-1;
        if(delta< -32767||delta>32767)refuse("resource word equality: source branch range");
        return Bits(delta)&65535u;
    };
    // Allocation order is original append order, including shared scalar zero
    // and omitted partial constant loads. Scratch working nodes have no load.
    for(unsigned i=13;i<p.node_count;++i){const auto&n=p.nodes[i];
        if(n.stream==2)compare(0,memory(n.slot,11,n.offset,false));
        else if(p.zero.present()&&n.slot.bank==p.zero.bank&&n.slot.index==p.zero.index)
            compare(0,fpu(1,n.slot,n.slot,n.slot));
    }
    for(unsigned i=0;i<effects.size();++i){const auto&e=effects[i];const auto&s=p.effects[i].constant;
        if(e.id==5||e.id==6||e.id==26)compare(1,fpu(s.bank==0?0x18:2,s,s,{s.bank,s.bank==0?1:12}));
    }
    const unsigned guard=start[6]+unsigned(p.intended[6]==9)+2,body=start[3]+2;
    compare(3,0x10000000u|branch(start[3],guard));compare(3,0x0000102du);
    for(unsigned i=0;i<attributes.size();++i){const auto&a=attributes[i];compare(3,memory(p.resources[i],4+a.stream,a.offset,false));}
    unsigned retained=0;
    for(unsigned i=0;i<attributes.size();++i)if(attributes[i].tag==259){retained=i;
        compare(3,0x78000000u|((4+attributes[i].stream)<<21)|(reg(p.retained)<<16)|(attributes[i].offset&65535u));break;}
    compare(3,0xcc800050u);compare(3,0xcca00050u);
    for(unsigned i=0;i<effects.size();++i){const auto&e=effects[i];
        if(e.id==17||e.id==21||e.id==22||e.id==24)continue;
        const auto d=p.resources[find(e.destination_tag)];
        if(e.id==1)compare(3,fpu(0,d,d,{1,12}));
        else if(e.id==2||e.id==3||e.id==25){const auto s=p.resources[find(e.source_tag)];
            compare(3,accumulator(d,d.bank==0?Slot{0,0}:p.zero));
            compare(3,fpu(d.bank==0?8:0x1c,d,s,{d.bank,d.bank==0?1:12}));
        }else if(e.id==5||e.id==6||e.id==26)compare(3,fpu(d.bank==0?0x28:0,d,d,p.effects[i].constant));
        else refuse("resource word equality: unsupported source effect");
    }
    compare(3,0x0088202du);compare(3,0x00a9282du);
    for(unsigned i=0;i<filters.size();++i){const auto s=p.resources[find(filters[i].source_tag)];
        compare(4,0x46000036u|(reg(p.filters[i])<<16)|(reg(s)<<11));
        const auto at=start[4]+used[4];compare(4,0x45000000u|branch(at,guard));compare(4,0);
    }
    if(p.retained.present())compare(5,0x7c000000u|((6+attributes[retained].stream)<<21)|(reg(p.retained)<<16)|(attributes[retained].offset&65535u));
    for(unsigned i=0;i<effects.size();++i)if(effects[i].id==17){const auto&t=p.effects[i];const auto s=p.resources[find(effects[i].source_tag)];
        compare(5,fpu(1,t.work,s,t.start));compare(5,fpu(0x28,t.work,t.work,p.zero));
        compare(5,fpu(0x29,t.work,t.work,t.extent));compare(5,accumulator(t.upper,p.zero));
        compare(5,fpu(0x1d,t.work,t.work,t.scale));compare(5,0x46000024u|(reg(t.work)<<11)|(reg(t.work)<<6));
    }
    compare(5,0x20420001u);
    for(unsigned i=0;i<attributes.size();++i){const auto&a=attributes[i];compare(5,memory(p.resources[i],6+a.stream,a.offset,true));}
    for(unsigned i=0;i<effects.size();++i)if(effects[i].id==17){const auto&a=attributes[find(257)];compare(6,memory(p.effects[i].work,6+a.stream,a.offset+12,true));}
    compare(6,0x00c8302du);compare(6,0x00e9382du);
    compare(6,0x15400000u|branch(guard,body));compare(6,0x214affffu);
    compare(6,0x03e00008u);compare(6,0);compare(6,0);compare(6,0);
    for(unsigned i:{0u,1u,3u,4u,5u,6u})if(used[i]!=length[i])refuse("resource word equality: packing length contradiction");
    result.equal=same&&result.compared==code;return result;
}
}
