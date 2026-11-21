#include "mka/retail_input_selection.hpp"

#include <cstdio>
#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailInputSelectionFailure& f, RetailInputSelectionError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
}

NativeRetailInputSelection::NativeRetailInputSelection(
    std::shared_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailGameControls> controls,
    std::shared_ptr<NativeRetailInputRoles> roles,
    std::shared_ptr<NativeRetailParticipants> participants,
    std::shared_ptr<NativeRetailInputAssignments> assignments) noexcept
    : session_(std::move(session)), controls_(std::move(controls)),
      roles_(std::move(roles)), participants_(std::move(participants)),
      assignments_(std::move(assignments)) {}

std::unique_ptr<NativeRetailInputSelection> NativeRetailInputSelection::create(
    std::shared_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailInputRoles> roles,
    std::shared_ptr<NativeRetailParticipants> participants,
    RetailInputSelectionFailure& f) noexcept {
    f={};
    if(!session||!roles||!participants) {
        fail(f,RetailInputSelectionError::invalid_owner,"retail selection owners");
        return {};
    }
    if(!session->validate_current_thread(f.session)) {
        fail(f,RetailInputSelectionError::game_session,"retail selection session thread");
        return {};
    }
    const auto controls=session->controls(f.session);
    if(!controls) {
        fail(f,RetailInputSelectionError::game_session,"retail selection controls owner");
        return {};
    }
    if(!controls->selection_flags(f.controls)) {
        fail(f,RetailInputSelectionError::game_controls,"retail selection control thread");
        return {};
    }
    RetailInputRoleFailure role_failure;
    if(!roles->observe(RetailInputRole::role0,role_failure)) {
        f.role=role_failure;
        fail(f,RetailInputSelectionError::input_role,"retail selection role owner");
        return {};
    }
    RetailParticipantFailure participant_failure;
    if(!participants->first_two(0,participant_failure)) {
        f.participant=participant_failure;
        fail(f,RetailInputSelectionError::participant,"retail selection participant owner");
        return {};
    }
    try {
        auto result=std::unique_ptr<NativeRetailInputSelection>(
            new NativeRetailInputSelection(std::move(session),std::move(controls),
                std::move(roles),std::move(participants),{}));
        // The adapter factory publishes mutual role/participant domain links.
        // Allocate the shell first, so no fallible work follows publication.
        auto assignments=NativeRetailInputAssignments::create(
            result->roles_,result->participants_,f.assignment);
        if(!assignments) {
            fail(f,RetailInputSelectionError::assignment,
                 "retail selection assignment domain");
            return {};
        }
        result->assignments_=std::move(assignments);
        return result;
    } catch(const std::bad_alloc&) {
        fail(f,RetailInputSelectionError::allocation,"retail selection allocation");
    } catch(...) {
        fail(f,RetailInputSelectionError::native_exception,"retail selection creation");
    }
    return {};
}

bool NativeRetailInputSelection::access(RetailInputSelectionFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_)
        return fail(f,RetailInputSelectionError::wrong_thread,"retail selection thread");
    if(diagnostic_failed_)
        return fail(f,RetailInputSelectionError::diagnostic_after_effect,
                    "retail selection diagnostic may have been partially written");
    if(!session_->validate_current_thread(f.session))
        return fail(f,RetailInputSelectionError::game_session,"retail selection session thread");
    return true;
}

bool NativeRetailInputSelection::emit_assignment_diagnostic(
    unsigned first_or_second, RetailInputSelectionFailure& f) noexcept {
    // Original 281930 calls 1DCC58 with these literal bytes only after
    // choosing an unassigned participant in the source flag-bit branch.
    const char* literal=first_or_second==0?
        "assigning plyr_info to P1\n":"assigning plyr_info to P2\n";
    if(std::fputs(literal,stderr)>=0) return true;
    // A native stream may have written a prefix. Refuse retry rather than
    // replaying an observable source diagnostic after a partial effect.
    diagnostic_failed_=true;
    return fail(f,RetailInputSelectionError::diagnostic_after_effect,
                "retail selection diagnostic output");
}

std::optional<std::shared_ptr<NativeRetailInputParticipant>>
NativeRetailInputSelection::select_physical(
    RetailInputRole role_id, RetailInputSelectionFailure& f) noexcept {
    if(!access(f)) return {};
    if(role_id!=RetailInputRole::role0&&role_id!=RetailInputRole::role1) {
        fail(f,RetailInputSelectionError::invalid_physical_role,
             "retail 281930 physical role zero or one");
        return {};
    }
    const auto mode=session_->scalar_mode(f.session);
    if(!mode) {
        fail(f,RetailInputSelectionError::game_session,"retail 185cb0 scalar mode");
        return {};
    }
    auto first=participants_->first_two(0,f.participant);
    auto second=participants_->first_two(1,f.participant);
    if(!first||!second) {
        fail(f,RetailInputSelectionError::participant,"retail first two participants");
        return {};
    }
    const auto role=static_cast<unsigned>(role_id);
    if(*mode==7||*mode==15) return role==0?first:second;
    if(*mode==8) return first;
    // Source mode 4 special case requires role 2 and is not reachable from
    // this physical-role 0/1 entry. Its separate GP word is never read here.
    const auto flags=controls_->selection_flags(f.controls);
    if(!flags) {
        fail(f,RetailInputSelectionError::game_controls,"retail 660084 selection flags");
        return {};
    }
    if((*flags&1u)==0) {
        const auto count=assignments_->association_count(f.assignment);
        if(!count) {
            fail(f,RetailInputSelectionError::assignment,"retail association count");
            return {};
        }
        if(*count!=0) {
            for(unsigned index=0;index!=3;++index) {
                const auto linked=roles_->observe(static_cast<RetailInputRole>(index),f.role);
                if(!linked) {
                    fail(f,RetailInputSelectionError::input_role,"retail linked role scan");
                    return {};
                }
                if(linked->participant)
                    return linked->participant==first?second:first;
            }
        }
        return role==0?first:second;
    }
    const unsigned selected=(*flags&2u)!=0?0u:1u;
    auto candidate=selected==0?first:second;
    const auto state=candidate->observe(f.participant);
    if(!state) {
        fail(f,RetailInputSelectionError::participant,"retail selection participant state");
        return {};
    }
    if(state->assigned_role!=-1) return std::shared_ptr<NativeRetailInputParticipant>{};
    if(!emit_assignment_diagnostic(selected,f)) return {};
    return candidate;
}

} // namespace mka::retail
