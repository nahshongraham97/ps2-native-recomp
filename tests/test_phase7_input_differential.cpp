#include "mka/phase7_input_native.hpp"
#include "phase7_track_b_capture.hpp"
#include <cstdio>
#include <limits>
using namespace mka::phase7;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static unsigned source_cases() {
    unsigned tested=0;
    // Independently enumerated instruction paths: four journals; 12..15
    // directional selection; 29->30 then zero; recent4->5 then zero.
    for(unsigned owner=0;owner<2;++owner) for(std::uint64_t action:{0ull,12ull,13ull,14ull,15ull,16ull,0x10000000cull}) {
        mka::retail::NativeRetailActionHistory history;
        for(auto& j:history.journals){j.cursor=29;j.records[0].context=0xdead;}
        for(auto& r:history.recent)r.cursor=4;
        NativeJournalState state{history,77,0,true,{true,true}};
        const bool directional=action==12||action==13||action==14||action==15;
        const unsigned group=owner*2+(directional?1:0);
        std::vector<int> calls;NativeJournalServices services;
        services.resolve_context=[&](bool second,std::uint32_t& context){require(second==bool(owner),"context actor identity");calls.push_back(1);context=0xa5;return true;};
        services.dispatch=[&]{calls.push_back(2);require(history.recent[owner].canonical[0]==19,"dispatch effect order");return true;};
        std::vector<JournalField> fields;
        const auto result=record_action_00162e70(state,{bool(owner),action,88,19,"action"},services,
          [&](JournalField field,unsigned,unsigned,std::uint32_t){fields.push_back(field);});
        require(result==JournalResult::complete,"journal source case result");
        require(history.journals[group].cursor==0&&history.journals[group].records[0].action==std::int32_t(std::uint32_t(action)),"journal group and wrap");
        require(state.last_action_frame==77&&history.journals[group].records[0].frame==88,"source frame vs argument frame");
        require(history.journals[group].records[0].context==(directional?0xdead:0xa5),"directional context untouched");
        require(calls==(directional?std::vector<int>{2}:std::vector<int>{1,2}),"callback order");
        require(fields[0]==JournalField::last_frame&&fields[1]==JournalField::cursor&&fields[2]==JournalField::cursor,"delay slot stores retained");
        ++tested;
    }
    {
        mka::retail::NativeRetailActionHistory h;NativeJournalState s{h,4,0,false,{true,false}};
        require(record_action_00162e70(s,{false,1,5,7,"x"},{})==JournalResult::context_service_unbound,"context missing refusal");
        require(h.journals[0].records[0].canonical==7&&h.recent[0].cursor==0,"refusal preserves earlier effects");++tested;
    }
    {
        mka::retail::NativeRetailActionHistory h;NativeJournalState s{h,4,0,true,{false,false}};
        require(record_action_00162e70(s,{true,1,5,7,"x"},{})==JournalResult::dispatch_service_unbound,"dispatch missing refusal");
        require(h.recent[1].cursor==1&&h.recent[1].canonical[1]==7,"dispatch boundary ordering");++tested;
    }
    {
        mka::retail::NativeRetailActionHistory h;h.journals[0].cursor=std::numeric_limits<std::int32_t>::max();NativeJournalState s{h};
        require(record_action_00162e70(s,{false,1,0,0,""},{})==JournalResult::invalid_cursor,"wrapped negative cursor refusal");
        require(h.journals[0].cursor==std::numeric_limits<std::int32_t>::min(),"ADDIU defined wrapping");++tested;
    }
    {
        mka::retail::NativeRetailActionHistory h;NativeJournalState s{h,4,0,false,{true,false}};
        NativeJournalServices service;service.resolve_context=[&](bool,std::uint32_t& value){h.journals[0].cursor=4;value=91;return true;};
        require(record_action_00162e70(s,{false,1,5,7,"x"},service)==JournalResult::complete,"reentrant context result");
        require(h.journals[0].records[4].context==91&&h.journals[0].records[0].context==0,"context live cursor reload");++tested;
    }
    return tested;
}
int main(int argc,char**argv)try {
    if(argc<2)return 2;const std::string plant=argc>2?argv[2]:"";
    const auto synthetic=source_cases();
    const auto files=track_b::files(argv[1],"cap_00162e70_");if(files.empty())return 2;
    unsigned partial=0,prefixes=0,completed=0;
    for(const auto& p:files) {
        auto c=track_b::load(p);require(c.pc==0x162e70,"input capture target");
        if(!c.returned())++partial;
        const auto gp=c.before.r[28].w[0];
        auto u=[&](unsigned a){return track_b::read(c.ram,a);};
        mka::retail::NativeRetailActionHistory history;
        // Four cursor/table bindings are established from original LUI/ADDIU
        // destination words, not from observed output addresses.
        const std::array<unsigned,4> bases{0x65ed40,0x65e880,0x65eae0,0x65e620};
        const std::array<unsigned,4> cursor_addresses{gp-32068,gp-32060,gp-32064,gp-32056};
        for(unsigned i=0;i<4;++i)history.journals[i].cursor=std::bit_cast<std::int32_t>(u(cursor_addresses[i]));
        history.recent[0].cursor=std::bit_cast<std::int32_t>(u(gp-24580));history.recent[1].cursor=std::bit_cast<std::int32_t>(u(gp-24584));
        NativeJournalState state{history,u(gp-25148),u(gp-24588),u(gp-24592)!=0,{u(0x66015c)!=0,u(0x6601a4)!=0}};
        NativeJournalAction a{c.before.r[4].d[0]!=0,c.before.r[5].d[0],c.before.r[6].w[0],c.before.r[8].w[0],"capture label"};
        std::vector<track_b::Store> observed;
        const auto result=record_action_00162e70(state,a,{},[&](JournalField field,unsigned group,unsigned slot,std::uint32_t value){
            unsigned address=0;
            switch(field) {
            case JournalField::last_frame:address=gp-24588;break;
            case JournalField::cursor:address=cursor_addresses[group];break;
            case JournalField::action:address=bases[group]+slot*20;break;
            case JournalField::frame:address=bases[group]+slot*20+4;break;
            case JournalField::label:address=bases[group]+slot*20+8;value=c.before.r[7].w[0];break;
            case JournalField::canonical:address=bases[group]+slot*20+16;break;
            case JournalField::context:address=bases[group]+slot*20+12;break;
            case JournalField::recent_cursor:address=gp-(group?24584:24580);break;
            case JournalField::recent_canonical:address=(group?0x65e5e0:0x65e600)+slot*4;break;
            }
            observed.push_back({address,value});
        });
        if(plant=="--planted-store"&&!observed.empty())observed.back().value^=1;
        if(plant=="--planted-order"&&observed.size()>1)std::swap(observed[0],observed[1]);
        const auto sp=c.before.r[29].w[0];std::vector<track_b::Store> expected;
        for(auto store:c.stores)if(!(store.address>=sp-4096&&store.address<sp))expected.push_back(store);
        // A missing service stops before its effects. Existing partial captures
        // can prove this exact prefix only; never promote them to whole calls.
        require(observed.size()<=expected.size(),"input prefix length");
        for(unsigned i=0;i<observed.size();++i)if(observed[i]!=expected[i]){std::fprintf(stderr,"FAIL input prefix %s index=%u actual=%x:%x expected=%x:%x\n",p.string().c_str(),i,observed[i].address,observed[i].value,expected[i].address,expected[i].value);return 1;}
        ++prefixes;
        if(result==JournalResult::complete&&c.returned()) {
            require(observed==expected,"completed input ordered stores");
            for(auto store:observed)std::memcpy(c.ram.data()+store.address,&store.value,4);
            for(std::size_t address=0;address<c.ram.size();++address)
                if(!(address>=sp-32&&address<sp)&&c.ram[address]!=c.exit_ram[address])
                    throw std::runtime_error("completed input full RAM mismatch");
            require(c.scratch==c.exit_scratch,"completed input scratch mismatch");
            require(c.before.fcr31==c.after.fcr31,"completed input numeric flags mismatch");
            ++completed;
        }
        std::printf("INPUT_SLICE %s ra=%08x exit_pc=%08x stores=%zu verified_prefix=%zu service=%s returned=%d\n",p.filename().string().c_str(),c.before.r[31].w[0],c.after.pc,c.stores.size(),observed.size(),result==JournalResult::complete?"none":result==JournalResult::context_service_unbound?"001EF2C0_unbound":"00162C20_unbound",c.returned());
    }
    std::printf("PASS_PHASE7_INPUT_COMPONENT source_cases=%u capture_prefixes=%u partial_slices=%u whole_function_passes=%u\n",synthetic,prefixes,partial,completed);
    if(plant=="--require-complete")return completed==files.size()?0:1;
    return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL_PHASE7_INPUT %s\n",e.what());return 1;}
