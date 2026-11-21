#pragma once

#include "mka/controller_input.hpp"
#include "mka/retail_game_controls.hpp"
#include "mka/retail_input_roles.hpp"

#include <cstdint>
#include <memory>
#include <thread>

namespace mka::retail {

enum class RetailAssignedPollError {
    none, invalid_owner, foreign_owner, wrong_thread, invalid_role,
    unassigned_role, missing_host_route, disconnected, missing_native_axes,
    role, controls, allocation, native_exception
};
struct RetailAssignedPollFailure {
    RetailAssignedPollError kind{RetailAssignedPollError::none};
    const char* operation{""};
    RetailInputRoleFailure role;
    RetailGameControlsFailure controls;
};

// Original 1632F0 reads a live unsigned byte threshold; its original ELF
// initial value is 48. The caller must supply the current game setting.
struct RetailAnalogDeadzone {
    std::uint8_t source_units;
};

// Connected 2214D0 assigned-ready branch and complete 163930 gate writer.
// It owns no host device, participant selector, or source status transport.
class NativeRetailAssignedPoll final {
public:
    static std::unique_ptr<NativeRetailAssignedPoll> create(
        std::shared_ptr<NativeRetailInputRoles>,
        std::shared_ptr<NativeRetailGameControls>,
        input::ControllerInput&,
        RetailAssignedPollFailure&) noexcept;
    NativeRetailAssignedPoll(const NativeRetailAssignedPoll&) = delete;
    NativeRetailAssignedPoll& operator=(const NativeRetailAssignedPoll&) = delete;

    // Source 163930: role 2,1,0 block bits from mask bits 0,1,2, then
    // global analog inhibit bit 6 from mask bit 3.
    bool store_gate_from_163930(NativeRetailInputRoles&,
        NativeRetailGameControls&, std::uint32_t source_mask,
        RetailAssignedPollFailure&) noexcept;

    // Source 2214D0 assigned branch for a ready connected sample. The host
    // player key is an explicit platform route, not a game participant ID.
    bool ready_assigned(NativeRetailInputRoles&, NativeRetailGameControls&,
        const input::ControllerInput&, RetailInputRole, std::uint32_t host_player,
        RetailAnalogDeadzone, RetailAssignedPollFailure&) noexcept;

private:
    NativeRetailAssignedPoll(std::shared_ptr<NativeRetailInputRoles>,
        std::shared_ptr<NativeRetailGameControls>,
        input::ControllerInput&) noexcept;
    bool access(NativeRetailInputRoles&, NativeRetailGameControls&,
        const input::ControllerInput&, RetailAssignedPollFailure&) const noexcept;
    bool access_gate(NativeRetailInputRoles&, NativeRetailGameControls&,
        RetailAssignedPollFailure&) const noexcept;
    static bool direction(std::int16_t, std::uint8_t) noexcept;

    std::shared_ptr<NativeRetailInputRoles> roles_;
    std::shared_ptr<NativeRetailGameControls> controls_;
    input::ControllerInput* samples_;
    const std::thread::id thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
