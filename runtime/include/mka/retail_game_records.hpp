#pragma once

#include "mka/retail_game_session.hpp"
#include "mka/retail_game_records_values.hpp"
#include "mka/retail_game_record_reset.hpp"
#include "mka/retail_game_resource_registry.hpp"
#include "mka/retail_initial_game_tasks.hpp"
#include "mka/retail_input_roles.hpp"
#include "mka/retail_memory_owners.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_participants.hpp"
#include "mka/retail_profile_main_state.hpp"

#include <cstdint>

namespace mka::retail {

enum class RetailGameRecordsError {
    none, missing_owner, wrong_thread, session, random
};
struct RetailGameRecordsFailure {
    RetailGameRecordsError kind{RetailGameRecordsError::none};
    const char* operation{ "" };
    RetailGameSessionFailure session;
    RetailRandomFailure random;
};

enum class RetailGameRecordsMainError {
    none, missing_owner, wrong_thread, foreign_owner, session, source_values,
    roles, registry, controls, initial_task, mode, heap, overlay, profile,
    reset, pool, domain, task_lists, task_body, participants,
    text_callbacks, after_effect_unknown
};
struct RetailGameRecordsMainFailure {
    RetailGameRecordsMainError kind{RetailGameRecordsMainError::none};
    const char* operation{ "" };
    RetailGameRecordsFailure values;
    RetailGameSessionFailure session;
    RetailInputRoleFailure roles;
    RetailGameResourceRegistryFailure registry;
    RetailGameControlsFailure controls;
    RetailInitialTaskStateFailure initial_task;
    RetailModeFailure mode;
    RetailMemoryOwnerFailure heap;
    RetailActiveOverlayFailure overlay;
    RetailProfileMainFailure profile;
    RetailGameRecordResetFailure reset;
    RetailGamePoolFailure pool;
    RetailTaskDomainFailure domain;
    RetailGameTaskListFailure task_lists;
    RetailInitialGameTaskFailure task_body;
    RetailParticipantFailure participants;
    RetailGameInfoResourceFailure global_values;
};

// Complete straight source effects before 281E00 in 1303F0. The caller
// continues at 281E00 in the same Main initializer; this is not a whole-
// initializer success binding.
bool apply_retail_game_records_prefix_1303f0(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;

// Direct 1303F0 pair before 2173D0 and final 163FD0 stores.
bool clear_retail_game_records_pair_1303f0(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;
bool reset_retail_game_records_tail_163fd0(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;
bool reset_retail_game_records_word_173570(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;
bool store_retail_game_records_bytes_127370(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;
bool clear_retail_game_records_13c180_early_words(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;
bool clear_retail_game_records_13c180_late_words(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;
bool clear_retail_game_records_16c1b0_word(
    NativeRetailGameSession&,RetailGameRecordsFailure&) noexcept;

} // namespace mka::retail
