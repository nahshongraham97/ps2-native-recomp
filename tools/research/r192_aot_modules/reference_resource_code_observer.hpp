#pragma once
// Read-only reference diagnostic. Candidate descriptors are hypotheses until
// every source code word matches. Even a match admits NO execution or lease.
#include "reference_resource_word_equality.hpp"
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace mka::emitted::reference_resources {
class ResourceCodeObserver {
    struct Count {std::uint64_t calls{};unsigned code_words{},first_base{},first_object{};bool equal{},intact{};std::string reason;};
    std::map<std::string,Count> rows_;std::uint64_t calls_{},resumes_{},observation_errors_{},resets_{};
    static unsigned word(unsigned at){if((at&3)||at>ps2::kRamSize-4)refuse("observation word bounds");unsigned v;std::memcpy(&v,ps2::g_ram+at,4);return v;}
    static unsigned half(unsigned at){if((at&1)||at>ps2::kRamSize-2)refuse("observation half bounds");unsigned short v;std::memcpy(&v,ps2::g_ram+at,2);return v;}
    static std::string shape(const Descriptor&d){
        std::string s="a";auto add=[&](unsigned n){s+=std::to_string(n)+":";};
        for(unsigned i=0;i<d.attribute_count;++i){const auto&a=d.attributes[i];add(a.tag);add(a.stream);add(a.offset);}
        s+="e";for(unsigned i=0;i<d.effect_count;++i){const auto&e=d.effects[i];add(e.id);add(e.destination_tag);add(e.source_tag);}
        s+="f";for(unsigned i=0;i<d.filter_count;++i){add(d.filters[i].id);add(d.filters[i].source_tag);}return s;
    }
    static void json_string(const std::string&s){std::fputc('"',stderr);for(unsigned char c:s){if(c=='"'||c=='\\')std::fputc('\\',stderr);if(c>=32)std::fputc(c,stderr);}std::fputc('"',stderr);}
public:
    void note_reset(){++resets_;}
    std::uint64_t observed_entries()const{return calls_;}
    void inspect(const ps2::Cpu&c)noexcept{
        // AFTER the unchanged admitted adapter declines; this observer never
        // runs a replacement, mutates Cpu/RAM, or alters AOT fallback choice.
        if(c.r[31].d[0]!=0x2e6180u||c.r[19].d[0]<16||c.r[19].d[0]>ps2::kRamSize-928)return;
        try{
            const unsigned object=c.r[19].w[0],base=word(object+76),pc=c.pc&~ps2::kUnwind;
            // Cpu.base identifies the selected image window, which can hold
            // many entries; it is not the descriptor's function address.
            if(pc!=base){++resumes_;return;}
            ++calls_;Descriptor d{};std::string key;Count measured{};measured.first_base=base;measured.first_object=object;
            try{
                const unsigned owner=word(object+72);if(owner<16||owner>ps2::kRamSize-544)refuse("candidate owner bounds");
                d.attribute_count=word(owner+536);const auto attributes=word(owner+540);d.effect_count=word(object+192);d.filter_count=word(object+80);
                if(!d.attribute_count||d.attribute_count>20||d.effect_count>13||d.filter_count>1||attributes>ps2::kRamSize-12*d.attribute_count||(attributes&3))refuse("candidate descriptor bounds");
                for(unsigned i=0;i<d.attribute_count;++i)d.attributes[i]={word(attributes+12*i),word(attributes+12*i+4),word(attributes+12*i+8)};
                for(unsigned i=0;i<d.effect_count;++i){const auto at=object+208+48*i;auto&e=d.effects[i];e.id=half(at);e.destination_tag=word(at+8);if(e.id==2||e.id==3||e.id==25||e.id==17)e.source_tag=word(at+16);}
                for(unsigned i=0;i<d.filter_count;++i){const auto at=object+84+24*i;if(word(at+16))refuse("candidate filter control");d.filters[i]={half(at),word(at+4),0};}
                key=shape(d);const auto p=derive_layout(d);measured.intact=p.intact();
                unsigned code=0;for(unsigned i:{0u,1u,3u,4u,5u,6u})code+=std::min(p.intended[i],p.capacity[i]);
                bool vectors=false;for(unsigned i=0;i<d.effect_count;++i)if((d.effects[i].id==5||d.effects[i].id==6||d.effects[i].id==26)&&format(d.effects[i].destination_tag)==1)vectors=true;
                const unsigned derived_total=(vectors?(code+3)&~3u:code)+p.data_words;
                if((base&15)||base>ps2::kRamSize||std::uint64_t(derived_total)*4>ps2::kRamSize-base)refuse("candidate code RAM bounds");
                // derived_total is NOT an authenticated allocation extent. It
                // only bounds a read from the stable reference RAM allocation.
                std::vector<Bits> original(derived_total);std::memcpy(original.data(),ps2::g_ram+base,std::size_t(derived_total)*4);
                const auto result=resource_word_equality({d.attributes.data(),d.attribute_count},{d.effects.data(),d.effect_count},{d.filters.data(),d.filter_count},original,word(object+916),derived_total);
                measured.equal=result.equal;measured.code_words=result.code_words;measured.reason=result.equal?"code_identity_only":"code_or_metadata_mismatch";
                if(!result.equal)measured.reason+=":"+std::to_string(result.mismatch_index)+":"+std::to_string(result.expected)+":"+std::to_string(result.actual);
            }catch(const std::exception&e){measured.reason=e.what();if(key.empty())key="unparsed";}
            key+='|';key+=measured.reason;auto [at,added]=rows_.try_emplace(key,measured);++at->second.calls;
        }catch(...){++observation_errors_;} // Diagnostics never change game control flow.
    }
    ~ResourceCodeObserver(){
        std::uint64_t matched=0;unsigned distinct=0;
        for(const auto&[key,row]:rows_){matched+=row.equal?row.calls:0;distinct+=row.equal;
            std::fprintf(stderr,"[resource-code-observation] {\"calls\":%llu,\"equal\":%s,\"intact\":%s,\"code_words\":%u,\"first_base\":%u,\"first_object\":%u,\"reason\":",static_cast<unsigned long long>(row.calls),row.equal?"true":"false",row.intact?"true":"false",row.code_words,row.first_base,row.first_object);json_string(row.reason);std::fputs(",\"candidate_signature\":",stderr);json_string(key);std::fputs("}\n",stderr);
        }
        std::fprintf(stderr,"[resource-code-observation] summary entries=%llu matched_invocations=%llu matched_signatures=%u total_signatures=%zu nonentry_invocations=%llu diagnostic_errors=%llu reset_hooks=%llu runtime_admission=0\n",static_cast<unsigned long long>(calls_),static_cast<unsigned long long>(matched),distinct,rows_.size(),static_cast<unsigned long long>(resumes_),static_cast<unsigned long long>(observation_errors_),static_cast<unsigned long long>(resets_));
    }
};
}
