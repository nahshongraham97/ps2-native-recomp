// Included after the source-pinned original v8 kernel and private AOT
// dispatcher. Requires the pinned original70 Source catalogue macro.
#ifdef R192_FRESH_DIAGNOSTIC_OWNER_HEADER
#include R192_FRESH_DIAGNOSTIC_OWNER_HEADER
#else
#include "iop/r192_fresh_execution_owner.hpp"
#endif
#include "iop/r192_fresh_execution_hooks.hpp"
#include <exception>
namespace ps2::iop::kernel::fresh_bridge {
namespace {
namespace lo=loader_owner;
namespace fx=fresh_execution;
namespace aot=r192_aot;
namespace md=metadata_guard;
#include R192_FRESH_ORIGINAL_SOURCES
struct Frame { fx::Token token{};std::uint32_t base{},size{},entry{};bool native_stub{}; };
struct Lease { Frame frame;lo::Facts observed;const lo::Source*source{};std::unique_ptr<aot::BoundPic>bound;bool live{}; };
std::unique_ptr<fx::Owners> owners;
std::map<int,Lease> leases;
std::map<int,Frame> thread_frames;
std::map<unsigned,Frame> irq_frames;
std::map<int,Frame> raw_frames;
std::vector<Frame> calls;
bool pending_load{},forced_irq{},open_release_reported{},bridge_poisoned{},link_active{};
unsigned forced_irq_line{};
std::vector<std::pair<int,std::uint32_t>> pending_threads;
[[noreturn]]void refuse(const char*why){
    bridge_poisoned=true;if(owners)owners->poison();
    // The preserved Windows crash handler observes GCC first-chance throws.
    // Print BEFORE throwing so a required refusal cannot become anonymous.
    std::fprintf(stderr,"[iop-fresh] IOP_FRESH_REFUSED:%s pc=%08X ra=%08X thread=%d\n",why,g_cpu.pc,g_cpu.r[31],g_current);std::fflush(stderr);
    throw std::runtime_error(std::string("IOP_FRESH_REFUSED:")+why);
}
lo::Facts fact(int id){auto i=g_modules.find(id);if(i==g_modules.end())refuse("loaded_fact_missing");const auto&m=i->second;return {m.id,m.name,m.base,m.size,m.entry,m.gp};}
std::vector<lo::Facts>facts(){std::vector<lo::Facts>r;for(const auto&[id,m]:g_modules)r.push_back(fact(id));return r;}
std::uint32_t read(std::uint32_t address,unsigned width){
    if(address>=kRamSize || width>kRamSize-address || (width!=1&&width!=4))refuse("owner_reader_bounds");
    std::uint32_t value{};for(unsigned i=0;i<width;++i)value|=std::uint32_t(g_ram[address+i])<<(8*i);return value;
}
fx::Owners&state(){
    if(bridge_poisoned)refuse("kernel_mutation_or_observer_failure_requires_reset");
    if(!owners)owners=std::make_unique<fx::Owners>(original_sources,read,aot::Services{nullptr,identity,nullptr},fact,facts,
        [](const std::string&name){auto i=g_libraries.find(name);return i==g_libraries.end()?0u:i->second;});
    return *owners;
}
bool legacy(){return owners && owners->mode()==fx::Mode::restored_legacy_unproved;}
bool contains(const Frame&frame,std::uint32_t pc){auto p=phys(pc);return frame.base<=p && p-frame.base<frame.size;}
bool same_fact(const lo::Facts&a,const lo::Facts&b){return a.id==b.id && a.name==b.name && a.base==b.base && a.size==b.size && a.entry==b.entry && a.gp==b.gp;}
const aot::BoundPic&code_view(const Frame&frame,std::uint32_t pc){
    const auto current=state().token(frame.token.id);
    if(current.epoch!=frame.token.epoch || current.generation!=frame.token.generation)refuse("stale_execution_context_generation");
    auto i=leases.find(frame.token.id);if(i==leases.end() || !i->second.live || !i->second.bound)refuse("missing_live_execution_lease");
    const auto&lease=i->second;if(!same_fact(lease.observed,fact(frame.token.id)))refuse("live_execution_facts_changed");
    for(const auto&[id,m]:g_modules){
        if(id==frame.token.id || !m.size)continue;
        auto old=leases.find(id);
        if(old!=leases.end() && !old->second.live && same_fact(old->second.observed,fact(id)))continue;
        if(std::uint64_t(m.base)<std::uint64_t(frame.base)+frame.size && std::uint64_t(frame.base)<std::uint64_t(m.base)+m.size)
            refuse("overlapping_live_execution_facts");
    }
    const auto physical=phys(pc);const auto&m=lease.bound->module;
    if((pc&3u) || physical<m.base || physical-m.base>=m.text_size)refuse("execution_PC_not_compiled_text");
    for(const auto&r:lease.source->records)if(physical>=m.base+r.offset && physical-(m.base+r.offset)<r.words.size()*4){
        const auto at=physical-(m.base+r.offset);
        if(r.kind!=md::Kind::client || at<20 || at>=20+8*((r.words.size()-7)/2))refuse("execution_PC_is_original_metadata");
    }
    // This private, owned view is checked immediately before every AOT step.
    // The unchanged dispatcher compares the exact executing original word;
    // imports additionally require current authenticated source linkage.
    // Whole-text/metadata checks remain at loader/registration/link barriers.
    return *lease.bound;
}
bool native_stub(std::uint32_t address){
    std::uint32_t id{};return registered_kernel_stub(phys(address),id);
}
Frame bind_entry(std::uint32_t address,bool original_registration){
    if(native_stub(address))return {{},0,0,address,true};
    Frame*match{};
    for(auto&[id,lease]:leases)if(lease.live && contains(lease.frame,address)){
        if(match)refuse("ambiguous_callback_or_thread_owner");match=&lease.frame;
    }
    if(!match)refuse("callback_or_thread_without_fresh_source");
    if(original_registration)state().enter(match->token,address);
    else{
        auto cached=raw_frames.find(match->token.id);
        if(cached==raw_frames.end()){
            state().enter(address); // Reused ranges require an actual registration token.
            raw_frames.emplace(match->token.id,*match);
        }else code_view(cached->second,address);
    }
    auto result=*match;result.entry=address;return result;
}
const aot::BoundPic&enter_frame(const Frame&frame,std::uint32_t pc){
    if(!frame.native_stub){
        if(contains(frame,pc))return code_view(frame,pc);
        code_view(frame,frame.entry); // Caller lifetime remains live across external code.
    }
    // No implicit rebinding on a reused range. A real loader/thread/IRQ event
    // must have captured a generation for that execution context.
    const auto bound=bind_entry(pc,false);return code_view(bound,pc);
}
}
bool restored_legacy(){return legacy();}
void dispatch_refused()noexcept{bridge_poisoned=true;if(owners)owners->poison();}
void instruction_boundary(){
    if(bridge_poisoned)refuse("execution_after_kernel_mutation_failure");
    if(!owners)return;
    if(owners->mode()==fx::Mode::poisoned)refuse("execution_after_progressive_restore_or_owner_failure");
    if(owners->mode()==fx::Mode::restoring)refuse("execution_during_progressive_machine_restore");
}
const r192_aot::BoundPic&select_fresh(Cpu&cpu){
    if(!calls.empty())return enter_frame(calls.back(),cpu.pc);
    auto thread=thread_frames.find(g_current);
    if(thread!=thread_frames.end())return enter_frame(thread->second,cpu.pc);
    const auto frame=bind_entry(cpu.pc,false);return code_view(frame,cpu.pc);
}
void reset_before_boot(){
    if(!calls.empty())refuse("reset_inside_guest_call_unproved");
    if(owners)owners->reset();leases.clear();thread_frames.clear();irq_frames.clear();raw_frames.clear();pending_threads.clear();
    pending_load=forced_irq=open_release_reported=bridge_poisoned=link_active=false;
    g_r192_pic_bindings.clear();
}
void allocation_erased(std::uint32_t base,std::uint32_t size,bool erased){
    if(!owners || legacy())return;
    state().allocation_freed(base,size,erased);
    if(!erased)return;
    base=phys(base);
    for(auto&[id,lease]:leases)if(lease.live && std::uint64_t(base)<std::uint64_t(lease.frame.base)+lease.frame.size &&
        std::uint64_t(lease.frame.base)<std::uint64_t(base)+size)lease.live=false;
    // Existing callback/thread tokens deliberately remain stale, not rebound
    // by numeric address if a new source image reuses the same allocation.
    g_r192_pic_bindings.clear();
}
void thread_created(int id,std::uint32_t entry){
    if(legacy())return;
    if(pending_load){pending_threads.emplace_back(id,entry);return;}
    thread_frames[id]=bind_entry(entry,true);
}
void irq_registered(unsigned line,std::uint32_t handler){
    if(legacy())return;
    if(!handler){irq_frames.erase(line);return;}
    irq_frames[line]=bind_entry(handler,true);
}
void irq_released(unsigned line){irq_frames.erase(line);}
void before_library_link(){
    if(link_active)refuse("nested_original_link_imports");
    // Initial new-image linkage is authenticated by the post-loader admission.
    // Actual restoration is an observation, never reconstructed SDK history.
    if(!owners || owners->mode()!=fx::Mode::fresh || pending_load)return;
    state().before_original_link_imports();link_active=true;
}
void after_library_link(){if(link_active){state().after_original_link_imports();link_active=false;}}
void note_open_release_service(){
    if(!open_release_reported){std::fprintf(stderr,"[iop-fresh] OPEN_reference_stub:loadcore#7_SDK_release_not_closed\n");open_release_reported=true;}
}
CallScope::CallScope(std::uint32_t address):exceptions(std::uncaught_exceptions()){
    if(legacy())return;
    Frame frame;
    if(forced_irq){
        auto i=irq_frames.find(forced_irq_line);if(i==irq_frames.end() || i->second.entry!=address)refuse("IRQ_registration_generation_missing");
        frame=i->second;if(!frame.native_stub)state().enter(frame.token,address);
    }else frame=bind_entry(address,false);
    calls.push_back(frame);active=true;
}
CallScope::~CallScope()noexcept{
    if(!active)return;
    if(owners && std::uncaught_exceptions()>exceptions)owners->poison();
    if(calls.empty()){if(owners)owners->poison();return;}calls.pop_back();
}
IrqScope::IrqScope(unsigned line,std::uint32_t handler){
    if(legacy())return;
    auto i=irq_frames.find(line);if(i==irq_frames.end() || i->second.entry!=handler)refuse("IRQ_source_registration_unproved");
    if(!i->second.native_stub)state().enter(i->second.token,handler);
    prior_valid=forced_irq;prior_line=forced_irq_line;forced_irq=true;forced_irq_line=line;active=true;
}
IrqScope::~IrqScope()noexcept{if(active){forced_irq=prior_valid;forced_irq_line=prior_line;}}
int load(const std::vector<std::uint8_t>&data,const std::string&path,const std::string&args,std::function<void(int,int)>done){
    if(legacy() || g_hle_modules.count(module_name_of(path)))
        return ps2::iop::r192_actual_load_module_buffer(data,path,args,std::move(done));
    if(pending_load)refuse("nested_original_loader_unproved");
    pending_load=true;pending_threads.clear();int id{};
    try{
        auto actual=[&](const auto&bytes,const auto&name,const auto&arguments){
            id=ps2::iop::r192_actual_load_module_buffer(bytes,name,arguments,done);return id;};
        const auto token=state().load(data,path,args,actual);const auto loaded=fact(id);
        const auto digest=lo::image_sha256(data);const lo::Source*source{};
        for(const auto&s:original_sources)if(s.digest==digest)source=&s;
        if(!source)refuse("fresh_source_catalogue_missing_after_admission");
        auto view=std::make_unique<aot::BoundPic>();if(!view->bind(*source->code,loaded.base))refuse("owned_execution_view_binding_failed");
        Frame frame{token,loaded.base,loaded.size,loaded.entry,false};
        leases[id]=Lease{frame,loaded,source,std::move(view),true};
        if(pending_threads.size()!=1 || pending_threads[0].second!=loaded.entry)refuse("actual_loader_start_thread_contract_changed");
        thread_frames[pending_threads[0].first]=frame;
        pending_threads.clear();pending_load=false;g_r192_pic_bindings.clear();return id;
    }catch(...){if(owners)owners->poison();pending_load=false;throw;}
}
bool validate_dispatch_import(std::uint32_t address,const aot::Import&entry){
    // A successful byte restore intentionally preserves the original AOT
    // identity route; it does not reconstruct fresh provider/SDK history.
    if(legacy())return identity(nullptr,address,entry);
    return state().import_identity(address,entry);
}
MutationScope::MutationScope()noexcept:exceptions(std::uncaught_exceptions()){}
MutationScope::~MutationScope()noexcept{
    if(std::uncaught_exceptions()>exceptions){bridge_poisoned=true;if(owners)owners->poison();}
}
}
namespace ps2::iop::kernel {
void r192_fresh_before_machine_restore(){
    using namespace fresh_bridge;
    if(!calls.empty())refuse("restore_inside_guest_call_unproved");
    state().before_restore();leases.clear();thread_frames.clear();irq_frames.clear();raw_frames.clear();pending_threads.clear();
    pending_load=forced_irq=link_active=false;g_r192_pic_bindings.clear();
}
void r192_fresh_after_machine_restore(bool success){
    if(!success && (fresh_bridge::bridge_poisoned ||
       (fresh_bridge::owners && fresh_bridge::owners->mode()==fresh_execution::Mode::poisoned))){
        if(fresh_bridge::owners)fresh_bridge::owners->poison();return;
    }
    fresh_bridge::state().after_restore(success);
    if(success)std::fprintf(stderr,"[iop-fresh] restored_AOT_legacy_identity_unproved_no_fresh_SDK_history\n");
}
namespace {
const aot_v8::BoundPic&select(Cpu&cpu,std::map<BindingKey,std::uint64_t>&verified){
    if(fresh_bridge::restored_legacy())return legacy_select(cpu,verified);
    return fresh_bridge::select_fresh(cpu);
}
}
}
namespace ps2::iop {
std::int32_t load_module_buffer(const std::vector<std::uint8_t>&data,const std::string&path,const std::string&args,std::function<void(int,int)>done){
    return kernel::fresh_bridge::load(data,path,args,std::move(done));
}
}
