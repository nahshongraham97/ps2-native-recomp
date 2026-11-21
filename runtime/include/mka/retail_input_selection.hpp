#pragma once

#include "mka/retail_game_session.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_participants.hpp"

#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailInputSelectionError {
    none, invalid_owner, wrong_thread, invalid_physical_role, game_session,
    game_controls, input_role, participant, assignment, allocation,
    diagnostic_after_effect, native_exception
};
struct RetailInputSelectionFailure {
    RetailInputSelectionError kind{RetailInputSelectionError::none};
    const char* operation{""};
    RetailGameSessionFailure session;
    RetailGameControlsFailure controls;
    RetailInputRoleFailure role;
    RetailParticipantFailure participant;
    RetailInputAssignmentFailure assignment;
};

// Physical positions 0 and 1 of the source 281930 selector. Null inside an
// engaged optional is the source's ordinary no-candidate result; disengaged
// optional means a named native ownership/diagnostic failure. No host player
// route, task existence, 280C50 admission, or role-2/3 selector is inferred.
class NativeRetailInputSelection final {
public:
    static std::unique_ptr<NativeRetailInputSelection> create(
        std::shared_ptr<NativeRetailGameSession>,
        std::shared_ptr<NativeRetailInputRoles>,
        std::shared_ptr<NativeRetailParticipants>,
        RetailInputSelectionFailure&) noexcept;
    NativeRetailInputSelection(const NativeRetailInputSelection&) = delete;
    NativeRetailInputSelection& operator=(const NativeRetailInputSelection&) = delete;

    std::optional<std::shared_ptr<NativeRetailInputParticipant>> select_physical(
        RetailInputRole, RetailInputSelectionFailure&) noexcept;

private:
    NativeRetailInputSelection(std::shared_ptr<NativeRetailGameSession>,
        std::shared_ptr<NativeRetailGameControls>,
        std::shared_ptr<NativeRetailInputRoles>,
        std::shared_ptr<NativeRetailParticipants>,
        std::shared_ptr<NativeRetailInputAssignments>) noexcept;
    bool access(RetailInputSelectionFailure&) const noexcept;
    bool emit_assignment_diagnostic(unsigned, RetailInputSelectionFailure&) noexcept;

    std::shared_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailGameControls> controls_;
    std::shared_ptr<NativeRetailInputRoles> roles_;
    std::shared_ptr<NativeRetailParticipants> participants_;
    std::shared_ptr<NativeRetailInputAssignments> assignments_;
    bool diagnostic_failed_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
