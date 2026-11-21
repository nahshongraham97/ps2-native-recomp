#include "mka/retail_profile_main_state.hpp"
#include "mka/retail_active_overlay.hpp"

#include <array>
#include <limits>
#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailProfileMainFailure& f,RetailProfileMainError kind,
          const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
constexpr std::array<const char*,3> companion_names{
    "NPCSTACK fixed block heap",
    "Player Col fixed block heap",
    "Player Attack Col fixed block heap"
};
constexpr std::array<memory::AllocationClassRole,3> companion_roles{
    memory::AllocationClassRole::npc_stack,
    memory::AllocationClassRole::player_collision,
    memory::AllocationClassRole::player_attack_collision
};
constexpr std::array<std::size_t,3> companion_payloads{0x4000,0x18f8,0x6790};
}

NativeRetailProfileMainState::NativeRetailProfileMainState(
    NativeRetailMemoryOwners& group,
    std::unique_ptr<NativeRetailProfileRuntime> wave) noexcept
    : group_(&group),wave_(std::move(wave)) {}

std::unique_ptr<NativeRetailProfileMainState> NativeRetailProfileMainState::create(
    NativeRetailMemoryOwners& group,RetailProfileMainFailure& f) noexcept {
    f={};
    auto wave=NativeRetailProfileRuntime::create(group,f.wave);
    if(!wave) {
        fail(f,f.wave.kind==RetailProfileRuntimeError::wrong_thread ?
            RetailProfileMainError::wrong_thread : RetailProfileMainError::missing_wave,
            "182670 actual Wave group prerequisite");
        return {};
    }
    try {
        return std::unique_ptr<NativeRetailProfileMainState>(
            new NativeRetailProfileMainState(group,std::move(wave)));
    } catch(const std::bad_alloc&) {
        fail(f,RetailProfileMainError::allocation,"profile main generation owner allocation");
    } catch(...) {
        fail(f,RetailProfileMainError::native_exception,"profile main generation owner construction");
    }
    return {};
}

bool NativeRetailProfileMainState::access(
    NativeRetailMemoryOwners& group,RetailProfileMainFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailProfileMainError::wrong_thread,"profile main owner thread");
    if(&group!=group_)
        return fail(f,RetailProfileMainError::foreign_group,"profile main memory group identity");
    RetailProfileRuntimeFailure wf;
    if(!wave_->observe(wf)) {
        f.wave=wf;
        return fail(f,RetailProfileMainError::missing_wave,"profile main Wave group identity");
    }
    return true;
}

bool NativeRetailProfileMainState::retire_companions(
    RetailProfileMainFailure& f) noexcept {
    // The source 310D30 visits AB90, AB94, AB8C before any Wave slot.
    constexpr std::array<std::size_t,3> retirement_order{0,2,1};
    try {
        for(const auto index:retirement_order) {
            if(!companions_[index].child) continue;
            const auto state=companions_[index].child->observe();
            if(!state.active || state.live_allocations || state.inflight_allocations ||
               state.child_owners || state.active_requests)
                return fail(f,RetailProfileMainError::companion_busy,
                            "310D30 companion preflight live payload/request/child");
            if(!overlay_parent_ || !state.parent ||
               *state.parent!=overlay_identity_)
                return fail(f,RetailProfileMainError::foreign_overlay,
                            "310D30 companion parent identity");
        }
        unsigned completed=0;
        for(const auto index:retirement_order) {
            auto& companion=companions_[index];
            if(!companion.child) continue;
            if(!companion.child->shutdown(f.heap)) {
                if(completed || f.heap.kind!=memory::NativeHeapError::owner_busy) {
                    phase_=RetailProfileMainPhase::partial_irreversible;
                    return fail(f,RetailProfileMainError::companion_partial,
                                "310D30 companion retirement after effect");
                }
                return fail(f,RetailProfileMainError::companion_busy,
                            "310D30 companion retirement before effect");
            }
            companion.leaf.reset();
            companion.child.reset(); // Original slot cleared after its retire call.
            ++completed;
        }
        overlay_parent_.reset();
        overlay_identity_={};
        return true;
    } catch(...) {
        phase_=RetailProfileMainPhase::partial_irreversible;
        return fail(f,RetailProfileMainError::native_exception,
                    "310D30 companion retirement uncertain after effect");
    }
}

bool NativeRetailProfileMainState::apply_182670(
    const NativeClassProfilePlan& plan,NativeRetailMemoryOwners& group,
    RetailProfileMainFailure& f) noexcept {
    if(!access(group,f)) return false;
    if(phase_==RetailProfileMainPhase::partial_irreversible)
        return fail(f,RetailProfileMainError::partial_irreversible,
                    "profile prior irreversible retirement");
    if(!NativeRetailProfileOwners::valid_source_plan(plan))
        return fail(f,RetailProfileMainError::invalid_plan,"182670 selected source plan");
    if(generation_==std::numeric_limits<std::uint64_t>::max())
        return fail(f,RetailProfileMainError::generation_exhausted,
                    "profile native generation counter");
    if(phase_==RetailProfileMainPhase::numeric_published && numeric_ &&
       numeric_->requested_selector!=plan.requested_selector)
        return fail(f,RetailProfileMainError::partial_irreversible,
                    "profile prior numeric publication pending");

    // 182670 writes its selected numeric record before 310D30. This is the
    // only production numeric authority; the Wave helper's private copy is
    // never published to Main or to companion callbacks.
    numeric_=plan;
    phase_=RetailProfileMainPhase::numeric_published;
    if(!retire_companions(f)) return false;
    if(!wave_->apply_numeric_and_wave_classes(plan,group,f.wave)) {
        if(f.wave.kind==RetailProfileRuntimeError::partial_irreversible)
            phase_=RetailProfileMainPhase::partial_irreversible;
        return fail(f,phase_==RetailProfileMainPhase::partial_irreversible ?
            RetailProfileMainError::partial_irreversible : RetailProfileMainError::wave_runtime,
            "182670 Wave class retirement or construction");
    }
    ++generation_;
    phase_=RetailProfileMainPhase::ready;
    f={};
    return true;
}

bool NativeRetailProfileMainState::install_companions_311260(
    const NativeRetailActiveOverlay& overlay,NativeRetailMemoryOwners& group,
    RetailProfileMainFailure& f) noexcept {
    if(!access(group,f)) return false;
    if(phase_!=RetailProfileMainPhase::ready || !numeric_)
        return fail(f,RetailProfileMainError::partial_irreversible,
                    "311260 requires completed profile numerics");
    for(const auto& entry:companions_)
        if(entry.child)
            return fail(f,RetailProfileMainError::companions_already_present,
                        "311260 companion slot already populated");
    RetailActiveOverlayFailure of;
    auto parent=overlay.retained_companion_parent(of);
    if(!parent) {
        f.overlay=of;
        return fail(f,of.kind==RetailActiveOverlayError::wrong_thread ?
                    RetailProfileMainError::wrong_thread :
                    RetailProfileMainError::missing_overlay,
                    "311260 actual 3DDA70 overlay parent unavailable");
    }
    try {
        const auto parent_state=parent->observe();
        if(!parent_state.active)
            return fail(f,RetailProfileMainError::missing_overlay,
                        "311260 overlay parent inactive");
        auto admission=parent->retain_request(f.heap);
        if(!admission)
            return fail(f,RetailProfileMainError::missing_overlay,
                        "311260 overlay parent admission");
        std::array<Companion,3> new_entries;
        const std::array<std::uint64_t,3> counts{
            numeric_->object_counts.npc_stacks,
            numeric_->object_counts.player_collision_objects,
            numeric_->object_counts.attack_collision_objects};
        for(std::size_t i=0;i<new_entries.size();++i) {
            auto child=memory::NativeHeapOwner::create(companion_names[i],parent,f.heap);
            if(!child)
                return fail(f,RetailProfileMainError::allocation,
                            "311260 companion heap construction");
            auto leaf=memory::NativeAllocationClass::create(
                {child,companion_roles[i],memory::AllocationClassSchema::source_kind2_payload,
                 companion_payloads[i],16},f.class_leaf);
            if(!leaf)
                return fail(f,RetailProfileMainError::allocation,
                            "311260 companion class construction");
            std::shared_ptr<memory::NativeAllocationClass> shared_leaf=std::move(leaf);
            new_entries[i]={std::move(child),std::move(shared_leaf),counts[i]};
        }
        companions_=std::move(new_entries);
        overlay_identity_=parent_state.identity;
        overlay_parent_=std::move(parent);
        f={};
        return true;
    } catch(const std::bad_alloc&) {
        return fail(f,RetailProfileMainError::allocation,
                    "311260 companion native allocation");
    } catch(...) {
        return fail(f,RetailProfileMainError::native_exception,
                    "311260 companion construction exception");
    }
}

std::optional<RetailProfileMainObservation> NativeRetailProfileMainState::observe(
    RetailProfileMainFailure& f) const noexcept {
    if(!access(*group_,f)) return {};
    auto wave_state=wave_->observe(f.wave);
    if(!wave_state) {
        fail(f,RetailProfileMainError::wave_runtime,"profile Wave observation");
        return {};
    }
    std::array<std::optional<bool>,3> slots{};
    std::array<std::optional<std::uint64_t>,3> counts{};
    if(phase_!=RetailProfileMainPhase::partial_irreversible)
        for(std::size_t i=0;i<slots.size();++i) {
            slots[i]=bool(companions_[i].child);
            if(companions_[i].child)
                counts[i]=companions_[i].configured_game_count;
        }
    return RetailProfileMainObservation{
        phase_,numeric_,generation_,slots,counts,wave_state->wave_classes};
}
} // namespace mka::retail
