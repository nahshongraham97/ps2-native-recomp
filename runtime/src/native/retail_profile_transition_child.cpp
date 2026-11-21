#include "mka/retail_profile_transition_child.hpp"
#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <array>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
// First LE32 column of the original64x16-byte table at5A2BC0. Work72
// independently proves its extent via189C50; row30 and39 both contain62.
constexpr std::array<std::uint32_t,64> profile_entry_keys{
    48,59,46,44,12,5,41,56,6,49,58,22,8,20,9,45,
    14,2,33,17,39,36,4,24,21,23,25,54,18,29,62,61,
    32,30,50,42,53,43,7,62,57,28,35,19,0,31,60,13,
    55,40,27,47,52,1,38,16,11,37,34,51,15,26,10,3};

bool fail(RetailProfileMenuFailure& f,RetailProfileMenuError kind,
    const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
bool child_failed(RetailProfileMenuFailure& f,const char* operation) noexcept {
    if(f.kind==RetailProfileMenuError::none)
        fail(f,RetailProfileMenuError::required_child,operation);
    return false;
}
} // namespace

std::optional<std::uint32_t> retail_profile_entry_key_from_5a2bc0(
    std::uint32_t index,RetailProfileMenuFailure& f) noexcept {
    f={};
    if(index>=profile_entry_keys.size()) {
        fail(f,RetailProfileMenuError::profile_entry_index,
            "18B0D0 unproved index outside original profile entry table");
        return std::nullopt;
    }
    return profile_entry_keys[index];
}

bool transition_retail_profile_from_18b0d0(
    NativeRetailMainState& main,std::uint32_t role,std::uint32_t argument,
    RetailProfileMenuFailure& f) {
    f={};
    if(!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f,RetailProfileMenuError::main_lifetime,"18B0D0 Main lifetime/thread");
    if(role>1)
        return fail(f,RetailProfileMenuError::invalid_role,"18B0D0 unbound participant outside proved0/1 callers");
    const auto session=main.startup.game_session;
    if(!session || !session->validate_current_thread(f.session))
        return fail(f,RetailProfileMenuError::session,"18B0D0 actual retained Session");
    if(!main.participants)
        return fail(f,RetailProfileMenuError::missing_participants,"18B0D0 source participant owner");
    const auto first=main.participants->first_two(0,f.participant);
    const auto first_value=first?first->observe(f.participant):std::nullopt;
    if(!first_value)
        return fail(f,RetailProfileMenuError::participant,"18B0D0 live first participant ordinal");
    //18B0F4/F8 select by the live first +4 word BEFORE the scene getter.
    // Fresh source ordinal5 deliberately selects the second record for role0.
    const auto selected=main.participants->first_two(
        role==static_cast<std::uint32_t>(first_value->ordinal)?0u:1u,f.participant);
    if(!selected)
        return fail(f,RetailProfileMenuError::participant,"18B0D0 selected source base");
    auto* reference=session->live_scene_reference(f.session);
    const auto scene=reference?reference->lookup_from_2f18f0(f.scene_reference):std::nullopt;
    if(!reference || !scene)
        return fail(f,RetailProfileMenuError::scene_reference,"18B0D0 actual scene/cookie lookup");
    if(!*scene) return true; // Original void early return, not a native failure.

    auto selected_state=[&]() -> std::optional<std::uint32_t> {
        const auto value=selected->observe(f.participant);
        if(!value) {
            fail(f,RetailProfileMenuError::participant,"18B0D0 selected live state reread");
            return std::nullopt;
        }
        return value->game_state;
    };
    auto profile_scene=[&]() -> std::shared_ptr<NativeRetailProfileSceneObject> {
        const auto value=std::dynamic_pointer_cast<NativeRetailProfileSceneObject>(*scene);
        if(!value || !value->scene_identity_live() || value->owner_session()!=session.get()) {
            f.scene_object={RetailProfileSceneObjectError::incompatible_object,
                "18B0D0 reached menu scene field capability"};
            fail(f,RetailProfileMenuError::scene_object,"18B0D0 actual typed menu scene");
            return {};
        }
        return value;
    };
    auto store_argument=[&]() -> bool {
        const auto value=profile_scene();
        if(!value) return false;
        if(!value->store_lane_word_from_18b0d0(role,argument,f.scene_object))
            return fail(f,RetailProfileMenuError::scene_object,"18B0D0 ordered scene argument store");
        return true;
    };

    try {
        const auto controls=session->controls(f.session);
        const auto flags=controls?controls->selection_flags(f.session.controls):std::nullopt;
        if(!flags)
            return fail(f,RetailProfileMenuError::controls,"18B0D0 live selection bit0");
        if((*flags&1u)!=0) {
            auto* domain=session->task_domain(f.session);
            const auto live=domain?domain->observe(f.session.task_domain):std::nullopt;
            if(!live)
                return fail(f,RetailProfileMenuError::task_domain,"18B0D0 live source partner word");
            if(role==live->live_partner_word) {
                const auto state=selected_state();
                if(!state) return false;
                if(*state==0) return true;
                if(!selected->set_game_state(1,f.participant))
                    return fail(f,RetailProfileMenuError::participant,"18B0D0 gate1319E0 state store");
                if(!apply_retail_profile_participant_from_188720(main,role,f))
                    return child_failed(f,"18B0D0 gate188720 required child");
                return true; // No scene field access/store in this source arm.
            }
        }
        const auto role_record=main.participants->first_two(role,f.participant);
        const auto role_value=role_record?role_record->observe(f.participant):std::nullopt;
        if(!role_value)
            return fail(f,RetailProfileMenuError::participant,"18B0D0 role-indexed live state");
        if(role_value->game_state==1) {
            const auto value=profile_scene();
            if(!value) return false;
            const auto fields=value->observe(f.scene_object);
            if(!fields)
                return fail(f,RetailProfileMenuError::scene_object,"18B0D0 live scene table index");
            const auto key=retail_profile_entry_key_from_5a2bc0(fields->values.lanes[role].word_10,f);
            if(!key) return false;
            const auto decision=decide_retail_profile_entry_from_1892d0(main,*key,0,role,f);
            if(!decision) return child_failed(f,"18B0D0 required1892D0 decision");
            if(*decision!=0) {
                if(!publish_retail_profile_participant_from_189690(main,role_record,f))
                    return child_failed(f,"18B0D0 required189690 participant publication");
                if(!store_argument()) return false; // Source18B210 after publication.
            }
            if(!apply_retail_profile_participant_from_188720(main,role,f))
                return child_failed(f,"18B0D0 first188720 required child");
        }
        // This is selected base+8, which can differ from the role-indexed
        // record above. Source reload18B220 follows the effectful child.
        const auto state=selected_state();
        if(!state) return false;
        if(*state==2) {
            if(!selected->set_game_state(1,f.participant))
                return fail(f,RetailProfileMenuError::participant,"18B0D0 final1319E0 state store");
            if(!store_argument()) return false; //18B244 JAL delay slot, before188720.
            if(!apply_retail_profile_participant_from_188720(main,role,f))
                return child_failed(f,"18B0D0 final188720 required child");
        }
        return true;
    } catch(const detail::NativeMainReentry&) {
        throw;
    } catch(const std::bad_alloc&) {
        return fail(f,RetailProfileMenuError::allocation,"18B0D0 child allocation after possible effects");
    } catch(const std::length_error&) {
        return fail(f,RetailProfileMenuError::allocation,"18B0D0 child capacity after possible effects");
    } catch(...) {
        return fail(f,RetailProfileMenuError::native_exception_after_effect,"18B0D0 child exception after possible effects");
    }
}
} // namespace mka::retail
