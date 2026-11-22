#pragma once

#include "mka/retail_input_roles.hpp"
#include "mka/retail_participants.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailInputAssignmentError {
    none, wrong_thread, invalid_owner, foreign_domain, foreign_participant, invalid_role,
    unbound_fighter, allocation, native_exception
};
struct RetailInputAssignmentFailure {
    RetailInputAssignmentError kind{RetailInputAssignmentError::none};
    const char* operation{""};
};

// Source 281060/280F90/163C40/2818B0 over real game role and participant
// owners. Count is game association state, independent of host input routing.
class NativeRetailInputAssignments final {
public:
    static std::shared_ptr<NativeRetailInputAssignments> create(
        std::shared_ptr<NativeRetailInputRoles>,
        std::shared_ptr<NativeRetailParticipants>,
        RetailInputAssignmentFailure&) noexcept;
    NativeRetailInputAssignments(const NativeRetailInputAssignments&) = delete;
    NativeRetailInputAssignments& operator=(const NativeRetailInputAssignments&) = delete;

    bool validate_domain(const NativeRetailInputRoles&, const NativeRetailParticipants&,
        RetailInputAssignmentFailure&) const noexcept;
    std::optional<std::int32_t> association_count(
        RetailInputAssignmentFailure&) const noexcept;
    // 281060: its internal detach condition is exactly old>0 && old!=new.
    bool bind_direct(const std::shared_ptr<NativeRetailInputParticipant>&,
        RetailInputRole, RetailInputAssignmentFailure&) noexcept;
    // 280F90: old role 0 is detached; positive count decrements even if the
    // participant was already unassigned. Axes remain as in 163C40.
    bool detach(const std::shared_ptr<NativeRetailInputParticipant>&,
        RetailInputAssignmentFailure&) noexcept;
    // 2818B0: only an unassigned participant binds role 2, then count is
    // incremented if below two and role 2 initialized bit 1 is set.
    bool bind_other_to_role_two(
        const std::shared_ptr<NativeRetailInputParticipant>&,
        RetailInputAssignmentFailure&) noexcept;
    // Source count stores from 280C50 admission and 18D970 game reset.
    bool increment_count_before_280c50_bind(RetailInputAssignmentFailure&) noexcept;
    bool reset_count_from_18d970(RetailInputAssignmentFailure&) noexcept;
    // Empty with failure.none is the source negative-role/null result.
    std::optional<NativeRetailRoleControlReference> role_control_from_281830(
        std::uint32_t participant_identity, RetailInputAssignmentFailure&) const noexcept;
    // Uses the live participant ordinal and the SAME retained role/mapping
    // state; does not detach, clear digital edges or decrement the count.
    bool reset_profile_controls_from_2817b0(
        const std::shared_ptr<NativeRetailInputParticipant>&,
        RetailInputAssignmentFailure&) noexcept;

private:
    NativeRetailInputAssignments(std::shared_ptr<NativeRetailInputRoles>,
        std::shared_ptr<NativeRetailParticipants>) noexcept;
    bool access(RetailInputAssignmentFailure&) const noexcept;
    bool checked_participant(const std::shared_ptr<NativeRetailInputParticipant>&,
        RetailInputAssignmentFailure&) const noexcept;
    bool checked_role(std::int32_t, RetailInputAssignmentFailure&) const noexcept;
    void clear_three_role_edges() noexcept;
    void clear_old_role(std::uint32_t) noexcept;
    bool bind_validated(const std::shared_ptr<NativeRetailInputParticipant>&,
        std::uint32_t, RetailInputAssignmentFailure&) noexcept;

    std::shared_ptr<NativeRetailInputRoles> roles_;
    std::shared_ptr<NativeRetailParticipants> participants_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
