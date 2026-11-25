// TEST REFERENCE ONLY. Copies of actual generated AOT operations run against
// disposable in-process snapshot memory; this target is never a product library.
#define main phase4_main_not_invoked_by_stack_reference
#include "test_phase4_aot_equivalence.cpp"
#undef main
#include "stack_reference_slices.hpp"

namespace stack_reference {
struct BoundaryStop { std::uint32_t pc; bool expected; const char* reason; };
struct Event {
    std::uint32_t pc,address; unsigned width; bool store;
    std::array<std::uint8_t,16> bytes{};
    std::array<ps2::Reg128,32> registers{};
    std::array<std::uint32_t,32> fpr_bits{};
};
const Slice* active=nullptr;
ps2::Cpu* active_cpu=nullptr;
std::uint32_t executing_pc=0;
std::vector<Event> events;
bool contains(const std::uint32_t* first,std::size_t count,std::uint32_t value) {
    return std::find(first,first+count,value)!=first+count;
}
void boundary(ps2::Cpu&,std::uint32_t pc) {
    executing_pc=pc;
    if (!active) throw BoundaryStop{pc,false,"missing active reference slice"};
    if (contains(active->stops,active->stop_count,pc))
        throw BoundaryStop{pc,true,"declared safe boundary"};
    if (!contains(active->allowed,active->allowed_count,pc))
        throw BoundaryStop{pc,false,"generated control flow escaped declared slice"};
}
void observe(std::uint32_t address,unsigned width,bool store,const void* value=nullptr) {
    if (address>ps2::kRamSize || width>ps2::kRamSize-address)
        throw BoundaryStop{executing_pc,false,"reference memory outside snapshot RAM"};
    Event event;event.pc=executing_pc;event.address=address;event.width=width;event.store=store;
    std::memcpy(event.bytes.data(),value ? value : ps2::g_ram+address,width);
    std::copy_n(active_cpu->r,32,event.registers.data());
    std::memcpy(event.fpr_bits.data(),active_cpu->f,sizeof active_cpu->f);
    events.push_back(event);
}
std::uint32_t observed_ld32(std::uint32_t a) { observe(a,4,false);return ps2::ld32(a); }
std::uint8_t observed_ld8(std::uint32_t a) { observe(a,1,false);return ps2::ld8(a); }
std::uint16_t observed_ld16(std::uint32_t a) { observe(a,2,false);return ps2::ld16(a); }
std::uint64_t observed_ld64(std::uint32_t a) { observe(a,8,false);return ps2::ld64(a); }
void observed_ld128(std::uint32_t a,ps2::Reg128& r) { observe(a,16,false);ps2::ld128(a,r); }
void observed_st32(std::uint32_t a,std::uint32_t v) { observe(a,4,true,&v);ps2::st32(a,v); }
void observed_st8(std::uint32_t a,std::uint8_t v) { observe(a,1,true,&v);ps2::st8(a,v); }
void observed_st16(std::uint32_t a,std::uint16_t v) { observe(a,2,true,&v);ps2::st16(a,v); }
void observed_st64(std::uint32_t a,std::uint64_t v) { observe(a,8,true,&v);ps2::st64(a,v); }
void observed_st128(std::uint32_t a,const ps2::Reg128& v) { observe(a,16,true,&v);ps2::st128(a,v); }
} // namespace stack_reference

namespace {
using namespace stack_reference;
bool plant_size=false,plant_ra=false,ra_plant_exercised=false;
std::string context;
void require_frame(bool ok,const char* reason) {
    if (!ok) {
        std::fprintf(stderr,"FAIL_STACK_REFERENCE context=%s reason=%s\n",context.c_str(),reason);
        std::exit(1);
    }
}
std::uint64_t observation_hash(const void* bytes,std::size_t length) {
    // Noncryptographic observation fingerprint. Original/source SHA256 pins are
    // checked by the offline producer, not represented by this fast digest.
    const auto* p=static_cast<const std::uint8_t*>(bytes);
    std::uint64_t h=14695981039346656037ull;
    while (length>=8) { std::uint64_t value;std::memcpy(&value,p,8);h=(h^value)*1099511628211ull;p+=8;length-=8; }
    while (length--) h=(h^*p++)*1099511628211ull;
    return h;
}
std::uint32_t effective_pc(const Slice& slice,const Event& event) {
    for (std::size_t i=0;i<slice.delay_count;++i)
        if (slice.delays[i].owner==event.pc) return slice.delays[i].delay;
    return event.pc;
}
const Slot* event_slot(const Slice& slice,const Event& event) {
    const auto pc=effective_pc(slice,event);
    for (std::size_t i=0;i<slice.slot_count;++i) {
        const auto& slot=slice.slots[i];
        if (contains(event.store?slot.stores:slot.loads,
                     event.store?slot.store_count:slot.load_count,pc)) return &slot;
    }
    return nullptr;
}
unsigned check_events(const Slice& slice,std::uint32_t allocated_sp) {
    unsigned checked=0;
    for (const auto& event:events) {
        if (event.store) {
            const auto at=effective_pc(slice,event);
            bool authenticated=false;
            for (std::size_t i=0;i<slice.store_count;++i)
                authenticated|=slice.stores[i].pc==at&&slice.stores[i].width==event.width;
            require_frame(authenticated,"observed store PC/width has no authenticated original instruction");
        }
        const auto* slot=event_slot(slice,event);
        if (!slot) continue; // Nonframe prefix effects remain observations only.
        const auto expected=std::int64_t(allocated_sp)+slot->offset;
        require_frame(expected>=0 && expected<=ps2::kRamSize-event.width,"proved stack address outside reference RAM");
        require_frame(event.address==expected,"compiled AOT stack slot address differs from declared frame offset");
        require_frame(event.width*8==slot->width,"compiled AOT stack transfer width differs from declared slot");
        if (event.store && slot->reg<32) {
            if (slot->bank==1) {
                require_frame(event.width==4,"FPR stack spill requires exact32-bit payload");
                require_frame(std::memcmp(event.bytes.data(),&event.fpr_bits[slot->reg],4)==0,
                              "compiled AOT FPR spill differs from actual FPR source bits");
            } else {
                require_frame(slot->bank==0,"unbound register bank cannot validate a spill");
                require_frame(std::memcmp(event.bytes.data(),&event.registers[slot->reg],event.width)==0,
                              "compiled AOT GPR spill differs from actual source register portion");
            }
        }
        ++checked;
    }
    return checked;
}
bool guard_interference(const Slice& slice,std::uint32_t frame_base,std::uint32_t old_sp,
                        const std::vector<std::uint8_t>& before) {
    const bool changed=std::memcmp(ps2::g_ram+frame_base-16,before.data()+frame_base-16,16)!=0 ||
                       std::memcmp(ps2::g_ram+old_sp,before.data()+old_sp,16)!=0;
    bool observed_nonframe=false,owned_frame_alias=false;
    for (const auto& event:events) {
        if (!event.store||event_slot(slice,event))continue;
        const auto end=std::uint64_t(event.address)+event.width;
        if ((event.address<frame_base&&end>frame_base-16)||
            (event.address<old_sp+16&&end>old_sp))observed_nonframe=true;
        if (event.address<old_sp&&end>frame_base)owned_frame_alias=true;
    }
    require_frame(!changed||observed_nonframe,"original frame adjacent canary changed without observed nonframe store");
    return observed_nonframe||owned_frame_alias;
}
void check_accounted_ram(const std::vector<std::uint8_t>& before) {
    // This is store accounting, not independent semantics for nonframe effects.
    // In particular an original epilogue may store through an output argument.
    auto accounted=before;
    for (const auto& event:events)if(event.store)
        std::memcpy(accounted.data()+event.address,event.bytes.data(),event.width);
    require_frame(std::memcmp(ps2::g_ram,accounted.data(),accounted.size())==0,
                  "full RAM changes exceed observed original AOT stores");
}
struct RunResult { bool stopped;bool safe;std::uint32_t pc;const char* reason; };
RunResult run(const Slice& slice) {
    active=&slice;active_cpu=&ps2::g_cpu;events.clear();
    try {
        slice.raw(ps2::g_cpu,slice.entry,0u);
        active=nullptr;
        return {false,slice.epilogue,ps2::g_cpu.pc,slice.epilogue?"natural original JR return":"entry prefix returned outside declared stop"};
    } catch (const BoundaryStop& stop) {
        active=nullptr;return {true,stop.expected,stop.pc,stop.reason};
    }
}
bool transport_class(const Slot& slot) {
    return std::strstr(slot.classification,"transport")!=nullptr;
}
const Slot* restored_ra_slot(const Slice& epilogue) {
    for (std::size_t i=0;i<epilogue.slot_count;++i) {
        const auto& slot=epilogue.slots[i];
        if (slot.reg!=31||slot.bank!=0) continue;
        for (std::size_t j=0;j<slot.load_count;++j)
            if (slot.loads[j]<epilogue.jr && contains(epilogue.allowed,epilogue.allowed_count,slot.loads[j]))return &slot;
    }
    return nullptr;
}
bool prefix_saved_ra(const Slice& prefix,const Slot& slot) {
    for (const auto& event:events)
        if (event.store && contains(slot.stores,slot.store_count,effective_pc(prefix,event)))return true;
    return false;
}
void check_restores(const Slice& epilogue,const ps2::Cpu& before,
                    const std::vector<std::uint8_t>& retained_ram,std::uint32_t frame_base) {
    for (std::size_t i=0;i<epilogue.slot_count;++i) {
        const auto& slot=epilogue.slots[i];
        if (slot.reg>=32 || !transport_class(slot))continue;
        bool executed=false;
        for (const auto& event:events)
            if (!event.store && event_slot(epilogue,event)==&slot)executed=true;
        if (!executed)continue;
        const auto address=std::int64_t(frame_base)+slot.offset;
        require_frame(address>=0 && address+slot.width/8<=ps2::kRamSize,"retained restore source extent");
        if (slot.bank==1) {
            require_frame(slot.width==32,"FPR restore requires exact32-bit payload");
            require_frame(std::memcmp(&ps2::g_cpu.f[slot.reg],retained_ram.data()+address,4)==0,
                          "restored FPR bits differ from retained original frame bytes");
            continue;
        }
        require_frame(slot.bank==0,"unbound register bank cannot validate a restore");
        auto expected=before.r[slot.reg];
        if (slot.width==32) {
            std::uint32_t value;std::memcpy(&value,retained_ram.data()+address,4);
            expected.sd[0]=static_cast<std::int32_t>(value);
        } else std::memcpy(&expected,retained_ram.data()+address,slot.width/8);
        require_frame(std::memcmp(&ps2::g_cpu.r[slot.reg],&expected,sizeof expected)==0,
            plant_ra&&slot.reg==31 ? "planted-ra-clobber: restored RA differs from retained original saved RA" :
                                    "restored register portion/upper lane differs from retained original frame bytes");
    }
}
void observation(const char* file,const Slice& slice,const char* phase,const char* status,
                 const char* reason,std::uint32_t before_sp,std::uint32_t boundary_pc,
                 const ps2::Cpu& before,std::uint64_t before_ram,unsigned checked,const char* canary_status="NOT_VALIDATED") {
    std::printf("STACK_OBSERVATION file=%s target=%08X phase=%s start=%08X status=%s reason=%s "
        "SP_before=%08X SP_after=%08X boundary=%08X checked_stack_events=%u "
        "full_CPU_bytes=%zu CPU_before=%016llX CPU_after=%016llX full_RAM_bytes=%u "
        "RAM_before=%016llX RAM_after=%016llX canary_status=%s native_admission=%s whole_function_E2=NOT_CLAIMED\n",
        file,slice.target,phase,slice.start,status,reason,before_sp,ps2::g_cpu.r[29].w[0],boundary_pc,checked,
        sizeof(ps2::Cpu),(unsigned long long)observation_hash(&before,sizeof before),
        (unsigned long long)observation_hash(&ps2::g_cpu,sizeof ps2::g_cpu),ps2::kRamSize,
        (unsigned long long)before_ram,(unsigned long long)observation_hash(ps2::g_ram,ps2::kRamSize),
        canary_status,slice.native_status);
}
} // namespace

int main(int argc,char** argv) {
    if (argc<2 || argc>3) { std::fprintf(stderr,"usage: test_generated_stack_frames STATES [--planted-frame-size|--planted-ra-clobber]\n");return 2; }
    if (argc==3) {
        plant_size=std::strcmp(argv[2],"--planted-frame-size")==0;
        plant_ra=std::strcmp(argv[2],"--planted-ra-clobber")==0;
        if (!plant_size&&!plant_ra)return 2;
    }
    register_generated_code();register_generated_vu();
    std::vector<std::filesystem::path> files;
    for (const auto& e:std::filesystem::directory_iterator(argv[1]))
        if (e.is_regular_file()&&e.path().extension()==".state")files.push_back(e.path());
    std::sort(files.begin(),files.end());require_frame(files.size()==61,"exact authenticated61-state corpus required");
    unsigned prefix_attempts=0,prefix_pass=0,prefix_unsafe=0,epi_attempts=0,epi_pass=0,epi_unsafe=0;
    for (const auto& file:files) {
        OracleVuLayout layout;std::string reason;const auto bytes=read_binary(file);
        require_frame(load_oracle_ee_vu(bytes,layout,reason),"bounded v8 snapshot admission");
        const auto saved=ps2::g_cpu;std::vector<std::uint8_t> saved_ram(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
        const auto saved_ram_hash=observation_hash(saved_ram.data(),saved_ram.size());
        for (std::size_t i=0;i<slice_count;++i) {
            const auto& prefix=slices[i];if (prefix.epilogue)continue;++prefix_attempts;
            context=file.filename().string()+":"+std::to_string(prefix.target);
            ps2::g_cpu=saved;std::memcpy(ps2::g_ram,saved_ram.data(),saved_ram.size());ps2::g_cpu.pc=prefix.start;
            const auto old_sp=saved.r[29].w[0];
            if (!prefix.frame || old_sp<prefix.frame+16 || old_sp>ps2::kRamSize-16 || old_sp&15u) {
                ++prefix_unsafe;observation(file.filename().string().c_str(),prefix,"entry","NOT_ADMITTED","original_SP_or_frame_bounds",old_sp,prefix.start,saved,saved_ram_hash,0);continue;
            }
            const auto result=run(prefix);
            if (!result.safe) { ++prefix_unsafe;observation(file.filename().string().c_str(),prefix,"entry","UNSAFE_BOUNDARY",result.reason,old_sp,result.pc,saved,saved_ram_hash,0);continue; }
            const auto expected_frame=prefix.frame+(plant_size?16u:0u);
            require_frame(ps2::g_cpu.r[29].w[0]==old_sp-expected_frame,
                plant_size?"planted-frame-size: expected frame differs from actual compiled AOT SP delta":"compiled AOT entry SP delta differs from declared frame geometry");
            auto expected_sp=saved.r[29];expected_sp.sd[0]=static_cast<std::int32_t>(old_sp-prefix.frame);
            require_frame(std::memcmp(&ps2::g_cpu.r[29],&expected_sp,sizeof expected_sp)==0,
                          "original ADDIU SP sign extension and upper128 portion preservation");
            const auto frame_base=ps2::g_cpu.r[29].w[0];const auto checked=check_events(prefix,frame_base);
            const auto prefix_guard=guard_interference(prefix,frame_base,old_sp,saved_ram);
            check_accounted_ram(saved_ram);
            if (prefix_guard)++prefix_unsafe;else ++prefix_pass;
            observation(file.filename().string().c_str(),prefix,"entry",
                prefix_guard?"NOT_ADMITTED_UNPROVED_FRAME_ISOLATION":"PASS_REFERENCE_GEOMETRY",
                "SP_geometry_observed_frame_admission_requires_isolation",old_sp,result.pc,saved,saved_ram_hash,checked,
                prefix_guard?"UNVALIDATED_NONFRAME_ALIAS":"PRESERVED");
            const auto post_prefix=ps2::g_cpu;std::vector<std::uint8_t> retained_ram(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
            const auto prefix_ram_hash=observation_hash(retained_ram.data(),retained_ram.size());
            bool ra_saved=false;const Slot* prefix_ra=nullptr;
            for (std::size_t k=0;k<prefix.slot_count;++k)
                if (prefix.slots[k].reg==31&&prefix.slots[k].bank==0&&prefix_saved_ra(prefix,prefix.slots[k])) {ra_saved=true;prefix_ra=&prefix.slots[k];}
            for (std::size_t j=0;j<slice_count;++j) {
                const auto& epilogue=slices[j];if (!epilogue.epilogue||epilogue.target!=prefix.target)continue;++epi_attempts;
                ps2::g_cpu=post_prefix;std::memcpy(ps2::g_ram,retained_ram.data(),retained_ram.size());ps2::g_cpu.pc=epilogue.start;
                const auto* ra=restored_ra_slot(epilogue);std::uint32_t expected_return=post_prefix.r[31].w[0];
                if (ra) {
                    const auto at=std::int64_t(frame_base)+ra->offset;
                    if (at<0||at+ra->width/8>ps2::kRamSize) {
                        ++epi_unsafe;observation(file.filename().string().c_str(),epilogue,"exit","NOT_ADMITTED",
                            "original_RA_slot_outside_snapshot_RAM",frame_base,epilogue.start,post_prefix,prefix_ram_hash,0);
                        continue;
                    }
                    std::memcpy(&expected_return,retained_ram.data()+at,4);
                }
                if (expected_return&3u) {
                    ++epi_unsafe;observation(file.filename().string().c_str(),epilogue,"exit","NOT_ADMITTED","natural_saved_RA_unaligned_no_repair",frame_base,epilogue.start,post_prefix,prefix_ram_hash,0);continue;
                }
                if (plant_ra&&!ra_plant_exercised&&ra_saved&&ra&&prefix_ra&&ra->offset==prefix_ra->offset) {
                    // Retain expected_return and retained_ram BEFORE mutation.
                    ps2::g_ram[frame_base+ra->offset]^=4u;ra_plant_exercised=true;
                }
                const auto result_exit=run(epilogue);
                if (!result_exit.safe) {++epi_unsafe;observation(file.filename().string().c_str(),epilogue,"exit","UNSAFE_BOUNDARY",result_exit.reason,frame_base,result_exit.pc,post_prefix,prefix_ram_hash,0);continue;}
                const auto exit_checked=check_events(epilogue,frame_base);
                const auto exit_guard=guard_interference(epilogue,frame_base,old_sp,retained_ram)||prefix_guard;
                if (exit_guard) {
                    check_accounted_ram(retained_ram);++epi_unsafe;
                    observation(file.filename().string().c_str(),epilogue,"exit","NOT_ADMITTED_UNPROVED_FRAME_ISOLATION",
                        "original_nonframe_store_aliases_owned_frame_or_guards",frame_base,result_exit.pc,post_prefix,prefix_ram_hash,exit_checked,"UNVALIDATED_NONFRAME_ALIAS");
                    continue;
                }
                check_restores(epilogue,post_prefix,retained_ram,frame_base);
                require_frame(ps2::g_cpu.r[29].w[0]==old_sp,"compiled AOT JR delay restores original SP");
                auto restored_sp=post_prefix.r[29];restored_sp.sd[0]=static_cast<std::int32_t>(old_sp);
                require_frame(std::memcmp(&ps2::g_cpu.r[29],&restored_sp,sizeof restored_sp)==0,
                              "original SP restoration preserves upper128 portion");
                require_frame(ps2::g_cpu.pc==expected_return,plant_ra?"planted-ra-clobber: return differs from retained original saved RA":"original JR return differs from retained source bytes");
                check_accounted_ram(retained_ram);
                ++epi_pass;observation(file.filename().string().c_str(),epilogue,"exit","PASS_REFERENCE_TRANSPORT",
                    ra_saved?"RA_saved_by_executed_prefix_no_whole_function_claim":"RA_source_is_original_unwritten_snapshot_slot_no_call_ABI_claim",
                    frame_base,result_exit.pc,post_prefix,prefix_ram_hash,exit_checked,"PRESERVED");
            }
        }
    }
    require_frame(prefix_attempts==28u*61u,"all28x61 entry admissions must be explicit");
    if (plant_ra)require_frame(ra_plant_exercised,"planted-ra-clobber: no eligible saved RA pair executed");
    std::printf("PASS_STACK_REFERENCE states=61 targets=28 entry_attempts=%u entry_pass=%u entry_unsafe=%u exit_attempts=%u exit_pass=%u exit_unsafe=%u whole_function_E2=NOT_ATTEMPTED historical_invocation=NOT_ATTEMPTED native_credit=0\n",
        prefix_attempts,prefix_pass,prefix_unsafe,epi_attempts,epi_pass,epi_unsafe);
}
