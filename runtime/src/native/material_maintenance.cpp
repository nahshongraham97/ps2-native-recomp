#include "mka/material_maintenance.hpp"
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::game {
namespace {
bool fail(MaterialMaintenanceFailure& f,MaterialMaintenanceError k,const char* op) noexcept {f.kind=k;f.operation=op;return false;}
struct Guard {bool& busy;explicit Guard(bool& b):busy(b){busy=true;}~Guard(){busy=false;}};
MaterialGameStepSnapshot scales(MaterialGameStepPolicy p) noexcept {
    // Source2207E0 is a stored GAME selection with a software override, not
    // the native display refresh rate. Source122AD0 refreshes these values.
    const auto effective=(p.game_flag0&&p.scaling_override)?60:p.stored_game_rate;
    if((!p.game_flag0||!p.scaling_override)&&effective==50)
        return {p,{{0x3f99999a}}, {0x3f555555},{0x3f8c378c}};
    return {p,{{0x3f800000}},{0x3f800000},{0x3f800000}};
}
NativeMaterialMatrix identity_matrix(std::uint32_t flags,std::array<std::uint32_t,3> pad) noexcept {
    return {{{0x3f800000},{0},{0}},{{0},{0x3f800000},{0}},{{0},{0},{0x3f800000}},
        {{0},{0},{0}},flags|0x20003u,pad};
}
bool any(MaterialLayerSelection s) noexcept {return s.first||s.second;}
}
NativeMaterialGameStep::NativeMaterialGameStep(MaterialGameStepPolicy p) noexcept:state_(scales(p)){}
bool NativeMaterialGameStep::access(MaterialMaintenanceFailure& f) const noexcept {
    f={};return owner_==std::this_thread::get_id()||fail(f,MaterialMaintenanceError::wrong_thread,"material step owner thread");
}
std::shared_ptr<NativeMaterialGameStep> NativeMaterialGameStep::create(MaterialGameStepPolicy p,MaterialMaintenanceFailure& f) noexcept {
    f={};try{return std::shared_ptr<NativeMaterialGameStep>(new NativeMaterialGameStep(p));}
    catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material step allocation");return {};}
}
bool NativeMaterialGameStep::refresh(MaterialGameStepPolicy p,MaterialMaintenanceFailure& f) noexcept {if(!access(f))return false;state_=scales(p);return true;}
std::optional<MaterialGameStepSnapshot> NativeMaterialGameStep::snapshot(MaterialMaintenanceFailure& f) const noexcept {if(!access(f))return {};return state_;}

std::shared_ptr<NativeMaterialGroup> NativeMaterialGroup::create(std::vector<std::shared_ptr<NativeMatFxBinding>> values,MaterialMaintenanceFailure& f) noexcept {
    f={};for(const auto& x:values){if(!x){fail(f,MaterialMaintenanceError::invalid_input,"material group null binding");return {};}
        if(!x->snapshot(f.binding)){fail(f,MaterialMaintenanceError::binding,"material group binding owner");return {};}}
    try{return std::shared_ptr<NativeMaterialGroup>(new NativeMaterialGroup(std::move(values)));}
    catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material group allocation");return {};}
}
struct MaterialOwnerGeneration {};
NativeTrackedMaterialOwner::NativeTrackedMaterialOwner(std::vector<std::shared_ptr<NativeMaterialGroup>> groups)
    :groups_(std::move(groups)),generation_(std::make_shared<MaterialOwnerGeneration>()){}
NativeTrackedMaterialOwner::~NativeTrackedMaterialOwner(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();busy_=true;groups_.clear();generation_.reset();}
bool NativeTrackedMaterialOwner::access(MaterialMaintenanceFailure& f) const noexcept {
    f={};if(owner_!=std::this_thread::get_id())return fail(f,MaterialMaintenanceError::wrong_thread,"material associated owner thread");
    if(busy_)return fail(f,MaterialMaintenanceError::reentry,"material associated owner reentry");return true;
}
std::shared_ptr<NativeTrackedMaterialOwner> NativeTrackedMaterialOwner::create(std::vector<std::shared_ptr<NativeMaterialGroup>> groups,MaterialMaintenanceFailure& f) noexcept {
    f={};for(const auto& g:groups)if(!g||g->owner_!=std::this_thread::get_id()){fail(f,MaterialMaintenanceError::invalid_input,"material owner group lifetime/thread");return {};}
    try{return std::shared_ptr<NativeTrackedMaterialOwner>(new NativeTrackedMaterialOwner(std::move(groups)));}
    catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material associated owner allocation");return {};}
}
bool NativeTrackedMaterialOwner::replace_groups(std::vector<std::shared_ptr<NativeMaterialGroup>> groups,MaterialMaintenanceFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    for(const auto& g:groups)if(!g||g->owner_!=owner_)return fail(f,MaterialMaintenanceError::invalid_input,"material replacement group lifetime/thread");
    groups_.swap(groups);groups.clear();return true; // Finalizers run inside the owner guard.
}
bool NativeTrackedMaterialOwner::invalidate_generation(MaterialMaintenanceFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);
    try{auto next=std::make_shared<MaterialOwnerGeneration>();generation_=std::move(next);return true;}
    catch(const std::bad_alloc&){return fail(f,MaterialMaintenanceError::allocation,"material owner generation allocation");}
}

struct NativeMaterialMaintenance::Record {
    std::uint64_t id;MaterialTrackingRoute route;
    std::weak_ptr<NativeTrackedMaterialOwner> owner;
    std::weak_ptr<const MaterialOwnerGeneration> associated_generation;
    std::weak_ptr<NativeMatFxBinding> direct;
    std::weak_ptr<NativeMaterialGroup> group;
    MaterialTranslationRates rates;MaterialLayerSelection selection;
    std::shared_ptr<NativeMaterialMatrix> first,second;
};
NativeMaterialMaintenance::NativeMaterialMaintenance():identity_(std::make_shared<const unsigned char>(0)){}
NativeMaterialMaintenance::~NativeMaterialMaintenance(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();busy_=true;records_.clear();}
std::shared_ptr<NativeMaterialMaintenance> NativeMaterialMaintenance::create(MaterialMaintenanceFailure& f) noexcept {
    f={};try{return std::shared_ptr<NativeMaterialMaintenance>(new NativeMaterialMaintenance);}
    catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material tracking registry allocation");return {};}
}
bool NativeMaterialMaintenance::access(MaterialMaintenanceFailure& f) const noexcept {
    f={};if(owner_!=std::this_thread::get_id())return fail(f,MaterialMaintenanceError::wrong_thread,"material tracking registry owner thread");
    if(busy_)return fail(f,MaterialMaintenanceError::reentry,"material tracking registry reentry");return true;
}
std::optional<MaterialTrackingToken> NativeMaterialMaintenance::register_direct(std::shared_ptr<NativeTrackedMaterialOwner> owner,
    std::shared_ptr<NativeMatFxBinding> target,MaterialTranslationRates rates,MaterialTrackingSeed seed,MaterialMaintenanceFailure& f) noexcept {
    return register_owned(MaterialTrackingRoute::direct_material,std::move(owner),std::move(target),{},rates,seed,f);
}
std::optional<MaterialTrackingToken> NativeMaterialMaintenance::register_prepared_group(std::shared_ptr<NativeTrackedMaterialOwner> owner,
    std::shared_ptr<NativeMaterialGroup> group,MaterialTranslationRates rates,MaterialTrackingSeed seed,MaterialMaintenanceFailure& f) noexcept {
    return register_owned(MaterialTrackingRoute::prepared_group,std::move(owner),{},std::move(group),rates,seed,f);
}
std::optional<MaterialTrackingToken> NativeMaterialMaintenance::register_owner_collection(std::shared_ptr<NativeTrackedMaterialOwner> owner,
    MaterialTranslationRates rates,MaterialTrackingSeed seed,MaterialMaintenanceFailure& f) noexcept {
    return register_owned(MaterialTrackingRoute::owner_collection,std::move(owner),{},{},rates,seed,f);
}
std::optional<MaterialTrackingToken> NativeMaterialMaintenance::register_owned(MaterialTrackingRoute route,
    std::shared_ptr<NativeTrackedMaterialOwner> owner,std::shared_ptr<NativeMatFxBinding> target,
    std::shared_ptr<NativeMaterialGroup> group,MaterialTranslationRates rates,MaterialTrackingSeed seed,MaterialMaintenanceFailure& f) noexcept {
    if(!access(f))return {};Guard guard(busy_);
    if(!owner||(route==MaterialTrackingRoute::direct_material&&!target))return {}; // Proved source absence.
    if(!owner->access(f))return {};Guard owner_guard(owner->busy_);
    if(route==MaterialTrackingRoute::prepared_group&&(!group||group->owner_!=owner_)){
        fail(f,MaterialMaintenanceError::invalid_input,"material prepared target lifetime/thread");return {};}
    if(generation_==std::numeric_limits<std::uint64_t>::max()){
        fail(f,MaterialMaintenanceError::overflow,"material tracking handle generation exhausted");return {};}
    try {
        // All native allocations happen before direct material conversion. An
        // allocator/property-finalizer cannot recursively publish another node.
        auto first=std::make_shared<NativeMaterialMatrix>(identity_matrix(seed.flags[0],seed.padding[0]));
        auto second=std::make_shared<NativeMaterialMatrix>(identity_matrix(seed.flags[1],seed.padding[1]));
        std::list<Record> staged;
        const auto next=generation_+1;
        staged.push_back({next,route,owner,owner->generation_,target,group,rates,select_material_layers(rates),std::move(first),std::move(second)});
        if(route==MaterialTrackingRoute::direct_material&&!target->prepare_direct_tracking(f.binding)){
            fail(f,MaterialMaintenanceError::binding,"material direct registration preparation");return {};}
        records_.splice(records_.begin(),staged);generation_=next;
        return MaterialTrackingToken(identity_,next);
    } catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material tracking registration allocation");}
    catch(const std::length_error&){fail(f,MaterialMaintenanceError::allocation,"material tracking registration extent");}
    return {};
}
bool NativeMaterialMaintenance::find(const MaterialTrackingToken& token,const Record*& out,MaterialMaintenanceFailure& f) const noexcept {
    if(token.owner_!=identity_)return fail(f,MaterialMaintenanceError::foreign_owner,"material tracking foreign handle");
    for(const auto& r:records_)if(r.id==token.id_){
        auto owner=r.owner.lock();auto generation=r.associated_generation.lock();
        if(r.route==MaterialTrackingRoute::direct_material&&(!owner||!generation||generation!=owner->generation_))
            return fail(f,MaterialMaintenanceError::stale_record,"material direct owner lifetime ended");
        out=&r;return true;
    }
    return fail(f,MaterialMaintenanceError::stale_record,"material tracking retired handle");
}
bool NativeMaterialMaintenance::retire(const MaterialTrackingToken& token,MaterialMaintenanceFailure& f) noexcept {
    if(!access(f))return false;Guard guard(busy_);const Record* r=nullptr;if(!find(token,r,f))return false;
    for(auto it=records_.begin();it!=records_.end();++it)if(&*it==r){records_.erase(it);return true;}
    return fail(f,MaterialMaintenanceError::stale_record,"material tracking retired handle");
}
bool NativeMaterialMaintenance::clear(MaterialMaintenanceFailure& f) noexcept {if(!access(f))return false;Guard guard(busy_);records_.clear();return true;}
std::optional<MaterialTrackedSnapshot> NativeMaterialMaintenance::record(const MaterialTrackingToken& token,MaterialMaintenanceFailure& f) const noexcept {
    if(!access(f))return {};Guard guard(busy_);const Record* r=nullptr;if(!find(token,r,f))return {};
    auto owner=r->owner.lock();auto generation=r->associated_generation.lock();
    return MaterialTrackedSnapshot{r->id,r->route,bool(owner&&generation&&generation==owner->generation_),r->rates,r->selection,{*r->first,*r->second}};
}
std::optional<MatFxParameterPair> NativeMaterialMaintenance::parameter_references(const MaterialTrackingToken& token,MaterialMaintenanceFailure& f) const noexcept {
    if(!access(f))return {};Guard guard(busy_);const Record* r=nullptr;if(!find(token,r,f))return {};
    return MatFxParameterPair{MatFxMatrixReference::borrow(r->first),MatFxMatrixReference::borrow(r->second)};
}
std::optional<std::vector<std::uint64_t>> NativeMaterialMaintenance::order(MaterialMaintenanceFailure& f) const noexcept {
    if(!access(f))return {};Guard guard(busy_);
    try{std::vector<std::uint64_t> out;out.reserve(records_.size());for(const auto& r:records_)out.push_back(r.id);return out;}
    catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material tracking order allocation");}
    catch(const std::length_error&){fail(f,MaterialMaintenanceError::allocation,"material tracking order extent");}return {};
}
bool NativeMaterialMaintenance::publish(Record& r,const std::shared_ptr<NativeMatFxBinding>& material,bool first,
    MaterialMaintenancePass& pass,MaterialMaintenanceFailure& f) noexcept {
    if(!material)return fail(f,MaterialMaintenanceError::expired_target,"material publication target expired");
    auto old=material->first_parameter_pair(f.binding);
    if(!old)return fail(f,MaterialMaintenanceError::binding,"material read actual parameter pair");
    MatFxParameterPair pair{MatFxMatrixReference::borrow(first?r.first:old->first),MatFxMatrixReference::borrow(first?old->second:r.second)};
    if(!material->set_first_parameter_pair(pair,f.binding))return fail(f,MaterialMaintenanceError::binding,"material publish actual parameter pair");
    ++pass.pair_publications;return true;
}
bool NativeMaterialMaintenance::advance_group(Record& r,const NativeMaterialGroup& group,MaterialStepDelta delta,
    MaterialMaintenancePass& pass,MaterialMaintenanceFailure& f) noexcept {
    if(group.owner_!=owner_)return fail(f,MaterialMaintenanceError::wrong_thread,"material group owner thread");
    const auto updated=translate_material_layers({*r.first,*r.second},r.rates,delta,r.selection);
    *r.first=updated.first;*r.second=updated.second;++pass.advances;
    // Source combined path publishes first across the list, then second.
    if(r.selection.first)for(const auto& m:group.materials_)if(!publish(r,m,true,pass,f))return false;
    if(r.selection.second)for(const auto& m:group.materials_)if(!publish(r,m,false,pass,f))return false;
    return true;
}
std::optional<MaterialMaintenancePass> NativeMaterialMaintenance::step(MaterialStepDelta delta,MaterialMaintenanceFailure& f) noexcept {
    if(!access(f))return {};Guard guard(busy_);MaterialMaintenancePass pass;
    for(auto it=records_.begin();it!=records_.end();){auto& r=*it;++pass.visited;
        auto owner=r.owner.lock();auto generation=r.associated_generation.lock();
        const bool live=owner&&generation&&generation==owner->generation_;
        if(!live){if(r.route==MaterialTrackingRoute::direct_material){it=records_.erase(it);++pass.retired;continue;}
            ++pass.skipped;++it;continue;}
        if(!owner->access(f)){f.record=r.id;return {};}Guard owner_guard(owner->busy_);f.record=r.id;
        if(any(r.selection)){
            // The source null-target collection callback reloads record+10,
            // not its visited object. No safe target substitution is proved.
            if(r.route==MaterialTrackingRoute::owner_collection&&!owner->groups_.empty()){
                fail(f,MaterialMaintenanceError::unbound_collection,"material selected null-target collection publication unbound");return {};}
            if(r.route==MaterialTrackingRoute::direct_material){auto material=r.direct.lock();
                if(!material){fail(f,MaterialMaintenanceError::expired_target,"material direct target expired");return {};}
                const auto updated=translate_material_layers({*r.first,*r.second},r.rates,delta,r.selection);
                *r.first=updated.first;*r.second=updated.second;++pass.advances;
                if(r.selection.first&&!publish(r,material,true,pass,f))return {};
                if(r.selection.second&&!publish(r,material,false,pass,f))return {};
            }else if(r.route==MaterialTrackingRoute::prepared_group){auto group=r.group.lock();
                if(!group){fail(f,MaterialMaintenanceError::expired_target,"material explicit group target expired");return {};}
                if(!advance_group(r,*group,delta,pass,f))return {};
            } // Empty or disabled collection makes no callback in the source.
        }
        ++it;
    }
    f={};return pass;
}

std::unique_ptr<NativeMaterialMaintenanceTask> NativeMaterialMaintenanceTask::create(
    std::shared_ptr<NativeMaterialMaintenance> registry,std::shared_ptr<NativeMaterialGameStep> step,MaterialMaintenanceFailure& f) noexcept {
    f={};if(!registry||!step){fail(f,MaterialMaintenanceError::invalid_input,"material task requires registry and owned game step");return {};}
    if(!step->snapshot(f)||!registry->order(f))return {};
    try{return std::unique_ptr<NativeMaterialMaintenanceTask>(new NativeMaterialMaintenanceTask(std::move(registry),std::move(step)));}
    catch(const std::bad_alloc&){fail(f,MaterialMaintenanceError::allocation,"material maintenance task allocation");return {};}
}
TaskUpdate NativeMaterialMaintenanceTask::update(NativeFrontendExecution& execution,FrontendSchedulerFailure& f) {
    f={};auto pending=execution.transition_pending(f);
    if(!pending||*pending){if(f.kind==FrontendSchedulerError::none){f.kind=FrontendSchedulerError::invalid_state;f.operation="material maintenance outside ordinary native pass";}return TaskUpdate::error;}
    MaterialMaintenanceFailure why;auto delta=step_->snapshot(why);
    if(delta&&registry_->step(delta->material,why))return TaskUpdate::yield;
    f.kind=FrontendSchedulerError::invalid_result;f.operation=why.operation;f.task_code=static_cast<std::int64_t>(why.kind);return TaskUpdate::error;
}
} // namespace mka::game
