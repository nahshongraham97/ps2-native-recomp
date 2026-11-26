// The actual kernel scheduler, exit closure and IRQ delivery execute through
// the disposable production connector. No game/card/device UI is started.
#include R192_OWNER_FIXTURE_PREFIX
#include "iop/r192_fresh_execution_hooks.hpp"
#include "ps2/state.hpp"
#include <iomanip>
int main(int argc,char**argv){
    if(argc!=5)return 2;const bool compiled=std::string(argv[1])=="compiled";
    const std::string route=argv[3],output=argv[4];std::string error;unsigned done_count{};int module_id{},module_result{};
    bool irq_equal=false;std::uint32_t leaf{},leaf_word{},leaf_delay{};unsigned production_controls{};
    std::vector<std::string> control_reasons;
    try{
        if(!ps2::g_options.memcard_path.empty())throw std::runtime_error("card_configuration_refused");
        std::ifstream in(argv[2],std::ios::binary);const std::vector<std::uint8_t>bytes{std::istreambuf_iterator<char>(in),{}};
        const auto digest=ps2::iop::loader_owner::image_sha256(bytes);
        const auto&sources=kernel::fresh_bridge::original_sources;
        auto source=std::find_if(sources.begin(),sources.end(),[&](const auto&s){return s.digest==digest;});
        if(source==sources.end())throw std::runtime_error("source_not_original70");
        kernel::boot();for(const auto&[key,id]:kernel::g_hle_ids)known_boot_ids.insert(id);
        module_id=load_module_buffer(bytes,route=="buffer"?"buffer":source->filename_name,"",[&](int id,int result){
            if(id!=module_id)throw std::runtime_error("actual_exit_module_id_changed");++done_count;module_result=result;
        });
        if(module_id<=0)throw std::runtime_error("actual_source_loader_failed");
        if(!compiled)execution_override=[](Cpu&c,std::int64_t&budget){reference_execute(c,budget);};
        // This is the original run()/scheduler/start_thread/on_exit path,
        // not manual entry state construction or a copied instruction loop.
        ps2::iop::run(12000);
        if(done_count!=1 || kernel::thread(1)->on_exit)throw std::runtime_error("actual_module_exit_closure_not_completed");
        const auto&loaded=kernel::modules().at(module_id);aot::BoundPic bound;
        if(!bound.bind(*source->code,loaded.base))throw std::runtime_error("reference_view_bind_failed");
        for(const auto&r:source->records)if(r.kind==ps2::iop::metadata_guard::Kind::exports)
            for(std::size_t i=5;i+1<r.words.size();++i){auto at=r.words[i];
                if(at+8<=bound.module.text_size && bound.words[at/4]==0x03e00008u && bound.words[at/4+1]==0){leaf=at;leaf_word=bound.words[at/4];leaf_delay=bound.words[at/4+1];break;}}
        if(!leaf)throw std::runtime_error("actual_source_JR_NOP_export_unavailable");
        for(unsigned i=1;i<32;++i)g_cpu.r[i]=0x56780000u+0x101u*i;g_cpu.r[0]=0;g_cpu.pc=0x12345678;g_cpu.npc=g_cpu.pc+4;
        const Cpu before=g_cpu;
        kernel::register_interrupt(9,loaded.base+leaf,0x77889900,0);kernel::enable_interrupt(9,true);ps2::iop::raise_interrupt(9);kernel::deliver_interrupts();
        irq_equal=std::memcmp(&before,&g_cpu,sizeof before)==0;
        if(!irq_equal)throw std::runtime_error("production_IRQ_current_Cpu_changed");
        execution_override={};
        save(output+".cpu",&g_cpu,sizeof g_cpu);save(output+".ram",g_ram,kRamSize);
        ps2::state::Writer state;state.set_version(8);ps2::iop::state(state);save(output+".iop-state",state.out.data(),state.out.size());
        if(compiled){
            auto fresh=[&](){
                kernel::boot();known_boot_ids.clear();for(const auto&[key,id]:kernel::g_hle_ids)known_boot_ids.insert(id);
                const auto id=load_module_buffer(bytes,route=="buffer"?"buffer":source->filename_name,"",{});
                if(id<=0)throw std::runtime_error("control_actual_loader_failed");return kernel::modules().at(id);
            };
            auto step=[&](std::uint32_t pc){Cpu c{};c.pc=pc;c.npc=pc+4;c.r[31]=kernel::g_return_stub;std::int64_t budget=1;kernel::execute_aot_v8(c,budget);};
            auto rejected=[&](const char*reason,const auto&action){
                try{action();}catch(const std::exception&e){if(std::string(e.what()).find(reason)!=std::string::npos){++production_controls;control_reasons.emplace_back(e.what());return;}throw;}
                throw std::runtime_error(std::string("production_control_accepted:")+reason);
            };
            auto poisoned=[&](){rejected("IOP_FRESH_REFUSED:execution_after_",[&]{step(kernel::g_return_stub);});};
            {
                const auto m=fresh();const auto at=m.base+leaf;step(at);const auto word=ram32(at);set_ram32(at,word^1u);
                rejected("original_instruction_changed_requires_precompiled_variant",[&]{step(at);});set_ram32(at,word);
                poisoned(); // Restoring the old bytes must not resurrect the generation.
            }
            {
                const auto m=fresh();step(m.base+leaf);auto duplicate=m;duplicate.id=0x1234;kernel::g_modules.emplace(duplicate.id,duplicate);
                rejected("overlapping_live_execution_facts",[&]{step(m.base+leaf);});poisoned();
            }
            {
                const auto m=fresh();kernel::register_interrupt(9,m.base+leaf,0,0);kernel::enable_interrupt(9,true);
                kernel::free(m.base);ps2::iop::raise_interrupt(9);
                rejected("stale_fresh_owner_generation",[&]{kernel::deliver_interrupts();});poisoned();
            }
            {
                const auto m=fresh();const auto old=ram32(m.base+leaf);set_ram32(m.base+leaf,old^1u);
                rejected("IOP_METADATA_REFUSED:word_identity_changed",[&]{kernel::call_guest(m.base+leaf);});
                set_ram32(m.base+leaf,old);poisoned();
            }
            // A failure AFTER successful IOP parsing (e.g. later EE/VU/GS or
            // paired metadata) must revoke even otherwise valid kernel stubs.
            kernel::boot();kernel::r192_fresh_before_machine_restore();
            ps2::state::Reader complete_iop(state.out.data(),state.out.size());complete_iop.set_version(8);ps2::iop::state(complete_iop);
            kernel::r192_fresh_after_machine_restore(false);poisoned();
            kernel::boot();
        }
        // Exercise real progressive IOP restore with the same public outer
        // guards that the full-machine owner will call. Success does not
        // manufacture fresh history; later failure poisons even kernel stubs.
        kernel::r192_fresh_before_machine_restore();ps2::state::Reader restored(state.out.data(),state.out.size());restored.set_version(8);
        ps2::iop::state(restored);kernel::r192_fresh_after_machine_restore(true);
        if(!kernel::fresh_bridge::restored_legacy())throw std::runtime_error("restored_route_not_explicitly_legacy");
        if(compiled){
            // Constructed import site tests only the preserved OLD identity
            // policy against a provider table from the actual completed start.
            // This does not admit fresh ownership or SDK registration history.
            const auto at=kRamSize-8;const auto saved0=ram32(at),saved1=ram32(at+4);bool checked=false;
            for(const auto&r:source->records)if(r.kind==ps2::iop::metadata_guard::Kind::exports){
                std::string name;for(unsigned i=0;i<8;++i){auto ch=static_cast<char>(r.words[3+i/4]>>(8*(i%4)));if(!ch)break;name+=ch;}
                auto found=kernel::g_libraries.find(name);if(found==kernel::g_libraries.end() || r.words.size()<8)continue;
                const auto target=ram32(phys(found->second)+24);aot::Import entry{0,name.c_str(),0,1};
                set_ram32(at,0x08000000u|((target>>2)&0x03ffffffu));set_ram32(at+4,0);
                if(!kernel::identity(nullptr,at,entry) || !kernel::fresh_bridge::validate_dispatch_import(at,entry))throw std::runtime_error("restored_old_export_identity_policy_regressed");
                ++production_controls;control_reasons.emplace_back("restored_legacy_original_export_identity_retained");
                set_ram32(at,ram32(at)^1u);
                if(kernel::fresh_bridge::validate_dispatch_import(at,entry))throw std::runtime_error("restored_wrong_export_target_accepted");
                ++production_controls;control_reasons.emplace_back("restored_wrong_export_target_rejected");checked=true;break;
            }
            set_ram32(at,saved0);set_ram32(at+4,saved1);if(!checked)throw std::runtime_error("actual_started_export_provider_unavailable");
        }
        kernel::r192_fresh_before_machine_restore();bool failed=false;
        try{ps2::state::Reader short_state(state.out.data(),state.out.size()-1);short_state.set_version(8);ps2::iop::state(short_state);}
        catch(const std::exception&e){if(std::string(e.what())!="state file is truncated")throw;failed=true;}
        kernel::r192_fresh_after_machine_restore(!failed);
        if(!failed)throw std::runtime_error("partial_restore_control_not_exercised");
        bool poisoned=false;try{kernel::fresh_bridge::instruction_boundary();}catch(const std::exception&e){poisoned=std::string(e.what()).find("IOP_FRESH_REFUSED:")==0;}
        if(!poisoned)throw std::runtime_error("partial_restore_allowed_IOP_stub_execution");
    }catch(const std::exception&e){error=e.what();execution_override={};}
    std::ofstream result(output+".json");result<<"{\"status\":"<<std::quoted(error.empty()?"PASS":"REFUSED")<<",\"reason\":"<<std::quoted(error)
        <<",\"actual_scheduler_exit_callbacks\":"<<done_count<<",\"actual_module_result\":"<<module_result<<",\"IRQ_current_Cpu_preserved\":"<<(irq_equal?"true":"false")
        <<",\"real_export_leaf_offset\":"<<leaf<<",\"real_export_words\":["<<leaf_word<<","<<leaf_delay<<"],\"production_controls\":"<<production_controls<<",\"control_reasons\":[";
    for(std::size_t i=0;i<control_reasons.size();++i){if(i)result<<',';result<<std::quoted(control_reasons[i]);}result<<"],\"native_credit\":0}\n";
    return error.empty()?0:1;
}
