#pragma once

#include "mka/retail_game_info_resources.hpp"
#include "mka/retail_game_record_values.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_task_domain.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace mka::retail {
enum class RetailGameRecordResetError {
    none, wrong_thread, foreign_session, foreign_domain, missing_owner, task_domain,
    controls, roles, mode, global_values, registry,
    task_argument_unbound, partial_reset, allocation, native_exception
};
struct RetailGameRecordResetFailure {
    RetailGameRecordResetError kind{RetailGameRecordResetError::none};
    const char* operation{""};
    RetailGameSessionFailure session;
    RetailGameControlsFailure controls;
    RetailInputRoleFailure roles;
    RetailModeFailure mode;
    RetailTaskDomainFailure task_domain;
    RetailTaskRegistryFailure registry;
    RetailGameResourceRegistryFailure resource_registry;
    RetailGameInfoResourceFailure values;
};

// Persistent real game-record reset state. This owner is retained across Main
// reentry and borrows only the source Main mode/input owners during each call.
class NativeRetailGameRecordReset final {
public:
    static std::unique_ptr<NativeRetailGameRecordReset> create(
        std::shared_ptr<NativeRetailGameSession>,
        RetailGameRecordResetFailure&) noexcept;
    NativeRetailGameRecordReset(const NativeRetailGameRecordReset&)=delete;
    NativeRetailGameRecordReset& operator=(const NativeRetailGameRecordReset&)=delete;

    // Source 185EE0: cookie seed, scalar and full primary-word resets,
    // retained top-mode reset, 163A50 role/task effects, then four names.
    // NativeMainReentry from effectful task lookup propagates unchanged.
    bool run_185ee0(NativeRetailTaskDomain&,NativeRetailModeState&,
        NativeRetailInputRoles&,NativeRetailGameInfoResources&,
        NativeRetailGameResourceRegistry&,
        RetailGameRecordResetFailure&);
    std::optional<RetailGameRecordResetSnapshot> observe(
        RetailGameRecordResetFailure&) const noexcept;

private:
    explicit NativeRetailGameRecordReset(
        std::shared_ptr<NativeRetailGameSession> session) noexcept
        : session_(std::move(session)) {}
    bool access(RetailGameRecordResetFailure&) const noexcept;
    bool reset_163a50(NativeRetailTaskDomain&,NativeRetailInputRoles&,
        RetailGameRecordResetFailure&);
    void reset_152880(RetailGameRecordResetSnapshot&) noexcept;
    void reset_direct_185ee0(RetailGameRecordResetSnapshot&) noexcept;

    std::shared_ptr<NativeRetailGameSession> session_;
};
} // namespace mka::retail
