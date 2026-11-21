#pragma once
#include "mka/retail_main.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {
class NativeRetailInputParticipant;

enum class RetailInputRole : std::uint8_t { role0, role1, role2, role3 };
enum class RetailInputRoleError { none, wrong_thread, invalid_role, invalid_action_row, allocation, native_exception };
struct RetailInputRoleFailure { RetailInputRoleError kind{RetailInputRoleError::none}; };

struct NativeRetailInputRoleObservation {
    bool initialized, dispatch_blocked, analog_mapping;
    std::uint32_t previous, current, rising;
    std::optional<std::array<std::uint8_t, 4>> axes;
    std::shared_ptr<NativeRetailInputParticipant> participant;
};

// Owned role records initialized by the 281E00 action-record initialization.
// A role is a retail identity, independent of a host sample route or a real
// participant. This component does not bind either association implicitly.
class NativeRetailInputRoles final {
public:
    static std::shared_ptr<NativeRetailInputRoles> create(
        RetailInputRoleFailure&) noexcept;
    NativeRetailInputRoles(const NativeRetailInputRoles&) = delete;
    NativeRetailInputRoles& operator=(const NativeRetailInputRoles&) = delete;

    std::optional<NativeRetailInputRoleObservation> observe(
        RetailInputRole, RetailInputRoleFailure&) const noexcept;
    std::optional<RetailActionRowObservation> observe_action_row(
        RetailInputRole, std::size_t row, RetailInputRoleFailure&) const noexcept;

private:
    NativeRetailInputRoles() noexcept;
    struct RoleState {
        std::array<RetailActionBinding, 16> action_rows{};
        std::uint32_t previous{}, current{}, rising{};
        std::optional<std::array<std::uint8_t, 4>> axes;
        std::weak_ptr<NativeRetailInputParticipant> participant;
        bool initialized{}, dispatch_blocked{}, analog_mapping{true};
    };
    const RoleState* role(RetailInputRole, RetailInputRoleFailure&) const noexcept;

    // Four initialized source roles. This is not a physical input or host
    // controller capacity; the platform owner stores endpoints independently.
    std::array<RoleState, 4> roles_;
    const std::thread::id thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
