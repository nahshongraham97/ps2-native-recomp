#include "mka/retail_input_assignments.hpp"

#include <new>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailInputAssignmentFailure& failure, RetailInputAssignmentError kind,
    const char* operation) noexcept {
    failure = {kind, operation};
    return false;
}
} // namespace

NativeRetailInputAssignments::NativeRetailInputAssignments(
    std::shared_ptr<NativeRetailInputRoles> roles,
    std::shared_ptr<NativeRetailParticipants> participants) noexcept
    : roles_(std::move(roles)), participants_(std::move(participants)) {}

std::shared_ptr<NativeRetailInputAssignments> NativeRetailInputAssignments::create(
    std::shared_ptr<NativeRetailInputRoles> roles,
    std::shared_ptr<NativeRetailParticipants> participants,
    RetailInputAssignmentFailure& failure) noexcept {
    failure = {};
    if (!roles || !participants) {
        fail(failure, RetailInputAssignmentError::invalid_owner,
            "retail assignment source owners");
        return {};
    }
    const auto thread = std::this_thread::get_id();
    if (thread != roles->thread_ || thread != participants->owner_thread_) {
        fail(failure, RetailInputAssignmentError::wrong_thread,
            "retail assignment source owner thread");
        return {};
    }
    // Both source owners belong to one game context. The weak control-block
    // identity remains comparable even if a former owner has expired.
    if ((roles->assignment_domain_bound_ &&
         (roles->assignment_domain_.owner_before(participants) ||
          participants.owner_before(roles->assignment_domain_))) ||
        (participants->assignment_domain_bound_ &&
         (participants->assignment_domain_.owner_before(roles) ||
          roles.owner_before(participants->assignment_domain_)))) {
        fail(failure, RetailInputAssignmentError::foreign_domain,
            "retail assignment game domain");
        return {};
    }
    try {
        auto result = std::shared_ptr<NativeRetailInputAssignments>(
            new NativeRetailInputAssignments(std::move(roles), std::move(participants)));
        result->roles_->assignment_domain_ = result->participants_;
        result->roles_->assignment_domain_bound_ = true;
        result->participants_->assignment_domain_ = result->roles_;
        result->participants_->assignment_domain_bound_ = true;
        return result;
    } catch (const std::bad_alloc&) {
        fail(failure, RetailInputAssignmentError::allocation, "retail assignment allocation");
    } catch (...) {
        fail(failure, RetailInputAssignmentError::native_exception,
            "retail assignment creation");
    }
    return {};
}

bool NativeRetailInputAssignments::access(RetailInputAssignmentFailure& failure) const noexcept {
    failure = {};
    return std::this_thread::get_id() == owner_thread_ ||
        fail(failure, RetailInputAssignmentError::wrong_thread,
            "retail assignment thread");
}

bool NativeRetailInputAssignments::checked_participant(
    const std::shared_ptr<NativeRetailInputParticipant>& participant,
    RetailInputAssignmentFailure& failure) const noexcept {
    if (!participant ||
        (participant != participants_->first_two_[0] &&
         participant != participants_->first_two_[1]))
        return fail(failure, RetailInputAssignmentError::foreign_participant,
            "retail assignment participant ownership");
    return true;
}

bool NativeRetailInputAssignments::checked_role(
    std::int32_t role, RetailInputAssignmentFailure& failure) const noexcept {
    if (role < 0 || static_cast<std::size_t>(role) >= roles_->roles_.size())
        return fail(failure, RetailInputAssignmentError::invalid_role,
            "retail assignment role identity");
    return true;
}

void NativeRetailInputAssignments::clear_three_role_edges() noexcept {
    // 163C40 touches roles 0..2, irrespective of the detached role.
    for (std::size_t index = 0; index != 3; ++index) {
        auto& state = roles_->roles_[index];
        state.previous = 0;
        state.current = 0;
        state.rising = 0;
    }
}

void NativeRetailInputAssignments::clear_old_role(std::uint32_t index) noexcept {
    auto& role = roles_->roles_[index];
    role.participant.reset();
    role.source_control_word = 0;
    clear_three_role_edges();
}

std::optional<std::int32_t> NativeRetailInputAssignments::association_count(
    RetailInputAssignmentFailure& failure) const noexcept {
    if (!access(failure)) return {};
    return roles_->association_count_;
}

bool NativeRetailInputAssignments::bind_validated(
    const std::shared_ptr<NativeRetailInputParticipant>& participant,
    std::uint32_t new_role, RetailInputAssignmentFailure& failure) noexcept {
    auto& source = participant->state_;
    const auto old_role = source.assigned_role;
    // No native fighter producer is connected. A source +0x30 owner would
    // require its real +0x390 role write before this path can execute.
    if (source.has_fighter)
        return fail(failure, RetailInputAssignmentError::unbound_fighter,
            "retail assignment fighter role effect");
    if (old_role > 0 && old_role != static_cast<std::int32_t>(new_role)) {
        // 281060 uses BLEZ, so role zero deliberately does not enter here.
        clear_old_role(static_cast<std::uint32_t>(old_role));
        if (old_role == 2) roles_->roles_[2].initialized = false;
        source.assigned_role = -1;
        if (roles_->association_count_ > 0) --roles_->association_count_;
    }
    roles_->roles_[new_role].participant = participant;
    source.assigned_role = static_cast<std::int32_t>(new_role);
    source.ordinal = static_cast<std::int32_t>(participant->identity_);
    return true;
}

bool NativeRetailInputAssignments::bind_direct(
    const std::shared_ptr<NativeRetailInputParticipant>& participant,
    RetailInputRole role, RetailInputAssignmentFailure& failure) noexcept {
    if (!access(failure) || !checked_participant(participant, failure)) return false;
    const auto index = static_cast<std::int32_t>(role);
    if (!checked_role(index, failure)) return false;
    const auto old_role = participant->state_.assigned_role;
    if (old_role >= 0 && !checked_role(old_role, failure)) return false;
    return bind_validated(participant, static_cast<std::uint32_t>(index), failure);
}

bool NativeRetailInputAssignments::detach(
    const std::shared_ptr<NativeRetailInputParticipant>& participant,
    RetailInputAssignmentFailure& failure) noexcept {
    if (!access(failure)) return false;
    if (!participant) return true; // 280F90(NULL) returns without effects.
    if (!checked_participant(participant, failure)) return false;
    auto& source = participant->state_;
    const auto old_role = source.assigned_role;
    if (old_role >= 0) {
        if (!checked_role(old_role, failure)) return false;
        clear_old_role(static_cast<std::uint32_t>(old_role));
    }
    if (old_role == 2) roles_->roles_[2].initialized = false;
    source.assigned_role = -1;
    if (roles_->association_count_ > 0) --roles_->association_count_;
    return true;
}

bool NativeRetailInputAssignments::bind_other_to_role_two(
    const std::shared_ptr<NativeRetailInputParticipant>& participant,
    RetailInputAssignmentFailure& failure) noexcept {
    if (!access(failure) || !checked_participant(participant, failure)) return false;
    if (participant->state_.assigned_role != -1) return true; // original 2818B0 guard
    if (!bind_validated(participant, 2, failure)) return false;
    if (roles_->association_count_ < 2) ++roles_->association_count_;
    roles_->roles_[2].initialized = true;
    return true;
}

bool NativeRetailInputAssignments::increment_count_before_280c50_bind(
    RetailInputAssignmentFailure& failure) noexcept {
    if (!access(failure)) return false;
    if (roles_->association_count_ < 2) ++roles_->association_count_;
    return true;
}

bool NativeRetailInputAssignments::reset_count_from_18d970(
    RetailInputAssignmentFailure& failure) noexcept {
    if (!access(failure)) return false;
    roles_->association_count_ = 0;
    return true;
}

} // namespace mka::retail
