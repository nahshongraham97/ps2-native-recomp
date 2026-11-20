#include "mka/material_maintenance.hpp"
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::game;
namespace {
bool fail_next=false;std::function<void()>* allocation_probe=nullptr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
}
void* operator new(std::size_t n){if(allocation_probe){auto* f=allocation_probe;allocation_probe=nullptr;(*f)();}if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
const MaterialTrackingSeed seed{{0x40000000,0x10000000},{{{11,12,13},{21,22,23}}}};
const MaterialStepDelta unit{{0x3f800000}};
const MaterialTranslationRates both{{0x3e800000},{0xbf000000},{0x3e000000},{0x3e800000}};
const MaterialTranslationRates first_only{{0x3e800000},{0},{0},{0}};
std::shared_ptr<NativeMatFxBinding> material(MatFxMode mode=MatFxMode::disabled){
 MatFxImportedState initial(mode,{});if(mode==MatFxMode::parameter_pair)initial.slots[0].kind=MatFxEffectKind::parameter_pair;
 MatFxBindingFailure f;auto value=NativeMatFxBinding::create(std::move(initial),f);check(bool(value),"owned source material data");return std::shared_ptr<NativeMatFxBinding>(std::move(value));
}
std::shared_ptr<NativeMaterialMaintenance> registry(){MaterialMaintenanceFailure f;auto p=NativeMaterialMaintenance::create(f);check(bool(p),"registry create");return p;}
std::shared_ptr<NativeMaterialGroup> group(std::vector<std::shared_ptr<NativeMatFxBinding>> m){MaterialMaintenanceFailure f;auto p=NativeMaterialGroup::create(std::move(m),f);check(bool(p),"group create");return p;}
std::shared_ptr<NativeTrackedMaterialOwner> owner(std::vector<std::shared_ptr<NativeMaterialGroup>> g={}){MaterialMaintenanceFailure f;auto p=NativeTrackedMaterialOwner::create(std::move(g),f);check(bool(p),"owner create");return p;}
MatFxParameterPairLease pair(const std::shared_ptr<NativeMatFxBinding>& m){MatFxBindingFailure f;auto p=m->first_parameter_pair(f);check(bool(p),"material actual parameter pair");return *p;}
void direct_and_order(){
 auto r=registry();auto o=owner();auto m=material();MaterialMaintenanceFailure f;
 check(!r->register_direct({},m,both,seed,f)&&f.kind==MaterialMaintenanceError::none,"source null owner is absence, not fabricated record");
 check(!r->register_direct(o,{},both,seed,f)&&f.kind==MaterialMaintenanceError::none,"source null target is absence");
 auto token=r->register_direct(o,m,both,seed,f);check(bool(token),"direct native source registration");
 auto initial=r->record(*token,f);check(initial&&initial->matrices.first.translation.x.bits==0&&initial->matrices.first.flags==0x40020003&&initial->matrices.first.opaque_padding==seed.padding[0],"source identity and explicit prior flag/padding words");
 auto p=r->step(unit,f);check(p&&p->visited==1&&p->advances==1&&p->pair_publications==2,"actual direct both-layer publication");
 auto held=pair(m);check(held.first&&held.second&&held.first->translation.x.bits==0x3e800000&&held.first->translation.y.bits==0xbf000000&&held.second->translation.x.bits==0x3e000000,"literal first native parameter update");
 check(held.first->flags==0x40000003&&held.second->flags==0x10000003,"only source identity bit cleared");
 auto first_identity=held.first.get(),second_identity=held.second.get();p=r->step(unit,f);held=pair(m);
 check(p&&held.first.get()==first_identity&&held.second.get()==second_identity&&held.first->translation.x.bits==0x3f000000&&held.first->translation.y.bits==0x80000000,"owned identity stable and signed remainder at negative integer");
 check(r->retire(*token,f)&&!r->record(*token,f)&&f.kind==MaterialMaintenanceError::stale_record,"retired handle never reused");
 check(held.first->translation.x.bits==0x3f000000,"explicit read lease remains safe beyond retirement");
 auto only=material(MatFxMode::parameter_pair);auto other=std::make_shared<NativeMaterialMatrix>(initial->matrices.second);std::shared_ptr<const NativeMaterialMatrix> other_const=other;MatFxBindingFailure bf;
 check(only->set_first_parameter_pair({MatFxMatrixReference::null(),MatFxMatrixReference::borrow(other_const)},bf),"explicit opposite parameter");
 auto old=r->register_direct(o,only,first_only,seed,f);auto newer=r->register_direct(o,only,{{0x3f000000},{0},{0},{0}},seed,f);check(old&&newer,"two records share material without guessed unique-target rule");
 auto order=r->order(f);check(order&&order->size()==2&&(*order)[0]==r->record(*newer,f)->id&&(*order)[1]==r->record(*old,f)->id,"source prepend order");
 p=r->step(unit,f);held=pair(only);check(p&&p->advances==2&&held.first->translation.x.bits==0x3e800000&&held.second.get()==other.get(),"newest walks first, older publishes last, opposite identity preserved");
 auto foreign=registry();check(!foreign->retire(*old,f)&&f.kind==MaterialMaintenanceError::foreign_owner,"actual native registry identity");
 check(r->clear(f)&&r->order(f)->empty(),"source global cleanup releases all records");
}
void groups_and_lifetimes(){
 auto r=registry();auto m0=material(MatFxMode::parameter_pair),m1=material(MatFxMode::parameter_pair);auto g=group({m0,m1});auto empty=group({});auto o=owner({g,empty});MaterialMaintenanceFailure f;
 auto t=r->register_prepared_group(o,g,both,seed,f);check(bool(t),"source explicit nonnull group route");
 auto p=r->step(unit,f);auto snap=r->record(*t,f);
 check(p&&p->advances==1&&p->pair_publications==4&&snap&&snap->matrices.first.translation.x.bits==0x3e800000,"explicit group advances once before two ordered publication passes");
 auto a=pair(m0),b=pair(m1);check(a.first.get()==b.first.get()&&a.second.get()==b.second.get()&&a.first->translation.x.bits==0x3e800000,"explicit group materials borrow same actual parameter identities");
 check(o->invalidate_generation(f),"associated generation changes independently");p=r->step(unit,f);
 check(p&&p->skipped==1&&p->retired==0&&p->advances==0&&r->record(*t,f)&&!r->record(*t,f)->associated_live,"valid tracking record with expired associated owner skips without rebinding");
 auto direct=r->register_direct(o,m0,first_only,seed,f);check(bool(direct),"direct lifetime attaches to current owner generation");check(o->invalidate_generation(f),"direct owner expires");p=r->step(unit,f);
 check(p&&p->retired==1&&p->skipped==1&&!r->record(*direct,f)&&f.kind==MaterialMaintenanceError::stale_record,"dead direct tracking record retires; hierarchy remains separately owned");
 check(r->clear(f),"separate lifetimes cleanup");
 auto null_target=r->register_owner_collection(o,both,seed,f);check(bool(null_target),"collection record is distinct data, not proof of publication target");
 check(!r->step(unit,f)&&f.kind==MaterialMaintenanceError::unbound_collection,"selected nonempty null-target collection fails by name");
 check(r->record(*null_target,f)->matrices.first.translation.x.bits==0,"unproved route performs no invented advance or target substitution");
 check(r->clear(f),"unbound collection cleanup");
 auto no_objects=owner();auto no_visit=r->register_owner_collection(no_objects,both,seed,f);p=r->step(unit,f);
 check(no_visit&&p&&p->advances==0&&p->pair_publications==0,"empty collection makes no source callback");
 check(r->clear(f),"empty collection cleanup");
 auto no_layers=r->register_owner_collection(o,{{0},{0},{0},{0}},seed,f);p=r->step(unit,f);
 check(no_layers&&p&&p->advances==0&&p->pair_publications==0,"disabled collection callback has no publication target read");
 check(r->clear(f),"disabled collection cleanup");
 auto empty_target=r->register_prepared_group(o,empty,first_only,seed,f);p=r->step(unit,f);
 check(empty_target&&p&&p->advances==1&&p->pair_publications==0&&r->record(*empty_target,f)->matrices.first.translation.x.bits==0x3e800000,"explicit empty group still advances before visiting zero materials");
 check(r->clear(f),"explicit empty group cleanup");
 auto explicit_group=group({m0});auto explicit_token=r->register_prepared_group(o,explicit_group,first_only,seed,f);check(bool(explicit_token),"explicit target route");explicit_group.reset();
 const auto expected_id=r->record(*explicit_token,f)->id;
 check(!r->step(unit,f)&&f.kind==MaterialMaintenanceError::expired_target&&f.record==expected_id,"expired explicit target never falls back to owner collection");
 check(r->clear(f),"expired target cleanup");
 auto zero=r->register_prepared_group(o,g,{{0},{0},{0},{0}},seed,f);check(bool(zero),"zero-rate record still registered");p=r->step(unit,f);
 check(p&&p->advances==0&&p->pair_publications==0&&r->record(*zero,f)->matrices.first.flags==0x40020003,"disabled source layers leave matrices and binding untouched");
 check(r->clear(f),"zero cleanup");
 auto bad=material();auto bad_group=group({bad});auto bad_token=r->register_prepared_group(o,bad_group,first_only,seed,f);check(bool(bad_token),"update route does not pretend resource preparation occurred");
 check(!r->step(unit,f)&&f.kind==MaterialMaintenanceError::binding&&f.binding.kind==MatFxBindingError::missing_slot,"unprepared material fails by name, no dummy hierarchy success");
}
void step_policy(){
 MaterialMaintenanceFailure f;auto s=NativeMaterialGameStep::create({false,false,50},f);check(bool(s),"owned explicit source step policy");
 auto v=s->snapshot(f);check(v&&v->material.value.bits==0x3f99999a&&v->companion_a.bits==0x3f555555&&v->companion_b.bits==0x3f8c378c,"source50 literal tuple");
 for(auto p:{MaterialGameStepPolicy{false,true,50},MaterialGameStepPolicy{true,false,50}}){check(s->refresh(p,f)&&s->snapshot(f)->material.value.bits==0x3f99999a,"source OR admission");}
 for(auto p:{MaterialGameStepPolicy{true,true,50},MaterialGameStepPolicy{false,false,60},MaterialGameStepPolicy{false,false,59}}){check(s->refresh(p,f)&&s->snapshot(f)->material.value.bits==0x3f800000,"software override/non50 source rule, no host monitor query");}
 bool rejected=false;std::thread t([&]{MaterialMaintenanceFailure e;rejected=!s->snapshot(e)&&e.kind==MaterialMaintenanceError::wrong_thread;});t.join();check(rejected,"step owner thread");
}
void allocation_reentry_and_count(){
 MaterialMaintenanceFailure f;fail_next=true;auto absent=NativeMaterialMaintenance::create(f);check(!absent&&f.kind==MaterialMaintenanceError::allocation,"actual registry allocation failure");
 auto r=registry();auto o=owner();auto m=material();fail_next=true;auto no=r->register_direct(o,m,both,seed,f);MatFxBindingFailure bf;
 check(!no&&f.kind==MaterialMaintenanceError::allocation&&m->snapshot(bf)->mode==MatFxMode::disabled&&r->order(f)->empty(),"allocation failure publishes no node or material conversion");
 bool nested_registry=false,nested_owner=false;
 std::function<void()> probe=[&]{MaterialMaintenanceFailure local;nested_registry=!r->order(local)&&local.kind==MaterialMaintenanceError::reentry;nested_owner=!o->invalidate_generation(local)&&local.kind==MaterialMaintenanceError::reentry;};
 allocation_probe=&probe;auto first=r->register_direct(o,m,first_only,seed,f);check(first&&nested_registry&&nested_owner,"allocation hook cannot mutate either lifetime or tracking generation");
 auto second=r->register_direct(o,m,first_only,seed,f);check(second&&r->record(*first,f)->id!=r->record(*second,f)->id,"nested rejection leaves distinct native record identities");
 fail_next=true;check(!o->invalidate_generation(f)&&f.kind==MaterialMaintenanceError::allocation,"owner generation allocation failure");check(r->record(*first,f)->associated_live,"failed invalidation preserves native association");
 bool thread_rejected=false;std::thread thread([&]{MaterialMaintenanceFailure e;thread_rejected=!r->step(unit,e)&&e.kind==MaterialMaintenanceError::wrong_thread;});thread.join();check(thread_rejected,"registry thread ownership");
 check(r->clear(f),"dynamic test starts empty");
 for(unsigned i=0;i<512;++i)check(r->register_direct(o,m,first_only,seed,f).has_value(),"host-owned dynamic tracking count");
 auto p=r->step(unit,f);check(p&&p->visited==512&&p->advances==512&&p->pair_publications==512,"all native records execute without source memory-pool capacity");
}
struct Finalizer final:NativeMatFxProperty {std::function<void()> done;explicit Finalizer(std::function<void()> f):done(std::move(f)){}~Finalizer()noexcept override{done();}};
void owner_release_guard(){
 std::weak_ptr<NativeTrackedMaterialOwner> weak;bool rejected=false;
 auto prop=std::make_shared<Finalizer>([&]{if(auto owner=weak.lock()){MaterialMaintenanceFailure f;rejected=!owner->invalidate_generation(f)&&f.kind==MaterialMaintenanceError::reentry;}});
 std::array<MatFxEffectSlot,2> slots{};slots[0].kind=MatFxEffectKind::property;slots[0].primary=prop;slots[0].selectors={5,6};MatFxBindingFailure bf;
 auto unique=NativeMatFxBinding::create({MatFxMode::property,std::move(slots)},bf);check(bool(unique),"owned finalizer binding");auto m=std::shared_ptr<NativeMatFxBinding>(std::move(unique));auto g=group({m});auto o=owner({g});weak=o;
 prop.reset();m.reset();g.reset();MaterialMaintenanceFailure f;check(o->replace_groups({},f)&&rejected,"displaced native resources finalize inside associated-owner mutation guard");
}
}
int main(){try{direct_and_order();groups_and_lifetimes();step_policy();allocation_reentry_and_count();owner_release_guard();
 std::puts("PASS native populated material tracking/publication/lifetimes/game step; no hierarchy preparation or renderer bootstrap");return 0;
}catch(const std::exception& e){allocation_probe=nullptr;fail_next=false;std::fprintf(stderr,"FAIL native material maintenance %s\n",e.what());return 1;}}
