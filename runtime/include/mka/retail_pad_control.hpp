#pragma once
#include "mka/retail_game_record_reset.hpp"

namespace mka::retail {
// Same live registry/control/role owners as source185EE0 and Main221C90.
// Real lookup/argument cleanup may transfer to Main and is not noexcept.
bool synchronize_retail_input_task_arguments(NativeRetailTaskDomain&,
    NativeRetailGameControls&,RetailGameRecordResetFailure&);
bool suspend_retail_input_from_163ba0(NativeRetailGameSession&,
    NativeRetailTaskDomain&,NativeRetailInputRoles&,RetailGameRecordResetFailure&);
bool resume_retail_input_from_163a50(NativeRetailGameSession&,
    NativeRetailTaskDomain&,NativeRetailInputRoles&,RetailGameRecordResetFailure&);
} // namespace mka::retail
