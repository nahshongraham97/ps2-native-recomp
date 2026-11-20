#include "mka/matfx_binding.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>
using namespace mka::game;
namespace {bool fail_next=false;void check(bool value,const char* what){if(!value)throw std::runtime_error(what);}}
void* operator new(std::size_t n){if(fail_next){fail_next=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
struct Resource final:NativeMatFxProperty {
 int& releases;NativeMatFxBinding* probe=nullptr;bool* rejected=nullptr;
 explicit Resource(int& count):releases(count){}
 ~Resource() noexcept override {++releases;if(probe){MatFxBindingFailure f;const auto view=probe->snapshot(f);*rejected=!view&&f.kind==MatFxBindingError::reentry;}}
};
MatFxImportedState imported(MatFxMode mode){
 std::array<MatFxEffectSlot,2> slots;
 switch(mode){
 case MatFxMode::disabled:break;
 case MatFxMode::effect1:slots[0].kind=MatFxEffectKind::effect1;break;
 case MatFxMode::effect2:slots[0].kind=MatFxEffectKind::effect2;break;
 case MatFxMode::combined12:slots[0].kind=MatFxEffectKind::effect1;slots[1].kind=MatFxEffectKind::effect2;break;
 case MatFxMode::property:slots[0].kind=MatFxEffectKind::property;break;
 case MatFxMode::parameter_pair:slots[0].kind=MatFxEffectKind::parameter_pair;break;
 case MatFxMode::parameter_pair_property:slots[0].kind=MatFxEffectKind::parameter_pair;slots[1].kind=MatFxEffectKind::property;break;
 }return {mode,std::move(slots)};
}
auto binding(MatFxMode mode){MatFxBindingFailure f;auto owner=NativeMatFxBinding::create(imported(mode),f);check(bool(owner)&&f.kind==MatFxBindingError::none,"actual owned import");return owner;}
void direct_property_and_cleanup(){
 MatFxBindingFailure f;int releases=0;auto property=std::make_shared<Resource>(releases);auto state=imported(MatFxMode::property);
 state.slots[0].primary=property;state.slots[0].selectors={-7,123};auto owner=NativeMatFxBinding::create(std::move(state),f);
 check(bool(owner),"import real owned property");property.reset();
 check(owner->prepare_direct_tracking(f),"actual4->6 conversion");
 auto view=owner->snapshot(f);check(view&&view->mode==MatFxMode::parameter_pair_property&&view->slots[0].kind==MatFxEffectKind::parameter_pair&&view->slots[1].kind==MatFxEffectKind::property,"ordered6 slots are5 then4");
 auto pair=owner->first_parameter_pair(f);check(pair&&!pair->first&&!pair->second,"fresh kind5 references explicitly null");
 auto selectors=owner->property_selectors(f);check(selectors&&selectors->first==-7&&selectors->second==123&&releases==0,"property and both selectors retained across conversion");
 view.reset();check(owner->set_property_selectors({5,6},f),"actual selector update");
 selectors=owner->property_selectors(f);check(selectors&&selectors->first==5&&selectors->second==6,"serialized selectors retained without guessed renderer behavior");
 auto retained=owner->first_property(f);check(retained&&bool(*retained),"strong temporary property lease");
 check(owner->prepare_direct_tracking(f)&&owner->snapshot(f)->mode==MatFxMode::parameter_pair&&releases==0,"old6 follows else branch to5; returned property lease keeps resource alive");
 retained.reset();check(releases==1,"last native property owner actually finalizes resource");
 check(!owner->first_property(f)&&f.kind==MatFxBindingError::missing_slot,"no fake missing property");
 auto empty=binding(MatFxMode::property);check(!empty->prepare_direct_tracking(f)&&f.kind==MatFxBindingError::missing_property&&empty->snapshot(f)->mode==MatFxMode::property,"unsafe source null conversion fails without mutation");
}
void borrowed_pairs(){
 MatFxBindingFailure f;auto owner=binding(MatFxMode::disabled);check(owner->prepare_direct_tracking(f),"normal fresh mode prepares5");
 auto first=std::make_shared<NativeMaterialMatrix>();auto second=std::make_shared<NativeMaterialMatrix>();
 first->translation.x.bits=0x3e800000;second->flags=0x12345678;second->opaque_padding={4,5,6};
 MatFxParameterPair refs{MatFxMatrixReference::borrow(first),MatFxMatrixReference::borrow(second)};
 check(owner->set_first_parameter_pair(refs,f)&&first.use_count()==1&&second.use_count()==1,"binding borrows matrices, never retains them");
 check(owner->prepare_direct_tracking(f),"repeated source mode5 preparation preserves existing pair");
 auto held=owner->first_parameter_pair(f);check(held&&held->first.get()==first.get()&&held->second.get()==second.get()&&held->first->translation.x.bits==0x3e800000,"getter provides exact temporary live leases");
 auto replacement=std::make_shared<NativeMaterialMatrix>();replacement->flags=99;
 check(owner->set_first_parameter_pair({MatFxMatrixReference::borrow(replacement),MatFxMatrixReference::borrow(held->second)},f),"replace first preserving actual opposite reference");
 held.reset();auto actual=owner->first_parameter_pair(f);check(actual&&actual->first.get()==replacement.get()&&actual->second.get()==second.get()&&actual->second->opaque_padding[2]==6,"opposite identity and opaque values preserved");
 actual.reset();replacement.reset();
 check(!owner->first_parameter_pair(f)&&f.kind==MatFxBindingError::stale_matrix,"nonnull expired first differs from actual null");
 auto live=std::make_shared<NativeMaterialMatrix>();check(owner->set_first_parameter_pair({MatFxMatrixReference::null(),MatFxMatrixReference::borrow(live)},f),"explicit null replacement is valid");
 auto expired=std::make_shared<NativeMaterialMatrix>();auto stale=MatFxMatrixReference::borrow(expired);expired.reset();
 check(!owner->set_first_parameter_pair({MatFxMatrixReference::null(),stale},f)&&f.kind==MatFxBindingError::stale_matrix,"expired setter rejected transactionally");
 auto unchanged=owner->first_parameter_pair(f);check(unchanged&&!unchanged->first&&unchanged->second.get()==live.get(),"failed setter preserves both stored references");
 live.reset();check(bool(unchanged->second),"temporary lease survives source matrix owner release");unchanged.reset();
 check(!owner->first_parameter_pair(f)&&f.kind==MatFxBindingError::stale_matrix,"expired second named independently");
 check(owner->set_first_parameter_pair({},f),"explicit null pair clears stale borrows");
 auto after=owner->first_parameter_pair(f);check(after&&!after->first&&!after->second,"null pair is success, not unavailable");
}
void previous_mode_cleanup(){
 for(auto mode:{MatFxMode::effect1,MatFxMode::effect2,MatFxMode::combined12,MatFxMode::parameter_pair_property}){
  int count=0;auto state=imported(mode);int expected=0;
  for(auto& slot:state.slots){if(slot.kind==MatFxEffectKind::effect1){slot.primary=std::make_shared<Resource>(count);slot.secondary=std::make_shared<Resource>(count);expected+=2;}
   else if(slot.kind==MatFxEffectKind::effect2){slot.secondary=std::make_shared<Resource>(count);++expected;}
   else if(slot.kind==MatFxEffectKind::property){slot.primary=std::make_shared<Resource>(count);++expected;}}
  MatFxBindingFailure f;auto owner=NativeMatFxBinding::create(std::move(state),f);check(bool(owner),"known previous mode owned import");
  check(owner->prepare_direct_tracking(f)&&count==expected&&owner->snapshot(f)->mode==MatFxMode::parameter_pair,"all displaced effect resources finalize before mode5 success");
 }
}
void errors_and_lifetime(){
 static_assert(std::is_abstract_v<NativeMatFxProperty>);MatFxBindingFailure f;
 auto invalid=imported(MatFxMode::disabled);invalid.slots[0].kind=MatFxEffectKind::parameter_pair;
 check(!NativeMatFxBinding::create(std::move(invalid),f)&&f.kind==MatFxBindingError::invalid_state,"inconsistent slot order rejected");
 auto unknown=imported(static_cast<MatFxMode>(99));check(!NativeMatFxBinding::create(std::move(unknown),f)&&f.kind==MatFxBindingError::invalid_state,"unknown semantic mode rejected");
 auto state=imported(MatFxMode::property);int count=0;state.slots[0].primary=std::make_shared<Resource>(count);
 fail_next=true;auto failed=NativeMatFxBinding::create(std::move(state),f);check(!failed&&f.kind==MatFxBindingError::allocation&&state.slots[0].primary&&count==0,"actual allocation refusal leaves imported resource ownership intact");
 auto owner=NativeMatFxBinding::create(std::move(state),f);check(bool(owner),"recoverable factory allocation failure");
 auto moved=std::move(owner);check(!owner&&bool(moved),"unique ownership transfers without changing native binding identity");owner=std::move(moved);
 fail_next=true;auto noalloc=owner->snapshot(f);const bool untouched=fail_next;fail_next=false;check(noalloc&&untouched,"fixed semantic snapshot needs no allocation");noalloc.reset();
 check(!owner->first_parameter_pair(f)&&f.kind==MatFxBindingError::missing_slot,"missing kind5 cannot return fabricated null success");
 auto replacement=std::make_shared<Resource>(count);check(!owner->replace_property({},f)&&f.kind==MatFxBindingError::missing_property,"null source resource replacement rejected");
 check(owner->replace_property(replacement,f)&&count==1,"real resource replacement releases displaced owner");
 auto before=owner->snapshot(f);check(!owner->prepare_hierarchy_tracking(f)&&f.kind==MatFxBindingError::unbound_hierarchy,"actual hierarchy dependency is named");auto after=owner->snapshot(f);check(before&&after&&before->mode==after->mode&&before->slots[0].primary==after->slots[0].primary,"unbound hierarchy changes no state");
 bool wrong=false;std::thread thread([&]{MatFxBindingFailure why;wrong=!owner->prepare_direct_tracking(why)&&why.kind==MatFxBindingError::wrong_thread;});thread.join();check(wrong,"foreign thread cannot mutate binding");
 before.reset();after.reset();replacement.reset();auto retained=owner->snapshot(f);owner.reset();check(retained&&count==1&&retained->slots[0].primary,"owned snapshot retains actual resource beyond binding teardown");retained.reset();check(count==2,"retained resource finalizes at actual last native owner");
 std::vector<std::unique_ptr<NativeMatFxBinding>> many;for(int i=0;i<513;++i)many.push_back(binding(MatFxMode::disabled));check(many.size()==513,"semantic two slots do not cap native binding count");
}
void finalizer_reentry(){
 MatFxBindingFailure f;int count=0;bool rejected=false;auto state=imported(MatFxMode::effect1);auto property=std::make_shared<Resource>(count);auto* raw=property.get();state.slots[0].primary=property;property.reset();auto owner=NativeMatFxBinding::create(std::move(state),f);raw->probe=owner.get();raw->rejected=&rejected;
 check(owner->prepare_direct_tracking(f)&&count==1&&rejected,"displaced property destructor cannot reenter active conversion");
 auto state2=imported(MatFxMode::property);auto property2=std::make_shared<Resource>(count);auto* raw2=property2.get();state2.slots[0].primary=property2;property2.reset();auto second=NativeMatFxBinding::create(std::move(state2),f);raw2->probe=second.get();raw2->rejected=&rejected;rejected=false;
 auto next=std::make_shared<Resource>(count);check(second->replace_property(next,f)&&count==2&&rejected,"replacement finalizer runs inside mutation guard");
 next.reset();auto held=second->first_property(f);auto* last=static_cast<Resource*>(held->get());last->probe=second.get();last->rejected=&rejected;held.reset();rejected=false;second.reset();check(count==3&&rejected,"binding destructor blocks resource-finalizer reentry");
}
}
int main(){try{direct_property_and_cleanup();borrowed_pairs();previous_mode_cleanup();errors_and_lifetime();finalizer_reentry();
 check(std::puts("PASS native MatFX owned bindings/borrowed matrix lifetime/resource cleanup; no renderer or hierarchy admission")>=0&&std::fflush(stdout)==0,"checked output");return 0;
}catch(const std::exception& e){fail_next=false;std::fprintf(stderr,"FAIL native MatFX binding %s\n",e.what());return 1;}}
