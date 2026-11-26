#pragma once
// Disposable R192 reference route. This does not declare an accepted native
// scheduler, engine owner, general ISA backend, or universal emitted family.
#include "reference_family_projection.hpp"
#include "ps2/system.hpp"
#include "kernel.hpp"
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <tuple>

namespace mka::emitted::reference {
struct Contract {
    std::array<std::uint32_t,52> code;
    std::array<std::uint32_t,21> caller;
    std::array<std::uint8_t,192> effect_shape;
    std::array<std::uint8_t,20> filter_shape;
};
class Adapter {
    struct Key {
        std::uint64_t epoch,stack;
        int thread;
        std::uint32_t object,base;
        bool operator<(const Key&other)const{return std::tie(epoch,thread,stack,object,base)<std::tie(other.epoch,other.thread,other.stack,other.object,other.base);}
    };
    struct Invocation {
        FrameInputs frame;
        semantic::ClipContinuation continuation;
        std::uint32_t owner,attributes,step;
        bool active{},cancelled{};
        Invocation(const ps2::Cpu&c,semantic::Streams streams,semantic::Pipeline pipeline,std::uint32_t owner_,std::uint32_t attributes_):
            frame(FrameInputs::capture(c)),continuation(streams,pipeline),owner(owner_),attributes(attributes_),step(pipeline.step){}
    };
    const Contract&contract_;
    semantic::Writes writes_;
    std::map<Key,std::unique_ptr<Invocation>> pending_;
    std::uint64_t epoch_{};
    std::uint64_t entries_{},resumes_{},completed_{},suspensions_{},callbacks_{},other_families_{};
    [[noreturn]] static void refuse(const char*reason){
        std::fprintf(stderr,"[semantic-reference] REFUSED %s\n",reason);
        throw std::runtime_error(std::string("semantic reference: ")+reason);
    }
    static std::uint32_t word(std::uint32_t address){
        if(address>ps2::kRamSize-4||(address&3u))refuse("word extent/alignment unproved");
        std::uint32_t value;std::memcpy(&value,ps2::g_ram+address,4);return value;
    }
    static std::uint16_t half(std::uint32_t address){
        if(address>ps2::kRamSize-2||(address&1u))refuse("halfword extent/alignment unproved");
        std::uint16_t value;std::memcpy(&value,ps2::g_ram+address,2);return value;
    }
    bool shape(std::uint32_t base,std::uint32_t words)const{
        return words==52&&!(base&15u)&&base<=ps2::kRamSize-244&&!std::memcmp(ps2::g_ram+base,contract_.code.data(),208);
    }
    void caller_source()const{
        if(std::memcmp(ps2::g_ram+0x2e612c,contract_.caller.data(),84))refuse("original caller words changed");
    }
    void descriptor_shape(std::uint32_t object)const{
        if(word(object+192)!=4||word(object+80)!=1)refuse("effect/filter cardinality unproved");
        for(unsigned i=0;i<192;++i){
            const unsigned record=i/48,offset=i%48;
            const bool numeric=(record==1&&offset>=16&&offset<28)||(record==2&&offset>=16&&offset<20)||(record==3&&offset>=24&&offset<40);
            if(!numeric&&ps2::g_ram[object+208+i]!=contract_.effect_shape[i])refuse("descriptor opaque binding unproved");
        }
        if(std::memcmp(ps2::g_ram+object+84,contract_.filter_shape.data(),20))refuse("filter opaque binding unproved");
    }
    static std::pair<std::uint32_t,std::uint32_t> resources(std::uint32_t object){
        const auto owner=word(object+72);
        if(owner>ps2::kRamSize-544||word(owner+536)!=5)refuse("resource owner/count unproved");
        const auto attributes=word(owner+540);
        constexpr std::uint32_t expected[]={256,0,0,259,0,16,257,0,32,258,0,12,769,1,0};
        if(attributes>ps2::kRamSize-sizeof expected||(attributes&3u)||std::memcmp(ps2::g_ram+attributes,expected,sizeof expected))refuse("resource layout unproved");
        return {owner,attributes};
    }
    static semantic::Pipeline pipeline(const ps2::Cpu&c,std::uint32_t object,std::uint32_t base){
        semantic::Pipeline p{};p.effect_count=4;p.filter_count=1;p.filter_id=2;p.filter_tag=769;p.filter_limit=word(object+104);p.step=c.f[12];
        for(unsigned i=0;i<4;++i){const auto record=object+208+i*48;auto&e=p.effects[i];e.id=half(record);e.destination_tag=word(record+8);
            if(e.id==5)for(unsigned lane=0;lane<3;++lane)e.constant[lane]=word(record+16+lane*4);
            if(e.id==6)e.constant[0]=word(record+16);
            if(e.id==17){e.source_tag=word(record+16);e.remap.start=word(record+24);e.remap.extent=word(record+28);e.remap.upper=static_cast<std::int32_t>(word(record+32));e.remap.lower=static_cast<std::int32_t>(word(record+36));}
        }
        p.effects[1].constant[3]=p.effects[2].constant[0];semantic::validate(p);
        if(c.vu0.vf[1].w[0]!=p.step||c.f[20]!=p.step)refuse("scalar/vector caller step relation unproved");
        const auto&r=p.effects[3].remap;std::uint32_t producer_flags=0;
        const std::uint32_t constants[]={p.effects[1].constant[0],p.effects[1].constant[1],p.effects[1].constant[2],p.effects[2].constant[0],
            ps2::fp::cvt_s_w(static_cast<std::uint32_t>(r.upper)),ps2::fp::ee::div(producer_flags,ps2::fp::ee::add(producer_flags,0xbd4ccccdu,ps2::fp::cvt_s_w(std::uint32_t(r.upper)-std::uint32_t(r.lower))),r.extent),r.start,r.extent,p.filter_limit};
        if(std::memcmp(ps2::g_ram+base+208,constants,sizeof constants))refuse("descriptor/original constant-block coherence unproved");
        return p;
    }
    static semantic::Streams streams(const ps2::Cpu&c){
        semantic::Streams streams{};
        if(c.r[10].d[0]>0x7fffffffu)refuse("count domain unproved");
        streams.count=c.r[10].w[0];streams.stride_a=c.r[8].d[0];streams.stride_b=c.r[9].d[0];
        if(!streams.count)return streams;
        auto span=[&](unsigned reg){const auto address=c.r[reg].d[0];if(address>=ps2::kRamSize)refuse("non-RAM stream unproved");return std::span(reinterpret_cast<std::byte*>(ps2::g_ram+address),ps2::kRamSize-address);};
        streams.input_a=span(4);streams.input_b=span(5);streams.output_a=span(6);streams.output_b=span(7);return streams;
    }
    static bool overlaps(std::uint64_t begin,std::uint64_t size,std::uint32_t zone,std::uint32_t extent){return begin<std::uint64_t(zone)+extent&&zone<begin+size;}
    static void output_isolation(const ps2::Cpu&c,std::uint32_t object,std::uint32_t base,std::uint32_t owner,std::uint32_t attributes){
        const auto n=c.r[10].d[0];if(!n)return;
        for(unsigned reg:{6u,7u}){
            const auto stride=c.r[reg==6?8:9].d[0],begin=c.r[reg].d[0];const unsigned width=reg==6?48u:4u;
            if(stride>ps2::kRamSize||n-1>(ps2::kRamSize-width)/(stride?stride:1))refuse("output arithmetic bounds unproved");
            const auto size=(n-1)*stride+width;
            if(begin>ps2::kRamSize||size>ps2::kRamSize-begin)refuse("output extent unproved");
            for(auto zone:std::array<std::pair<std::uint32_t,std::uint32_t>,6>{{{object,928},{base,244},{owner,544},{attributes,60},{c.r[29].w[0],496},{0x2e60e0,0x2e6dec-0x2e60e0}}})
                if(overlaps(begin,size,zone.first,zone.second))refuse("output aliases code/descriptor/frame identity");
        }
    }
    static void first_arguments(const ps2::Cpu&c,std::uint32_t object){
        const auto position=word(object+4),stride_a=word(object+44),stride_b=word(object+52);
        const auto output_a=word(object+40)+position*stride_a,output_b=word(object+48)+position*stride_b;
        auto extended=[](std::uint32_t value){return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(value)));};
        if(c.r[4].d[0]!=extended(word(object+8))||c.r[5].d[0]!=extended(word(object+16))||c.r[6].d[0]!=extended(output_a)||c.r[7].d[0]!=extended(output_b)||c.r[8].d[0]!=extended(stride_a)||c.r[9].d[0]!=extended(stride_b)||c.r[10].d[0]!=extended(word(object)))refuse("original caller argument relation unproved");
    }
    struct GuardBridge {Adapter&adapter;Invocation&invocation;ps2::Cpu&cpu;const Key&key;};
    static semantic::GuardDecision poll(void*context,semantic::GuardObservation,std::int32_t&budget){
        auto&bridge=*static_cast<GuardBridge*>(context);auto&c=bridge.cpu;auto&invocation=bridge.invocation;
        project(c,invocation.frame,invocation.continuation);
        if(c.budget!=budget)refuse("guard budget projection disagrees");
        const ps2::Cpu before=c; // Reference-only call frame, never native state.
        ++bridge.adapter.callbacks_;const bool unwind=ps2::preempt(c);budget=c.budget;
        if(invocation.cancelled)refuse("invocation retired during original scheduler callback");
        ps2::Cpu retained=c;retained.budget=before.budget;
        if(std::memcmp(&retained,&before,sizeof before))refuse("original callback changed private CPU beyond proved budget");
        const auto object=bridge.key.object,base=bridge.key.base;
        if(word(object+76)!=base||!bridge.adapter.shape(base,word(object+916))){invocation.cancelled=true;refuse("code identity changed inside original callback");}
        bridge.adapter.caller_source();bridge.adapter.descriptor_shape(object);
        const auto [owner,attributes]=resources(object);
        if(owner!=invocation.owner||attributes!=invocation.attributes){invocation.cancelled=true;refuse("descriptor owner changed inside original callback");}
        return unwind?semantic::GuardDecision::suspend:semantic::GuardDecision::keep_running;
    }
    void retire_if(bool all,int thread){
        for(auto i=pending_.begin();i!=pending_.end();){
            if(all||i->first.thread==thread){if(i->second->active){i->second->cancelled=true;++i;}else i=pending_.erase(i);}else ++i;
        }
    }
public:
    explicit Adapter(const Contract&contract,semantic::Writes writes={}):contract_(contract),writes_(writes){static_assert(sizeof(ps2::Cpu)==1456);}
    Adapter(const Adapter&)=delete;Adapter& operator=(const Adapter&)=delete;
    ~Adapter(){std::fprintf(stderr,"[semantic-reference] summary entries=%llu resumes=%llu completed=%llu suspended=%llu original_preempt_calls=%llu other_original_families=%llu pending=%zu\n",entries_,resumes_,completed_,suspensions_,callbacks_,other_families_,pending_.size());}
    void reset_after_machine_state_replacement(){
        if(epoch_==std::numeric_limits<std::uint64_t>::max())refuse("state epoch exhausted");retire_if(true,0);++epoch_;
    }
    void retire_thread(int id){retire_if(false,id);}
    std::size_t pending()const{return pending_.size();}
    bool try_dispatch(ps2::Cpu&c){
        if(c.r[31].d[0]!=0x2e6180u){
            for(const auto&[key,invocation]:pending_)if(key.thread==ps2::kernel::current_thread()&&key.stack==c.r[29].d[0]&&key.object==c.r[19].w[0]&&((c.pc&~ps2::kUnwind)==key.base||(c.pc&~ps2::kUnwind)==key.base+0x30u))refuse("retained return continuation changed");
            return false;
        }
        const auto object=c.r[19].w[0],pc=c.pc&~ps2::kUnwind;
        if(object>ps2::kRamSize-928||(object&15u))return false;
        const auto base=word(object+76);const int thread=ps2::kernel::current_thread();
        for(const auto&[prior,invocation]:pending_)if(prior.epoch==epoch_&&prior.thread==thread&&prior.stack==c.r[29].d[0]&&prior.object==object&&prior.base!=base&&(pc==prior.base+0x30u||pc==base))refuse("object code pointer changed during active invocation");
        const Key key{epoch_,c.r[29].d[0],thread,object,base};auto found=pending_.find(key);
        if(!shape(base,word(object+916))){
            if(found!=pending_.end()){found->second->cancelled=true;refuse("code identity changed during suspension");}
            if(pc==base){++other_families_;if(other_families_<=5)std::fprintf(stderr,"[semantic-reference] UNADMITTED original compiled family pc=%08X\n",pc);}return false;
        }
        if(pc!=base&&pc!=base+0x30u)return false;
        if(&c!=&ps2::g_cpu)refuse("global CPU/timebase binding unproved");
        if(thread<=0||key.stack<16||key.stack>ps2::kRamSize-496||(key.stack&15u)||c.r[19].d[0]!=object||c.r[12].d[0]!=base||c.r[11].d[0]!=base+208u)refuse("thread/frame/object/target relation unproved");
        caller_source();descriptor_shape(object);const auto [owner,attributes]=resources(object);
        if(pc==base){
            if(found!=pending_.end())refuse("active invocation context collision");
            first_arguments(c,object);auto p=pipeline(c,object,base);auto s=streams(c);output_isolation(c,object,base,owner,attributes);
            found=pending_.emplace(key,std::make_unique<Invocation>(c,s,p,owner,attributes)).first;++entries_;
        }else{
            if(found==pending_.end())refuse("orphan resume requires proved typed rehydration");
            if(found->second->cancelled||found->second->active||found->second->continuation.phase()!=semantic::ContinuationPhase::suspended)refuse("resume lifecycle unproved");
            if(found->second->owner!=owner||found->second->attributes!=attributes)refuse("descriptor owner lifetime changed");
            if(c.f[12]!=found->second->step||c.r[8].d[0]!=found->second->frame.stride_a||c.r[9].d[0]!=found->second->frame.stride_b)refuse("live step/stride argument changed during suspension");
            // Numeric fields/constants can change externally: current source
            // resume retains its prepared values, so never rebind them here.
            if(!parked_state_matches(c,found->second->frame,found->second->continuation))refuse("retained scratch/shared VU changed during suspension");++resumes_;
        }
        auto&invocation=*found->second;invocation.active=true;GuardBridge bridge{*this,invocation,c,key};
        try{
            const auto phase=invocation.continuation.run(c.budget,{&bridge,&poll},writes_);invocation.active=false;
            if(invocation.cancelled)refuse("invocation retired while active");project(c,invocation.frame,invocation.continuation);
            if(phase==semantic::ContinuationPhase::complete){c.pc=0x2e6180u;++completed_;pending_.erase(key);}
            else if(phase==semantic::ContinuationPhase::suspended){c.pc=(base+0x30u)|ps2::kUnwind;++suspensions_;}
            else refuse("continuation phase unproved");
        }catch(...){invocation.active=false;if(invocation.cancelled)pending_.erase(key);throw;}
        return true;
    }
};
// Implemented once by the disposable builder with an authenticated contract.
bool preferred_family(ps2::Cpu&c);
void reset_family_after_state_replacement();
void retire_family_thread(int id);
bool family_pending();
}
