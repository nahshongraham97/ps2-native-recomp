#include "mka/retail_input_assignments.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
NativeRetailInputRoleObservation role_state(const std::shared_ptr<NativeRetailInputRoles>& roles,
    RetailInputRole role) {
    RetailInputRoleFailure failure;
    const auto observed = roles->observe(role, failure);
    check(bool(observed), "role observation");
    return *observed;
}
RetailParticipantObservation participant_state(
    const std::shared_ptr<NativeRetailInputParticipant>& participant) {
    RetailParticipantFailure failure;
    const auto observed = participant->observe(failure);
    check(bool(observed), "participant observation");
    return *observed;
}
} // namespace

void* operator new(std::size_t size) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

int main() {
    try {
        RetailInputRoleFailure role_failure;
        RetailParticipantFailure participant_failure;
        RetailInputAssignmentFailure failure;
        auto roles = NativeRetailInputRoles::create(role_failure);
        auto participants = NativeRetailParticipants::create_first_two(participant_failure);
        check(bool(roles) && bool(participants), "source role and participant owners");
        check(!NativeRetailInputAssignments::create({}, participants, failure) &&
            failure.kind == RetailInputAssignmentError::invalid_owner,
            "missing role owner is a named failure");
        fail_allocation = true;
        auto allocation_failure = NativeRetailInputAssignments::create(roles, participants, failure);
        fail_allocation = false;
        check(!allocation_failure && failure.kind == RetailInputAssignmentError::allocation,
            "owner allocation failure does not publish an association owner");
        auto assignments = NativeRetailInputAssignments::create(roles, participants, failure);
        check(bool(assignments), "assignment owner created from real source owners");
        auto second_adapter = NativeRetailInputAssignments::create(roles, participants, failure);
        check(bool(second_adapter) &&
            assignments->increment_count_before_280c50_bind(failure) &&
            second_adapter->association_count(failure) == 1 &&
            second_adapter->detach({}, failure) &&
            assignments->association_count(failure) == 1,
            "shared game count and 280F90 null no-op");
        assignments.reset();
        second_adapter.reset();
        assignments = NativeRetailInputAssignments::create(roles, participants, failure);
        check(bool(assignments) && assignments->association_count(failure) == 1 &&
            assignments->reset_count_from_18d970(failure) &&
            assignments->association_count(failure) == 0,
            "adapter retirement and recreation retain game count until source reset");
        auto other_roles = NativeRetailInputRoles::create(role_failure);
        auto other_participants = NativeRetailParticipants::create_first_two(participant_failure);
        check(bool(other_roles) && bool(other_participants) &&
            !NativeRetailInputAssignments::create(other_roles, participants, failure) &&
            failure.kind == RetailInputAssignmentError::foreign_domain &&
            !NativeRetailInputAssignments::create(roles, other_participants, failure) &&
            failure.kind == RetailInputAssignmentError::foreign_domain,
            "source role and participant owners cannot split across game domains");
        const auto first = participants->first_two(0, participant_failure);
        const auto second = participants->first_two(1, participant_failure);
        check(bool(first) && bool(second), "first two participant records");

        check(assignments->bind_direct(first, RetailInputRole::role0, failure),
            "281060 binds participant zero to role zero");
        check(participant_state(first).assigned_role == 0 &&
            participant_state(first).ordinal == 0 &&
            role_state(roles, RetailInputRole::role0).participant == first,
            "source inverse association and ordinal zero");
        check(assignments->bind_direct(first, RetailInputRole::role1, failure),
            "281060 binds again from old role zero");
        check(role_state(roles, RetailInputRole::role0).participant == first &&
            role_state(roles, RetailInputRole::role1).participant == first &&
            participant_state(first).assigned_role == 1,
            "281060 BLEZ deliberately skips old role-zero detach");

        check(assignments->increment_count_before_280c50_bind(failure),
            "source count increment before selected bind");
        check(assignments->detach(first, failure),
            "280F90 explicit detach handles old role one");
        check(!role_state(roles, RetailInputRole::role1).participant &&
            role_state(roles, RetailInputRole::role0).participant == first &&
            participant_state(first).assigned_role == -1 &&
            participant_state(first).ordinal == 0,
            "detach clears the current inverse, retains source role-zero stale link and ordinal");
        // The 163C40 edge-clear implementation is source-mapped, but a
        // nonzero edge/axis fixture requires the actual 2214D0 producer.
        check(assignments->association_count(failure) == 0,
            "280F90 decremented a positive game association count");
        check(assignments->increment_count_before_280c50_bind(failure) &&
            assignments->detach(first, failure) &&
            assignments->association_count(failure) == 0,
            "280F90 decrements count even when already unassigned");

        check(assignments->bind_direct(first, RetailInputRole::role0, failure) &&
            assignments->detach(first, failure) &&
            !role_state(roles, RetailInputRole::role0).participant,
            "280F90 explicitly detaches old role zero, unlike 281060");
        check(assignments->increment_count_before_280c50_bind(failure) &&
            assignments->bind_direct(second, RetailInputRole::role1, failure) &&
            assignments->bind_direct(second, RetailInputRole::role3, failure) &&
            !role_state(roles, RetailInputRole::role1).participant &&
            role_state(roles, RetailInputRole::role3).participant == second &&
            assignments->association_count(failure) == 0,
            "281060 old role one detaches and decrements positive count");
        check(assignments->detach(second, failure),
            "280F90 releases old role three before role-two admission");
        check(assignments->bind_other_to_role_two(second, failure) &&
            role_state(roles, RetailInputRole::role2).initialized &&
            role_state(roles, RetailInputRole::role2).participant == second &&
            participant_state(second).ordinal == 1 &&
            assignments->association_count(failure) == 1,
            "2818B0 binds the other source participant to role two");
        check(assignments->bind_direct(second, RetailInputRole::role3, failure),
            "281060 positive-old-role reassignment");
        check(!role_state(roles, RetailInputRole::role2).participant &&
            !role_state(roles, RetailInputRole::role2).initialized &&
            role_state(roles, RetailInputRole::role3).participant == second &&
            assignments->association_count(failure) == 0,
            "positive old role two retires and decrements count");

        auto foreign_collection = NativeRetailParticipants::create_first_two(participant_failure);
        auto foreign = foreign_collection->first_two(0, participant_failure);
        check(!assignments->bind_direct(foreign, RetailInputRole::role0, failure) &&
            failure.kind == RetailInputAssignmentError::foreign_participant,
            "foreign participant cannot enter the source role graph");
        check(!assignments->bind_direct(first, static_cast<RetailInputRole>(4), failure) &&
            failure.kind == RetailInputAssignmentError::invalid_role,
            "unknown game role cannot become host capacity");
        bool wrong_thread = false;
        std::thread worker([&] {
            RetailInputAssignmentFailure other;
            wrong_thread = !assignments->detach(first, other) &&
                other.kind == RetailInputAssignmentError::wrong_thread;
        });
        worker.join();
        check(wrong_thread, "assignment effects remain on source owner thread");
        roles.reset();
        participants.reset();
        check(assignments->association_count(failure) == 0 &&
            participant_state(first).assigned_role == -1,
            "assignment owner retains role and participant lifetimes");
        std::puts("PASS native source assignment protocol; no host route, game or endpoint");
        return 0;
    } catch (const std::exception& error) {
        fail_allocation = false;
        std::fprintf(stderr, "FAIL retail input assignments: %s\n", error.what());
        return 1;
    }
}
