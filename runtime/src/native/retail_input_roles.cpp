#include "mka/retail_input_roles.hpp"
#include <new>

namespace mka::retail {
namespace {
constexpr std::array<RetailActionBinding, 16> action_bindings{{
    {0x0001, 0x1607c0, 0x62a008},
    {0x0002, 0x1604c0, 0x62a010},
    {0x0004, 0x160a10, 0x62a018},
    {0x0008, 0x160640, 0x62a020},
    {0x0010, 0x161110, 0x62a028},
    {0x0020, 0x160b90, 0x62a030},
    {0x0040, 0x160ef0, 0x62a038},
    {0x0080, 0x160d40, 0x62a040},
    {0x0100, 0x160100, 0x62a048},
    {0x0200, 0x15ff60, 0x62a058},
    {0x0400, 0x15fe10, 0x62a068},
    {0x0800, 0x160250, 0x62a078},
    {0x1000, 0x1616b0, 0x62a088},
    {0x2000, 0x1612c0, 0x62a090},
    {0x4000, 0x161560, 0x62a098},
    {0x8000, 0x161410, 0x62a0a0},
}};
} // namespace

const std::array<RetailActionBinding, 16>& retail_default_action_bindings() noexcept {
    return action_bindings;
}

std::uint32_t retail_canonical_action_index(std::uint32_t mask, std::uint32_t row) noexcept {
    for (std::uint32_t index = 0; index != action_bindings.size(); ++index) {
        if (action_bindings[index].rising_mask == mask) return index;
    }
    return row;
}

NativeRetailInputRoles::NativeRetailInputRoles() noexcept {
    for (auto& state : roles_) state.action_rows = retail_default_action_bindings();
}

bool NativeRetailInputRoles::initialize_roles_from_281e00(
    RetailInputRoleFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != thread_) {
        failure.kind = RetailInputRoleError::wrong_thread;
        return false;
    }
    for (auto& state : roles_) {
        state = RoleState{};
        state.action_rows = retail_default_action_bindings();
    }
    // 281E00 does not write the separate association count.
    return true;
}

bool NativeRetailInputRoles::reset_first_two_mapping_tables_from_281870(
    RetailInputRoleFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != thread_) {
        failure.kind = RetailInputRoleError::wrong_thread;
        return false;
    }
    for (std::size_t position = 0; position != 2; ++position) {
        mapping_reset_words_[position] = 0;
        for (std::size_t row = 0; row != 16; ++row) {
            mapping_tables_[position][0][row] = retail_default_action_bindings()[row];
            mapping_tables_[position][1][row] = retail_default_action_bindings()[row];
        }
    }
    roles_[0].source_control_word = 0;
    roles_[1].source_control_word = 0;
    return true;
}

std::optional<std::array<RetailActionBinding,16>>
NativeRetailInputRoles::observe_mapping_table(
    std::size_t position, std::size_t variant,
    RetailInputRoleFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != thread_) {
        failure.kind = RetailInputRoleError::wrong_thread;
        return {};
    }
    if (position >= mapping_tables_.size() ||
        variant >= mapping_tables_[position].size()) {
        failure.kind = RetailInputRoleError::invalid_mapping_table;
        return {};
    }
    return mapping_tables_[position][variant];
}

std::shared_ptr<NativeRetailInputRoles> NativeRetailInputRoles::create(
    RetailInputRoleFailure& failure) noexcept {
    failure = {};
    try {
        return std::shared_ptr<NativeRetailInputRoles>(new NativeRetailInputRoles);
    } catch (const std::bad_alloc&) {
        failure.kind = RetailInputRoleError::allocation;
    } catch (...) {
        failure.kind = RetailInputRoleError::native_exception;
    }
    return {};
}

const NativeRetailInputRoles::RoleState* NativeRetailInputRoles::role(
    RetailInputRole role_id, RetailInputRoleFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != thread_) {
        failure.kind = RetailInputRoleError::wrong_thread;
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(role_id);
    if (index >= roles_.size()) {
        failure.kind = RetailInputRoleError::invalid_role;
        return nullptr;
    }
    return &roles_[index];
}

std::optional<NativeRetailInputRoleObservation> NativeRetailInputRoles::observe(
    RetailInputRole role_id, RetailInputRoleFailure& failure) const noexcept {
    const auto* state = role(role_id, failure);
    if (!state) return {};
    return NativeRetailInputRoleObservation{state->initialized, state->dispatch_blocked,
        state->analog_mapping, state->previous, state->current, state->rising, state->axes,
        state->participant.lock(), state->source_control_word};
}

bool NativeRetailInputRoles::clear_digital_first_three_from_163ba0(
    RetailInputRoleFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != thread_) {
        failure.kind = RetailInputRoleError::wrong_thread;
        return false;
    }
    for (std::size_t index = 0; index != 3; ++index) {
        roles_[index].previous = 0;
        roles_[index].current = 0;
        roles_[index].rising = 0;
    }
    return true;
}

bool NativeRetailInputRoles::clear_dispatch_blocked_first_three_from_163a50(
    RetailInputRoleFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != thread_) {
        failure.kind = RetailInputRoleError::wrong_thread;
        return false;
    }
    for (std::size_t index = 0; index != 3; ++index)
        if (roles_[index].participant.lock())
            roles_[index].dispatch_blocked = false;
    return true;
}

std::optional<RetailActionRowObservation> NativeRetailInputRoles::observe_action_row(
    RetailInputRole role_id, std::size_t row, RetailInputRoleFailure& failure) const noexcept {
    const auto* state = role(role_id, failure);
    if (!state) return {};
    if (row >= state->action_rows.size()) {
        failure.kind = RetailInputRoleError::invalid_action_row;
        return {};
    }
    return RetailActionRowObservation{state->action_rows[row], state->rising};
}

} // namespace mka::retail
