#pragma once
// Owned-candidate integration seam. Include the build-time-generated known
// family kernel; no decoder, translator, worker or address-specific DLL table.
// This supports one proved code identity, not universal constructor closure.
#include "compiled_stub_family.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <stdexcept>
#include <tuple>

// Exact declaration from original runtime/src/kernel.hpp; no inferred ID.
namespace ps2::kernel { int current_thread(); }

namespace mka::emitted {
class FamilyDispatcher {
    struct Key {
        int thread;std::uint64_t stack;std::uint32_t object,base;
        bool operator<(const Key& other)const{return std::tie(thread,stack,object,base)<std::tie(other.thread,other.stack,other.object,other.base);}
    };
    struct Admission {
        known_clip48::Descriptor descriptor;
        std::uint32_t object;
        std::uint64_t yielded_state;
    };
    std::map<Key,Admission> yielded_;
    [[noreturn]] static void refuse(const char* why) {
        throw std::runtime_error(std::string("[emitted-family] ")+why);
    }
    static std::uint32_t word(std::uint32_t address) {
        if ((address&3u)||address>ps2::kRamSize-4)refuse("caller/object word outside RAM");
        std::uint32_t value;std::memcpy(&value,ps2::g_ram+address,4);return value;
    }
    static std::uint64_t fingerprint(const ps2::Cpu& c) {
        // A resumed invocation must retain its architectural state. Only the
        // scheduler-owned budget and PC/unwind representation may change.
        const auto* bytes=reinterpret_cast<const unsigned char*>(&c);
        std::uint64_t h=0xcbf29ce484222325ull;
        for(std::size_t i=0;i<sizeof c;++i){
            if((i>=offsetof(ps2::Cpu,pc)&&i<offsetof(ps2::Cpu,pc)+sizeof c.pc)||
               (i>=offsetof(ps2::Cpu,budget)&&i<offsetof(ps2::Cpu,budget)+sizeof c.budget))continue;
            h=(h^bytes[i])*0x100000001b3ull;
        }
        return h;
    }
    static void caller_source() {
        // Complete original call setup from LW target through JALR delay.
        constexpr std::array<std::uint32_t,21> words={
            0x8C8C004C,0x11800015,0x46006506,0x8E650028,0x8E640030,
            0x440AA000,0x8E68002C,0x8E620004,0x8E690034,0x00481818,
            0x70491018,0x00A33021,0x00823821,0x48AA0800,0x8E620394,
            0x8E640008,0x8E650010,0x00021080,0x8E6A0000,0x0180F809,0x01825821};
        for(unsigned i=0;i<words.size();++i)if(word(0x002E612Cu+i*4)!=words[i])refuse("original caller instruction changed");
    }
    static void object_identity(const ps2::Cpu& c,std::uint32_t object,std::uint32_t base) {
        if(object>ps2::kRamSize-0x398u||c.r[19].d[0]!=object)refuse("object extent/retained S3 unproved");
        if(word(object+0x4c)!=base||word(object+0x394)!=52)refuse("object code pointer/word count changed");
        if(c.r[31].d[0]!=0x002E6180u||c.r[12].d[0]!=base||c.r[11].d[0]!=base+208u)refuse("JALR target/continuation/constant-table relation unproved");
    }
    static void first_arguments(const ps2::Cpu& c,std::uint32_t object) {
        const std::uint32_t position=word(object+4),stride_a=word(object+0x2c),stride_b=word(object+0x34);
        const std::uint32_t out_a=word(object+0x28)+position*stride_a;
        const std::uint32_t out_b=word(object+0x30)+position*stride_b;
        if(c.r[4].d[0]!=word(object+8)||c.r[5].d[0]!=word(object+0x10)||
           c.r[6].d[0]!=out_a||c.r[7].d[0]!=out_b||c.r[8].d[0]!=stride_a||
           c.r[9].d[0]!=stride_b||c.r[10].d[0]!=word(object)||c.f[20]!=c.f[12]||c.vu0.vf[1].w[0]!=c.f[12])
            refuse("original caller parameter relation unproved");
    }
public:
    // Pure identity predicate: different cached/compiled families retain their
    // existing route. No caller checks, registration or CPU changes occur.
    static bool matches_known_shape(std::uint32_t base,std::uint32_t words){
        if(words!=52||(base&15u)||base>ps2::kRamSize-244)return false;
        for(unsigned i=0;i<52;++i){std::uint32_t value;std::memcpy(&value,ps2::g_ram+base+i*4,4);if(value!=known_clip48::pattern[i])return false;}
        return true;
    }
    // The future integration must invoke these at actual machine-state
    // replacement and guest-thread termination/reuse seams, respectively.
    // Those lifecycle hooks are NOT installed by this standalone component.
    void reset_after_machine_state_replacement(){yielded_.clear();}
    void retire_guest_thread(int id){for(auto i=yielded_.begin();i!=yielded_.end();)if(i->first.thread==id)i=yielded_.erase(i);else ++i;}
    // Default integration point: ordinary AOT miss, after existing modules.
    // A preferred-route integration must first use matches_known_shape, so
    // different families preserve their existing compiled route. No c.base
    // changes are required. Lifecycle hooks above remain integration gates.
    bool try_dispatch(ps2::Cpu& c) {
        if(c.r[31].d[0]!=0x002E6180u)return false;
        const auto object=c.r[19].w[0];
        if(object>ps2::kRamSize-0x398u)return false;
        const auto base=word(object+0x4c),pc=c.pc&~ps2::kUnwind;
        const Key key{ps2::kernel::current_thread(),c.r[29].d[0],object,base};
        if(!matches_known_shape(base,word(object+0x394))){
            const bool active=yielded_.find(key)!=yielded_.end();
            for(auto i=yielded_.begin();i!=yielded_.end();)if(i->first.base==base)i=yielded_.erase(i);else ++i;
            if(active&&pc==base+0x30u)refuse("emitted code changed after yield");
            return false;
        }
        caller_source();
        if(key.thread<=0||key.stack<16u||key.stack>ps2::kRamSize-0x1f0u||(key.stack&15u))refuse("guest task/caller stack context unproved");
        object_identity(c,object,base);
        if(pc!=base&&pc!=base+0x30u)refuse("unknown emitted entry");
        known_clip48::Descriptor descriptor{};
        if(pc==base){
            if(yielded_.find(key)!=yielded_.end())refuse("active invocation context collision");
            first_arguments(c,object);
            descriptor=known_clip48::bind(base,word(object+0x394));
        }else{
            const auto it=yielded_.find(key);
            if(it==yielded_.end()||it->second.object!=object)refuse("orphan emitted-family resume");
            try{known_clip48::bind(base,52);}catch(...){yielded_.erase(it);refuse("emitted code changed after yield");}
            if(it->second.yielded_state!=fingerprint(c)){yielded_.erase(it);refuse("retained resume state changed");}
            descriptor=it->second.descriptor;
        }
        known_clip48::run(c,descriptor);
        if(c.pc&ps2::kUnwind)yielded_[key]={descriptor,object,fingerprint(c)};
        else yielded_.erase(key);
        return true;
    }
};
}
