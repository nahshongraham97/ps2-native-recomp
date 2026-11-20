#include "mka/material_maintenance.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::game;
namespace {
bool fail_next=false;
std::function<void()>* allocation_probe=nullptr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
}
void* operator new(std::size_t n){
    if(allocation_probe){auto* fn=allocation_probe;allocation_probe=nullptr;(*fn)();}
    if(fail_next){fail_next=false;throw std::bad_alloc();}
    if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
const MaterialTrackingSeed seed{{0x40000000,0x10000000},{{{11,12,13},{21,22,23}}}};
const MaterialTranslationRates both{{0x3e800000},{0xbf000000},{0x3e000000},{0x3e800000}};
const MaterialStepDelta unit{{0x3f800000}};
std::shared_ptr<NativeMaterialMaintenance> registry(){
    MaterialMaintenanceFailure f;auto x=NativeMaterialMaintenance::create(f);check(bool(x),"registry");return x;
}
std::shared_ptr<NativeTrackedMaterialOwner> owner(){
    MaterialMaintenanceFailure f;auto x=NativeTrackedMaterialOwner::create({},f);check(bool(x),"owner");return x;
}
std::shared_ptr<NativeMatFxBinding> material(MatFxMode mode=MatFxMode::disabled){
    MatFxImportedState state{mode,{}};
    if(mode==MatFxMode::parameter_pair)state.slots[0].kind=MatFxEffectKind::parameter_pair;
    MatFxBindingFailure f;auto x=NativeMatFxBinding::create(std::move(state),f);check(bool(x),"material");return std::shared_ptr<NativeMatFxBinding>(std::move(x));
}
std::shared_ptr<NativeMaterialGeometry> geometry(std::vector<std::shared_ptr<NativeMatFxBinding>> list,
    MatFxHierarchyRoute route,std::int32_t count){
    MaterialMaintenanceFailure f;auto group=NativeMaterialGroup::create(std::move(list),f);check(bool(group),"group");
    auto x=NativeMaterialGeometry::create(group,{route,count},f);check(bool(x),"geometry");return x;
}
std::shared_ptr<NativeMaterialHierarchy> hierarchy(std::shared_ptr<NativeMaterialGeometry> geo,
    std::int32_t kind=0,bool enabled=false){
    MaterialMaintenanceFailure f;auto x=NativeMaterialHierarchy::create(std::move(geo),{kind,enabled},f);check(bool(x),"hierarchy");return x;
}
void actual_preparation_and_publication(){
    for(std::int32_t count:{-1,0,1,2,3,512}){
        auto r=registry();auto o=owner();auto a=material(),b=material();
        auto geo=geometry({a,b},MatFxHierarchyRoute::plugin_absent,count);auto h=hierarchy(geo);
        MaterialMaintenanceFailure f;auto token=r->register_hierarchy(o,h,both,seed,f);
        check(bool(token)&&h->snapshot(f)->state.effect_enabled,"actual source absent-plugin enable owner");
        check(h->snapshot(f)->geometry.get()==geo.get()&&geo->snapshot(f)->imported.source_count==count,
            "same geometry owner/count, no guessed packet-build condition repair");
        MatFxBindingFailure bf;
        check(a->snapshot(bf)->mode==MatFxMode::parameter_pair&&b->snapshot(bf)->mode==MatFxMode::parameter_pair,
            "source mode0 converted to actual effect5 binding");
        auto pass=r->step(unit,f);auto first=a->first_parameter_pair(bf),second=b->first_parameter_pair(bf);
        check(pass&&pass->advances==1&&pass->pair_publications==4&&first&&second&&
            first->first.get()==second->first.get()&&first->second.get()==second->second.get()&&
            first->first->translation.x.bits==0x3e800000&&first->first->translation.y.bits==0xbf000000&&
            first->first->flags==0x40000003&&first->first->opaque_padding==seed.padding[0],
            "real hierarchy registration advances once and publishes actual shared pairs in source passes");
        const auto identity=first->first.get();pass=r->step(unit,f);first=a->first_parameter_pair(bf);
        check(pass&&first->first.get()==identity&&first->first->translation.x.bits==0x3f000000,
            "native hierarchy matrix identity survives source iterations");
    }
    auto r=registry();auto o=owner();MaterialMaintenanceFailure f;
    auto geo=geometry({},MatFxHierarchyRoute::plugin_present,2);auto h=hierarchy(geo,0x81,false);
    auto token=r->register_hierarchy(o,h,both,seed,f);auto pass=r->step(unit,f);
    check(token&&pass&&pass->advances==1&&pass->pair_publications==0&&!h->snapshot(f)->state.effect_enabled,
        "present-plugin branch skips resource producer and does not change absent-branch enable flag; empty target still advances");
}
void unsupported_and_identity(){
    auto r=registry();auto o=owner();auto m=material();auto geo=geometry({m},MatFxHierarchyRoute::plugin_absent,2);
    auto missing=hierarchy(geo,0x81,false);MaterialMaintenanceFailure f;MatFxBindingFailure bf;
    check(!r->register_hierarchy(o,missing,both,seed,f)&&f.kind==MaterialMaintenanceError::unbound_hierarchy_resource,
        "actual nonzero serialized/native-program body required, no fabricated preparation");
    check(!missing->snapshot(f)->state.effect_enabled&&m->snapshot(bf)->mode==MatFxMode::disabled&&r->order(f)->empty(),
        "unbound branch publishes neither fake resource nor tracking handle");
    auto h=hierarchy(geo);auto token=r->register_hierarchy(o,h,both,seed,f);check(bool(token),"supported exact branch");
    auto id=r->record(*token,f)->id;
    auto retained=h->snapshot(f);h.reset();
    check(retained->geometry.get()==geo.get(),"geometry may remain owned independently of the atomic");
    check(!r->step(unit,f)&&f.kind==MaterialMaintenanceError::expired_target&&f.record==id,
        "expired atomic never rebound to still-live shared geometry");
    check(r->clear(f),"expired tracking cleanup");
    auto first=hierarchy(geo),second=hierarchy(geo);auto t1=r->register_hierarchy(o,first,both,seed,f);
    auto t2=r->register_hierarchy(o,second,{{0x3f000000},{0},{0},{0}},seed,f);
    auto order=r->order(f);check(t1&&t2&&order&&(*order)[0]==r->record(*t2,f)->id&&(*order)[1]==r->record(*t1,f)->id,
        "actual hierarchy records prepend newest first");
    auto pass=r->step(unit,f);auto pair=m->first_parameter_pair(bf);
    check(pass&&pass->advances==2&&pair&&pair->first->translation.x.bits==0x3e800000,
        "shared geometry receives newer then older actual record publication");
    check(o->invalidate_generation(f),"separate associated generation");pass=r->step(unit,f);
    check(pass&&pass->skipped==2&&pass->retired==0&&pass->advances==0,
        "live hierarchy record skips dead associated owner without changing target identity");
}
void ordered_partial_preparation_failure(){
    auto r=registry();auto o=owner();auto first=material();
    MatFxImportedState imported{MatFxMode::property,{}};
    imported.slots[0].kind=MatFxEffectKind::property;
    imported.slots[0].selectors={5,6}; // Explicit unsafe-source NULL property: native names it.
    MatFxBindingFailure bf;auto value=NativeMatFxBinding::create(std::move(imported),bf);
    check(bool(value),"explicit source NULL property state can be imported");
    auto bad=std::shared_ptr<NativeMatFxBinding>(std::move(value));
    auto h=hierarchy(geometry({first,bad},MatFxHierarchyRoute::plugin_absent,2));
    MaterialMaintenanceFailure f;auto token=r->register_hierarchy(o,h,both,seed,f);
    check(!token&&f.kind==MaterialMaintenanceError::binding&&f.binding.kind==MatFxBindingError::missing_property,
        "second material names actual missing property rather than invented resource");
    check(h->snapshot(f)->state.effect_enabled&&first->snapshot(bf)->mode==MatFxMode::parameter_pair&&
        bad->snapshot(bf)->mode==MatFxMode::property&&r->order(f)->empty(),
        "ordered enable/first conversion persist after later failure; no phantom record or rollback claim");
}
void failures_and_reentry(){
    auto r=registry();auto o=owner();auto m=material();auto geo=geometry({m},MatFxHierarchyRoute::plugin_absent,2);auto h=hierarchy(geo);
    MaterialMaintenanceFailure f;MatFxBindingFailure bf;
    check(!r->register_hierarchy({},h,both,seed,f)&&f.kind==MaterialMaintenanceError::none,"source null associated owner is normal absence");
    check(!r->register_hierarchy(o,{},both,seed,f)&&f.kind==MaterialMaintenanceError::invalid_input,"native null target names unsafe source input");
    fail_next=true;auto failed=r->register_hierarchy(o,h,both,seed,f);
    check(!failed&&f.kind==MaterialMaintenanceError::allocation&&r->order(f)->empty()&&
        !h->snapshot(f)->state.effect_enabled&&m->snapshot(bf)->mode==MatFxMode::disabled,
        "outer allocation failure precedes all source preparation side effects");
    bool registry_rejected=false,owner_rejected=false,hierarchy_rejected=false;
    std::function<void()> hook=[&]{MaterialMaintenanceFailure e;
        registry_rejected=!r->order(e)&&e.kind==MaterialMaintenanceError::reentry;
        owner_rejected=!o->invalidate_generation(e)&&e.kind==MaterialMaintenanceError::reentry;
        hierarchy_rejected=!h->snapshot(e)&&e.kind==MaterialMaintenanceError::reentry;
    };
    allocation_probe=&hook;auto token=r->register_hierarchy(o,h,both,seed,f);
    check(token&&registry_rejected&&owner_rejected&&hierarchy_rejected,"real allocation callback cannot reenter any preparation owner");
    bool rejected=false;std::thread worker([&]{MaterialMaintenanceFailure e;
        rejected=!geo->snapshot(e)&&e.kind==MaterialMaintenanceError::wrong_thread&&
            !h->snapshot(e)&&e.kind==MaterialMaintenanceError::wrong_thread;
    });worker.join();check(rejected,"geometry/atomic owner thread");
    check(!NativeMaterialGeometry::create({}, {MatFxHierarchyRoute::plugin_absent,0},f)&&f.kind==MaterialMaintenanceError::invalid_input,"geometry requires owned material group");
    check(!NativeMaterialHierarchy::create({}, {0,false},f)&&f.kind==MaterialMaintenanceError::invalid_input,"atomic requires actual geometry");
}
struct Finalizer final:NativeMatFxProperty {
    std::function<void()> finish;
    explicit Finalizer(std::function<void()> f):finish(std::move(f)){}
    ~Finalizer()noexcept override{finish();}
};
void real_resource_release_guard(){
    auto r=registry();auto o=owner();std::weak_ptr<NativeMaterialHierarchy> weak;
    bool atomic_rejected=false,registry_rejected=false,owner_rejected=false;unsigned releases=0;
    auto property=std::make_shared<Finalizer>([&]{++releases;MaterialMaintenanceFailure e;
        if(auto h=weak.lock())atomic_rejected=!h->snapshot(e)&&e.kind==MaterialMaintenanceError::reentry;
        registry_rejected=!r->order(e)&&e.kind==MaterialMaintenanceError::reentry;
        owner_rejected=!o->invalidate_generation(e)&&e.kind==MaterialMaintenanceError::reentry;
    });
    MatFxImportedState state{MatFxMode::property,{}};state.slots[0].kind=MatFxEffectKind::property;
    state.slots[0].primary=property;state.slots[0].selectors={5,6};MatFxBindingFailure bf;
    auto binding=NativeMatFxBinding::create(std::move(state),bf);check(bool(binding),"real property owner");
    auto m=std::shared_ptr<NativeMatFxBinding>(std::move(binding));
    auto h=hierarchy(geometry({m},MatFxHierarchyRoute::plugin_present,1),0x200,false);weak=h;property.reset();
    MaterialMaintenanceFailure f;auto token=r->register_hierarchy(o,h,both,seed,f);
    check(token&&releases==1&&atomic_rejected&&registry_rejected&&owner_rejected&&m->snapshot(bf)->mode==MatFxMode::parameter_pair,
        "actual force5 resource finalizer executes under atomic/registry/associated-owner mutation guards");
}
}
int main(){try{
    actual_preparation_and_publication();unsupported_and_identity();ordered_partial_preparation_failure();failures_and_reentry();real_resource_release_guard();
    std::puts("PASS native retail hierarchy preparation/registration/owned geometry; serialized program producer and renderer remain open");return 0;
}catch(const std::exception& e){allocation_probe=nullptr;fail_next=false;std::fprintf(stderr,"FAIL native material hierarchy %s\n",e.what());return 1;}}
