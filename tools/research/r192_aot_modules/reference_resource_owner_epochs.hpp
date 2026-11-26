#pragma once
// Reference-only host epochs attached to actual original source-label hooks.
// This is not native ownership credit, a guest allocator, or an opcode engine.
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"
#include "effective_resource_construction.hpp"
#include "reference_resource_word_equality.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mka::emitted::resource_owner_epochs {
struct Validation {
    bool admitted{},current_member{};
    std::uint64_t generation{};
    std::string reason;
};
struct Retire {void*context{};void(*invoke)(void*,unsigned cache){};};
struct SourceProgram {
    unsigned cache{},descriptor{},code{},bytes{},data_start_words{};
    std::uint64_t generation{};
    std::shared_ptr<const reference_resources::construction::CapturedPlanInput> input;
    bool copy_origin_verified{},final_pool_preparation_required=true;
    bool restored_paired_witness{};
};
struct CheckpointOwner {
    unsigned cache{},identity{},backing{},bytes{};
    std::vector<SourceProgram>programs;
};
struct Counters {std::uint64_t observed{},refused{},quarantined{},allocations{},commits{},identity_setters{};unsigned last_requested_identity{};};
class Registry {
    struct Key {int thread{};unsigned frame{};auto operator<=>(const Key&)const=default;};
    struct Produced {unsigned temporary{},code{},capacity{},words{},pool{};bool returned{};std::vector<unsigned char>bytes;std::shared_ptr<const reference_resources::construction::CapturedPlanInput>input;};
    struct Pending {
        unsigned cache{},temporary_list{};bool constructor{},allocation_return{},code_copy{},patched{},copied{},relocated{},bound{},quarantined{};
        std::map<unsigned,Produced>produced;
    };
    struct Owner {
        unsigned cache{},identity{},backing{},bytes{};std::uint64_t generation{};
        bool committed{},retiring{},snapshot_observation{},restored_paired_witness{};std::vector<std::shared_ptr<const SourceProgram>>programs;
    };
    std::map<unsigned,Owner>owners_;std::map<Key,Pending>pending_;
    std::uint64_t serial_{};bool poisoned_{};Retire retire_;
    Counters counters_;std::string last_refusal_;
    static bool range(std::uint64_t at,std::uint64_t bytes){return at<=ps2::kRamSize&&bytes<=ps2::kRamSize-at;}
    static unsigned word(unsigned at){unsigned v;if((at&3)||!range(at,4))throw std::runtime_error("owner epoch: RAM word bounds");std::memcpy(&v,ps2::g_ram+at,4);return v;}
    static std::int16_t half(unsigned at){std::int16_t v;if((at&1)||!range(at,2))throw std::runtime_error("owner epoch: RAM half bounds");std::memcpy(&v,ps2::g_ram+at,2);return v;}
    static auto source_input(unsigned list,unsigned descriptor){
        using namespace reference_resources;using namespace reference_resources::construction;
        std::vector<Attribute>attributes;bool sentinel=false;
        for(unsigned i=0;i<=20;++i){const auto at=std::uint64_t(list)+12ull*i;if(!range(at,4))throw std::runtime_error("owner epoch: source sentinel bounds");const auto tag=word(unsigned(at));if(tag==0xffffffff){sentinel=true;break;}if(i==20||!range(at,12))break;attributes.push_back({tag,word(unsigned(at)+4),word(unsigned(at)+8)});}
        if(!sentinel)throw std::runtime_error("owner epoch: original sentinel list outside bounded native domain");
        const auto effects_count=std::int32_t(word(descriptor+192)),filter_count=std::int32_t(word(descriptor+80));
        if(effects_count<0||effects_count>13)throw std::runtime_error("owner epoch: source effect count unproved");
        std::vector<Effect>effects;std::vector<RawFilter>filters;
        for(int i=0;i<effects_count;++i){const auto at=descriptor+208+48*i;Effect e{};e.id=unsigned(std::int32_t(half(at)));e.destination_tag=word(at+8);e.source_tag=word(at+16);for(unsigned k=0;k<3;++k)e.constant[k]=word(at+16+4*k);e.remap={std::int32_t(word(at+32)),std::int32_t(word(at+36)),word(at+24),word(at+28)};effects.push_back(e);}
        for(int i=0;i<std::min(filter_count,35);++i){const auto at=descriptor+84+24*i;filters.push_back({half(at),word(at+4),word(at+16),word(at+20)});}
        return std::make_shared<const CapturedPlanInput>(capture_plan_input(attributes,effects,filters,filter_count));
    }
    static bool inside(const Owner&o,std::uint64_t at,std::uint64_t bytes){return at>=o.backing&&range(at,bytes)&&at-o.backing<=o.bytes&&bytes<=o.bytes-(at-o.backing);}
    std::uint64_t next(){if(serial_==UINT64_MAX)throw std::runtime_error("owner epoch: generation exhausted");return ++serial_;}
    static Validation inspect(const Owner*o,unsigned cache,unsigned descriptor,unsigned code,unsigned code_bytes){
        Validation r;
        if(o&&o->retiring){r.generation=o->generation;r.reason="OWNER_RETIRING";return r;}
        if(!range(cache,784)||cache<16||(cache&3)){r.reason="CACHE_RAM_BOUNDS";return r;}
        if(word(cache)!=0x59fd90||word(cache+4)==0||(ps2::g_ram[cache+8]&1)){r.reason="CACHE_TABLE_IDENTITY_OR_RETIRING_FLAG";return r;}
        if(word(0x59fd90)!=0x12d870||word(0x59fd94)!=0x12d860||word(0x59fd98)!=0x12d830||word(0x59fd9c)!=0x210920){r.reason="PINNED_CACHE_TABLE_CHANGED";return r;}
        const unsigned owner=cache+64,count=word(owner+524),table=word(owner+528),backing=word(cache+60),pool=word(owner+636);
        // +520 is unrelated; +532 is flags, never a code length.
        if(!count||count>10||!range(table,4ull*count)||(table&3)||!range(descriptor,928)||word(descriptor+72)!=owner){r.reason="OWNER_DESCRIPTOR_MEMBERSHIP_BOUNDS";return r;}
        bool found=false;
        for(unsigned i=0;i<count;++i){const auto d=word(table+4*i);if(d!=std::uint64_t(table)+4ull*count+928ull*i){r.reason="ORIGINAL_DESCRIPTOR_TABLE_LAYOUT";return r;}found|=d==descriptor;}
        if(!found||!code_bytes||!range(code,code_bytes)||word(descriptor+76)!=code||code<pool||std::uint64_t(code)+code_bytes>table){r.reason="DESCRIPTOR_CODE_POOL_RELATION";return r;}
        r.current_member=true;
        if(!o){r.reason="UNPROVED_BACKING_ALLOCATION_EXTENT";return r;}
        r.generation=o->generation;
        if(o->snapshot_observation&&!o->bytes){r.reason="UNPROVED_BACKING_ALLOCATION_EXTENT";return r;}
        if(o->retiring||!o->committed){r.reason=o->retiring?"OWNER_RETIRING":"OWNER_TRANSACTION_UNCOMMITTED";return r;}
        if(o->identity!=word(cache+4)||o->backing!=backing||!inside(*o,table,4ull*count+928ull*count)||!inside(*o,code,code_bytes)||!inside(*o,pool,table-pool)){r.reason="OWNER_ALLOCATION_OR_IDENTITY_CHANGED";return r;}
        auto p=std::find_if(o->programs.begin(),o->programs.end(),[&](const auto&p){return p->descriptor==descriptor&&p->code==code&&code_bytes<=p->bytes;});
        if(p==o->programs.end()){r.reason="PROGRAM_NOT_SOURCE_COMMITTED";return r;}
        r.admitted=true;r.reason=o->restored_paired_witness?"PAIRED_SOURCE_PLAN_NEW_HOST_EPOCH":"SOURCE_COMMITTED_HOST_EPOCH";return r;
    }
    void revoke(unsigned cache){
        auto&owner=owners_[cache];owner.cache=cache;owner.retiring=true;owner.committed=false;owner.generation=next();
        // Publish revocation BEFORE a callback can reenter the adapter.
        if(retire_.invoke)retire_.invoke(retire_.context,cache);
    }
    static unsigned parent_frame(const ps2::Cpu&c,unsigned site){
        const auto sp=c.r[29].d[0];std::uint64_t frame=sp;
        if(site==0x2fa9d0){if(sp<1504)throw std::runtime_error("owner epoch: parent stack bounds");frame=sp-1504;}
        else if(site==0x2ed460)frame=sp+128;
        else if(site==0x212200)frame=sp+1296;
        if(frame>ps2::kRamSize)throw std::runtime_error("owner epoch: parent stack overflow");return unsigned(frame);
    }
    void commit(Pending&p,unsigned frame){
        auto at=owners_.find(p.cache);if(at==owners_.end())return;auto&o=at->second;
        if(p.quarantined||!p.constructor||!p.allocation_return||!p.code_copy||!p.patched||!p.copied||!p.relocated||!p.bound)throw std::runtime_error("owner epoch: source publication transaction incomplete");
        const unsigned owner=p.cache+64,count=word(owner+524),table=word(owner+528),pool=word(owner+636),temporary=p.temporary_list;
        if(!count||count>10||word(frame+1424)!=count||!inside(o,table,4ull*count+928ull*count)||!inside(o,pool,table-pool))return;
        unsigned cursor=0;std::vector<std::shared_ptr<const SourceProgram>>programs;
        for(unsigned i=0;i<count;++i){
            const auto length=word(frame+448+4*i),d=word(table+4*i);auto produced=p.produced.find(temporary+928*i);
            if(d!=table+4*count+928*i)return;
            if(!length)continue;
            if(produced==p.produced.end()||!produced->second.returned)return;const auto&q=produced->second;
            if(!q.input||!q.words||length!=((q.words+3)&~3u)||word(d+916)!=q.pool||word(d+72)!=owner||word(d+76)!=pool+4*cursor||!inside(o,pool+4ull*cursor,4ull*length))return;
            // Full copy origin was checked at2FAF4C. Later original binding
            // may change literal data; only source code/alignment BEFORE the
            // original data-start offset is immutable at publication.
            if(q.pool>q.words||std::memcmp(ps2::g_ram+pool+4*cursor,q.bytes.data(),4ull*q.pool))return;
            programs.push_back(std::make_shared<const SourceProgram>(SourceProgram{p.cache,d,pool+4*cursor,4*q.words,q.pool,o.generation,q.input,true,true}));cursor+=length;
        }
        if(programs.empty()||o.identity!=word(p.cache+4)||word(p.cache)!=0x59fd90||word(p.cache+60)!=o.backing)return;
        o.programs=std::move(programs);o.committed=true;o.retiring=false;++counters_.commits;
    }
    void authenticate_copy(Pending&p,unsigned frame,unsigned gp){
        auto at=owners_.find(p.cache);if(at==owners_.end()||p.quarantined||!p.allocation_return)throw std::runtime_error("owner epoch: copy lacks fresh allocation");
        const auto&o=at->second;const unsigned count=word(frame+1424),temporary=word(gp-21844u),pool=word(p.cache+700);
        if(!count||count>10)throw std::runtime_error("owner epoch: copy count unproved");
        unsigned cursor=0;
        for(unsigned i=0;i<count;++i){const auto length=word(frame+448+4*i);if(!length)continue;auto q=p.produced.find(temporary+928*i);
            if(q==p.produced.end()||!q->second.returned||!q->second.words||length!=((q->second.words+3)&~3u)||!inside(o,pool+4ull*cursor,4ull*length)||std::memcmp(ps2::g_ram+pool+4*cursor,q->second.bytes.data(),q->second.bytes.size()))throw std::runtime_error("owner epoch: original final copy mismatch");cursor+=length;}
        p.code_copy=true;
    }
    void quarantine(const ps2::Cpu&c,unsigned site,int thread)noexcept{
        try{
            unsigned cache=0;const Key key{thread,parent_frame(c,site)};auto p=pending_.find(key);
            if(p!=pending_.end()){p->second.quarantined=true;p->second.produced.clear();cache=p->second.cache;++counters_.quarantined;}
            if(site==0x212200&&range(c.r[21].w[0],784))cache=c.r[21].w[0];
            if(cache)revoke(cache);
        }catch(...){/* Revocation is recorded before invoking external callbacks. */}
    }
public:
    explicit Registry(Retire retire={}):retire_(retire){}
    bool poisoned()const{return poisoned_;}
    bool has_owner(unsigned cache)const{auto p=owners_.find(cache);return p!=owners_.end()&&(!p->second.snapshot_observation||p->second.retiring);}
    const Counters&counters()const{return counters_;}
    const std::string&last_refusal()const{return last_refusal_;}
    void reset(){const bool keep_poison=poisoned_;for(auto&[cache,o]:owners_)revoke(cache);owners_.clear();pending_.clear();poisoned_=keep_poison;next();}
    void begin_restore(){poisoned_=true;for(auto&[cache,o]:owners_)revoke(cache);owners_.clear();pending_.clear();next();}
    void end_restore(bool success){if(success){owners_.clear();pending_.clear();poisoned_=false;next();}}
    std::vector<CheckpointOwner>export_committed()const{
        if(poisoned_||!pending_.empty())throw std::runtime_error("owner epoch: checkpoint during restore or producer transaction");
        std::vector<CheckpointOwner>result;
        for(const auto&[cache,o]:owners_)if(o.committed&&!o.retiring){CheckpointOwner row{cache,o.identity,o.backing,o.bytes,{}};
            for(const auto&p:o.programs){if(!p->input||!p->copy_origin_verified||!validate(cache,p->descriptor,p->code,p->bytes).admitted)throw std::runtime_error("owner epoch: checkpoint live witness changed");row.programs.push_back(*p);}if(!row.programs.empty())result.push_back(std::move(row));}
        return result;
    }
    void import_paired_checkpoint(std::span<const CheckpointOwner>rows){
        // Called only after full game-state parsing and paired-bundle preflight.
        // The sealed exporter is the paired allocation witness; this is not a
        // claim that the loader observed the original allocation again.
        try{
        if(poisoned_||!owners_.empty()||!pending_.empty())throw std::runtime_error("owner epoch: paired import requires completed clean restore");
        if(rows.size()>4096)throw std::runtime_error("owner epoch: paired owner count bound");
        std::map<unsigned,Owner>staged;
        for(const auto&row:rows){if(!row.bytes||!range(row.backing,row.bytes)||staged.contains(row.cache)||row.programs.empty()||row.programs.size()>10)throw std::runtime_error("owner epoch: malformed paired owner");
            Owner o;o.cache=row.cache;o.identity=row.identity;o.backing=row.backing;o.bytes=row.bytes;o.generation=next();o.committed=true;o.restored_paired_witness=true;
            for(const auto&p:row.programs){if(!p.input||p.cache!=row.cache||!p.copy_origin_verified||!p.bytes||p.bytes%4||p.bytes>8000||!inside(o,p.code,p.bytes))throw std::runtime_error("owner epoch: malformed paired program");
                auto fresh=p;fresh.generation=o.generation;fresh.restored_paired_witness=true;o.programs.push_back(std::make_shared<const SourceProgram>(std::move(fresh)));}
            for(const auto&p:o.programs){if(!inspect(&o,row.cache,p->descriptor,p->code,p->bytes).admitted)throw std::runtime_error("owner epoch: paired member witness changed");
                std::vector<reference_resources::Bits>storage(p->bytes/4);std::memcpy(storage.data(),ps2::g_ram+p->code,p->bytes);
                const auto&input=*p->input;const auto result=reference_resources::resource_word_equality(input.original_attributes,input.original_effects,input.outcome.effective,storage,p->data_start_words,p->bytes/4);
                if(!result.equal)throw std::runtime_error("owner epoch: paired source equations mismatch");}
            staged.emplace(row.cache,std::move(o));}
        owners_.swap(staged);
        }catch(...){try{begin_restore();}catch(...){/* Poison is set before retirement callbacks. */}throw;}
    }
    Validation validate(unsigned cache,unsigned descriptor,unsigned code,unsigned code_bytes)const{
        if(poisoned_)return {false,false,0,"FAILED_OR_IN_PROGRESS_RESTORE_POISON"};
        try{auto at=owners_.find(cache);return inspect(at==owners_.end()?nullptr:&at->second,cache,descriptor,code,code_bytes);}catch(const std::exception&e){return {false,false,0,e.what()};}
    }
    Validation validate_region(unsigned cache,unsigned address,unsigned bytes,std::uint64_t generation)const{
        // Bounds come from the authenticated WHOLE backing allocation, not
        // returned program bytes. This permits proved adjacent literal words
        // without manufacturing an extent from RAM availability or padding.
        if(poisoned_)return {false,false,0,"FAILED_OR_IN_PROGRESS_RESTORE_POISON"};
        try{
            const auto at=owners_.find(cache);
            if(at==owners_.end())return {false,false,0,"UNPROVED_BACKING_ALLOCATION_EXTENT"};
            const auto&o=at->second;
            if(!generation||generation!=o.generation)return {false,false,o.generation,"OWNER_GENERATION_CHANGED"};
            if(o.programs.empty())return {false,false,o.generation,"OWNER_TRANSACTION_UNCOMMITTED"};
            const auto&p=*o.programs.front();auto result=inspect(&o,cache,p.descriptor,p.code,p.bytes);
            if(!result.admitted)return result;
            if(!bytes||!inside(o,address,bytes))return {false,true,o.generation,"REGION_OUTSIDE_AUTHENTICATED_BACKING"};
            return result;
        }catch(const std::exception&e){return {false,false,0,e.what()};}
    }
    Validation rehydrate(unsigned cache,unsigned descriptor,unsigned code,unsigned code_bytes){
        // A saved pointer, matching body or RAM bounds is not an allocation
        // witness. Until a source-proved heap extent reader exists, absent
        // fresh original allocation history falls back to compiled AOT.
        auto result=validate(cache,descriptor,code,code_bytes);
        if(!poisoned_&&result.current_member&&!owners_.contains(cache)){
            Owner o;o.cache=cache;o.identity=word(cache+4);o.backing=word(cache+60);o.generation=next();o.snapshot_observation=true;
            owners_[cache]=o;result.generation=o.generation;
        }
        return result;
    }
    std::shared_ptr<const SourceProgram>get_program(unsigned cache,unsigned descriptor)const{
        auto at=owners_.find(cache);if(poisoned_||at==owners_.end()||at->second.retiring||!at->second.committed)return {};
        for(const auto&p:at->second.programs)if(p->descriptor==descriptor&&validate(cache,descriptor,p->code,p->bytes).admitted)return p;
        return {};
    }
    void hook(ps2::Cpu&c,unsigned site,int thread){
        if(site==0x210780){revoke(c.r[4].w[0]);return;}
        // This sets the identity of a FUTURE original allocation; it is not
        // evidence that an existing cache died. Host epochs are independent.
        if(site==0x130ab0){++counters_.identity_setters;counters_.last_requested_identity=c.r[4].w[0];return;}
        if(poisoned_)return;
        {
            const Key key{thread,parent_frame(c,site)};
            if(site==0x2fa9d0){pending_[key]=Pending{};return;}
            auto current=pending_.find(key);
            // Every actual fresh backing allocation is tracked, even when an
            // unsupported producer was quarantined before allocation, or the
            // parent is another source path. It can never become snapshot
            // compatibility input merely because no native Plan was captured.
            if(site==0x212200){const auto cache=c.r[21].w[0],backing=c.r[19].w[0],bytes=c.r[17].w[0];
                if(!range(cache,784)||!bytes||!range(backing,bytes)||word(cache+60)!=backing||word(cache)!=0x59fd90||!word(cache+4))throw std::runtime_error("owner epoch: fresh allocation witness bounds");
                revoke(cache);Owner o;o.cache=cache;o.identity=word(cache+4);o.backing=backing;o.bytes=bytes;o.generation=next();owners_[cache]=o;++counters_.allocations;
                if(current!=pending_.end())current->second.cache=cache;return;
            }
            if(current==pending_.end())return;auto&p=current->second;
            if(site==0x211c50){p.constructor=c.r[8].w[0]==0x2fee20&&c.r[10].d[0]==0;return;}
            if(site==0x2ed3e0){
                Produced q;q.temporary=c.r[5].w[0];q.code=c.r[6].w[0];q.capacity=c.r[7].w[0];
                if(!range(q.temporary,928)||!q.capacity||q.capacity>2000||!range(q.code,4ull*q.capacity))throw std::runtime_error("owner epoch: producer capture bounds");
                if(p.quarantined)throw std::runtime_error("owner epoch: producer transaction quarantined");
                q.input=source_input(c.r[4].w[0],q.temporary);
                p.produced[q.temporary]=std::move(q);return;
            }
            if(site==0x2ed460){auto q=p.produced.find(c.r[18].w[0]);if(q==p.produced.end())return;const auto count=c.r[2].w[0];if(count>q->second.capacity)return;
                q->second.words=count;q->second.pool=word(q->second.temporary+916);q->second.returned=true;q->second.bytes.assign(ps2::g_ram+q->second.code,ps2::g_ram+q->second.code+4*count);return;}
            const unsigned frame=key.frame;
            if(site==0x2faf20){p.allocation_return=p.cache&&word(frame+1500)==p.cache;return;}
            if(site==0x2faf4c){authenticate_copy(p,frame,c.r[28].w[0]);return;}
            if(site==0x2fafb0){p.patched=true;return;}
            if(site==0x2fb078){p.copied=true;p.temporary_list=word(c.r[28].w[0]-21844u);return;}
            if(site==0x2fb0d0){p.relocated=true;return;}
            if(site==0x2fb158){p.bound=true;return;}
            if(site==0x2fb38c){commit(p,frame);return;}
            if(site==0x2fb3c4){if(p.cache){auto o=owners_.find(p.cache);if(o!=owners_.end()&&!o->second.committed)revoke(p.cache);}pending_.erase(current);return;}
        }
    }
    bool observe_hook(ps2::Cpu&c,unsigned site,int thread)noexcept{
        ++counters_.observed;
        try{hook(c,site,thread);return true;}
        catch(const std::exception&e){++counters_.refused;try{last_refusal_=e.what();}catch(...){}quarantine(c,site,thread);return false;}
        catch(...){++counters_.refused;try{last_refusal_="owner epoch: unknown hook failure";}catch(...){}quarantine(c,site,thread);return false;}
    }
};
}
