// Actual frozen v8 boot/loader/start/IRQ component, not a game replay.
#include R192_OWNER_FIXTURE_PREFIX
#include "iop/r192_fresh_execution_owner.hpp"
#include "ps2/state.hpp"
#include <iomanip>
namespace lo=ps2::iop::loader_owner;
namespace fx=ps2::iop::fresh_execution;
#include R192_OWNER_SOURCES
namespace {
std::unique_ptr<fx::Owners> owners;
fx::Token execution_token;
aot::Pipeline execution_pipeline;
lo::Facts facts(int id){const auto&m=kernel::g_modules.at(id);return {m.id,m.name,m.base,m.size,m.entry,m.gp};}
std::vector<lo::Facts> all_facts(){std::vector<lo::Facts>out;for(const auto&[id,m]:kernel::g_modules)out.push_back(facts(id));return out;}
std::uint32_t test_read(std::uint32_t address,unsigned width){
    if(address>=kRamSize || width>kRamSize-address || (width!=1&&width!=4))throw std::runtime_error("test_reader_bounds");
    std::uint32_t value{};for(unsigned i=0;i<width;++i)value|=std::uint32_t(g_ram[address+i])<<(8*i);return value;
}
bool owned_invoke(void*,const aot::Import&entry,Cpu&cpu){
    const bool links=std::strcmp(entry.library,"loadcore")==0 && entry.ordinal==6;
    if(links)owners->before_original_link_imports();
    try{const bool result=invoke(nullptr,entry,cpu);if(links)owners->after_original_link_imports();return result;}
    catch(...){owners->poison();throw;}
}
void compiled_execute(Cpu&cpu,std::int64_t&budget){
    while(budget-- >0){
        bool stub=false;const auto pc=phys(cpu.pc);
        for(const auto&[address,id]:kernel::g_hle_stubs)if(phys(address)==pc){
            if(test_read(pc,4)!=((kHleOpcode<<26)|id) || test_read(pc+4,4)!=0)throw std::runtime_error("kernel_stub_identity");
            const auto target=cpu.r[31];cpu.pc=target;cpu.npc=target+4;cpu.r[0]=0;service(id,cpu);cpu.r[0]=0;stub=true;break;
        }
        if(stub)continue;
        const auto&bound=owners->enter(execution_token,cpu.pc);
        const aot::Services services{owners.get(),[](void*u,std::uint32_t a,const aot::Import&i){return static_cast<fx::Owners*>(u)->import_identity(a,i);},owned_invoke};
        const auto result=aot::dispatch(bound.module,cpu,execution_pipeline,1,services);
        if(result.instructions!=1 || result.status!=aot::Status::budget_exhausted || execution_pipeline.pending_load)
            throw std::runtime_error(std::string("fresh_compiled_frontier:")+result.reason);
    }
}
template<class F>void refuses(F f,const std::string&reason){try{f();}catch(const std::exception&e){if(e.what()==reason)return;throw;}throw std::runtime_error("required_refusal_absent:"+reason);}
}
int main(int argc,char**argv){
    if(argc!=5)return 2;const std::string mode=argv[1],route=argv[3],output=argv[4];
    const bool compiled=mode!="reference";std::string error;unsigned instructions{},controls{};bool irq_preserved=false;
    std::uint32_t leaf_offset{},leaf_word{},leaf_delay{};Cpu cpu{};int id{};
    try{
        if(!ps2::g_options.memcard_path.empty())throw std::runtime_error("card_configuration_refused");
        std::ifstream in(argv[2],std::ios::binary);const std::vector<std::uint8_t>bytes{std::istreambuf_iterator<char>(in),{}};
        const auto digest=lo::image_sha256(bytes);
        const auto source=std::find_if(original_sources.begin(),original_sources.end(),[&](const auto&s){return s.digest==digest;});
        if(source==original_sources.end())throw std::runtime_error("test_source_not_in_original_catalogue");
        kernel::boot();for(const auto&[key,value]:kernel::g_hle_ids)known_boot_ids.insert(value);
        owners=std::make_unique<fx::Owners>(original_sources,test_read,aot::Services{nullptr,kernel::identity,nullptr},facts,all_facts,
            [](const std::string&name){const auto i=kernel::g_libraries.find(name);return i==kernel::g_libraries.end()?0u:i->second;});
        auto loader=[&](const auto&b,const auto&p,const auto&a){id=load_module_buffer(b,p,a);return id;};
        const auto path=route=="buffer"?"buffer":source->filename_name;
        if(compiled)execution_token=owners->load(bytes,path,"",loader);else id=loader(bytes,path,"");
        const auto loaded=facts(id);aot::BoundPic reference; if(!reference.bind(*source->code,loaded.base))throw std::runtime_error("reference_PIC_descriptor");
        if(mode=="wrong_word"){
            const auto old=ram32(loaded.entry);set_ram32(loaded.entry,old^1u);
            refuses([&]{owners->enter(execution_token,loaded.entry);},"IOP_METADATA_REFUSED:word_identity_changed");++controls;set_ram32(loaded.entry,old);
            refuses([&]{owners->enter(execution_token,loaded.entry);},"fresh_execution_session_poisoned");++controls;
        }else if(mode=="wrong_image"){
            auto wrong=bytes;wrong.back()^=1u;unsigned calls{};
            refuses([&]{owners->load(wrong,path,"",[&](const auto&,const auto&,const auto&){++calls;return 1;});},"unknown_original_before_loader");
            if(calls)throw std::runtime_error("wrong_image_entered_loader");++controls;
        }else if(mode=="free_reuse"){
            const auto old_token=execution_token;const auto size=kernel::g_used.at(loaded.base);
            kernel::free(loaded.base);owners->allocation_freed(loaded.base,size,true);
            refuses([&]{owners->enter(old_token,loaded.entry);},"stale_fresh_owner_generation");++controls;
            execution_token=owners->load(bytes,path,"",loader);const auto now=facts(id);
            if(now.base!=loaded.base)throw std::runtime_error("actual_allocator_reuse_missing");
            refuses([&]{owners->enter(old_token,now.entry);},"stale_fresh_owner_generation");++controls;
            refuses([&]{owners->enter(now.entry);},"reused_code_range_requires_generation_bound_entry");++controls;
            owners->enter(execution_token,now.entry);++controls;
        }else if(mode=="noop_free"){
            const auto count=kernel::g_used.size();kernel::free(loaded.base+4);
            const bool erased=kernel::g_used.size()!=count;
            if(erased)throw std::runtime_error("original_interior_free_was_not_noop");
            owners->allocation_freed(loaded.base+4,loaded.size,erased);owners->enter(execution_token,loaded.entry);++controls;
        }else if(mode=="overlap_current"){
            kernel::g_modules[id+100]=kernel::Module{id+100,"PLANTED_STALE",loaded.base+0x100,0x100,loaded.entry,loaded.gp};
            refuses([&]{owners->enter(execution_token,loaded.entry);},"overlapping_live_original_modules");++controls;
            kernel::g_modules.erase(id+100);
            refuses([&]{owners->enter(execution_token,loaded.entry);},"fresh_execution_session_poisoned");++controls;
        }else if(mode=="wrong_facts"){
            kernel::g_modules[id].entry+=4;
            refuses([&]{owners->enter(execution_token,loaded.entry);},"fresh_loaded_facts_changed");++controls;
            kernel::g_modules[id].entry-=4;
            refuses([&]{owners->enter(execution_token,loaded.entry);},"fresh_execution_session_poisoned");++controls;
        }else if(mode=="metadata_pc"){
            const auto record=std::find_if(source->records.begin(),source->records.end(),[](const auto&r){return r.kind==ps2::iop::metadata_guard::Kind::exports;});
            if(record==source->records.end())throw std::runtime_error("actual_source_export_metadata_missing");
            refuses([&]{owners->enter(execution_token,loaded.base+record->offset);},"metadata_or_data_PC_not_original_code");++controls;
        }else if(mode=="actual_reset"){
            const auto old=execution_token;owners->reset();kernel::boot();
            refuses([&]{owners->enter(old,loaded.entry);},"stale_fresh_owner_epoch");++controls;
            execution_token=owners->load(bytes,path,"",loader);owners->enter(execution_token,facts(id).entry);++controls;
        }else if(mode=="restore_boundary"){
            kernel::thread(1)->on_exit=nullptr;
            ps2::state::Writer saved;saved.set_version(8);ps2::iop::state(saved);
            const auto prior=execution_token;owners->before_restore();
            refuses([&]{owners->enter(prior,loaded.entry);},"restored_or_restoring_not_fresh_owner");++controls;
            g_ram[0]^=0x55;ps2::state::Reader restored(saved.out.data(),saved.out.size());restored.set_version(8);
            ps2::iop::state(restored);if(restored.remaining())throw std::runtime_error("actual_IOP_restore_trailing_bytes");
            owners->after_restore(true);
            if(owners->mode()!=fx::Mode::restored_legacy_unproved)throw std::runtime_error("restored_mode_fabricated_owner");++controls;
            owners->reset();owners->before_restore();g_ram[0]^=0x55;const auto damaged=g_ram[0];bool partial=false;
            try{ps2::state::Reader truncated(saved.out.data(),saved.out.size()-1);truncated.set_version(8);ps2::iop::state(truncated);}
            catch(const std::exception&e){if(std::string(e.what())!="state file is truncated")throw;partial=g_ram[0]!=damaged;}
            if(!partial)throw std::runtime_error("actual_progressive_IOP_restore_failure_not_exercised");
            owners->after_restore(false);
            refuses([&]{owners->enter(prior,loaded.entry);},"fresh_execution_session_poisoned");++controls;
        }else if(mode=="reference" || mode=="compiled"){
            cpu=kernel::thread(1)->context;const auto exit=cpu.r[31];
            trace.open(output+".trace",std::ios::binary);observe=true;checkpoint(cpu);
            while(instructions<100000 && cpu.pc!=exit){std::int64_t one=1;
                if(compiled)compiled_execute(cpu,one);else{
#ifdef R192_USE_PRIVATE_AOT
                    reference_execute(cpu,one);
#else
                    execute(cpu,one);
#endif
                }
                ++instructions;checkpoint(cpu);
            }
            observe=false;trace.close();if(cpu.pc!=exit)throw std::runtime_error("whole_start_did_not_return");
            // Select only a real source export with a two-word nonblocking
            // return body. NOP preserves the explicit incoming V0; a literal
            // delay assigns V0. This is IRQ transport, not an inferred SDK ABI.
            for(const auto&r:source->records)if(r.kind==ps2::iop::metadata_guard::Kind::exports)
                for(std::size_t slot=5;slot+1<r.words.size();++slot){const auto at=r.words[slot];
                    if(at+8<=reference.module.text_size && reference.words[at/4]==0x03e00008u &&
                       (reference.words[at/4+1]==0 || (reference.words[at/4+1]&0xffff0000u)==0x24020000u)){leaf_offset=at;leaf_word=reference.words[at/4];leaf_delay=reference.words[at/4+1];break;}}
            if(!leaf_offset)throw std::runtime_error("real_original_export_leaf_unavailable");
            for(unsigned i=1;i<32;++i)g_cpu.r[i]=0x12340000u+i*0x101u;g_cpu.r[0]=0;g_cpu.pc=0x12345678;g_cpu.npc=g_cpu.pc+4;
            const Cpu before=g_cpu;
            if(compiled)execution_override=compiled_execute;
            kernel::register_interrupt(9,loaded.base+leaf_offset,0x77889900,0);kernel::enable_interrupt(9,true);
            ps2::iop::raise_interrupt(9);kernel::deliver_interrupts();execution_override={};
            irq_preserved=std::memcmp(&before,&g_cpu,sizeof before)==0;if(!irq_preserved)throw std::runtime_error("IRQ_current_Cpu_not_preserved");
            kernel::thread(1)->on_exit=nullptr;kernel::thread(1)->context=cpu;
            save(output+".cpu",&cpu,sizeof cpu);save(output+".ram",g_ram,kRamSize);
            ps2::state::Writer state;state.set_version(8);ps2::iop::state(state);save(output+".iop-state",state.out.data(),state.out.size());
        }else throw std::runtime_error("unsupported_test_mode");
    }catch(const std::exception&e){error=e.what();observe=false;if(trace.is_open())trace.close();execution_override={};}
    std::ofstream report(output+".json");report<<"{\"status\":"<<std::quoted(error.empty()?"PASS":"REFUSED")<<",\"mode\":"<<std::quoted(mode)
        <<",\"reason\":"<<std::quoted(error)<<",\"whole_start_instructions\":"<<instructions<<",\"IRQ_current_Cpu_preserved\":"<<(irq_preserved?"true":"false")
        <<",\"source_export_leaf_offset\":"<<leaf_offset<<",\"source_leaf_words\":["<<leaf_word<<","<<leaf_delay<<"],\"controls\":"<<controls
        <<",\"native_service_calls\":"<<service_calls<<",\"forbidden_interpreter_calls\":"<<forbidden_interpreter_calls<<",\"native_credit\":0}\n";
    return error.empty()?0:1;
}
