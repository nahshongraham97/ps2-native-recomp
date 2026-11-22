#pragma once
#include "mka/retail_input_selection.hpp"
#include "mka/retail_task_domain.hpp"
namespace mka::retail {
struct NativeRetailMainState;
enum class RetailInputAdmissionError { none, owner, reentrant, allocation, native_exception, role, session, tasks, selection, participant, assignment, fighter };
struct RetailInputAdmissionFailure {
    RetailInputAdmissionError kind{RetailInputAdmissionError::none};
    const char* operation{""};
    RetailInputRoleFailure role;
    RetailGameSessionFailure session;
    RetailTaskDomainFailure tasks;
    RetailTaskRegistryFailure registry;
    RetailInputSelectionFailure selection;
    RetailParticipantFailure participant;
    RetailInputAssignmentFailure assignment;
};
// Source280C50 for physical roles0/1, the only arguments from2214D0.
// Fighter propagation still fails by its existing required assignment effect.
// Engaged false is ordinary game refusal, disengaged is a named native error.
// No host route, connected state, polling or disconnect work is inferred.
std::optional<bool> admit_retail_physical_role_from_280c50(
    NativeRetailMainState&,RetailInputRole,RetailInputAdmissionFailure&);
} // namespace mka::retail
