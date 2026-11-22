#pragma once

#include "mka/retail_saved_profile_child.hpp"
#include "mka/retail_profile_preferences.hpp"
#include "mka/retail_profile_aggregate_main.hpp"
#include "mka/retail_input_assignments.hpp"
#include "mka/native_save_store.hpp"
#include <array>
#include <cstdint>
#include <thread>

namespace mka::retail {
class NativeRetailGameSession;
struct NativeRetailMainState;

enum class RetailSavedProfileRecordsError {
    none, wrong_thread, invalid_player, invalid_slot, invalid_child,
    main_lifetime, foreign_session, reentrant, prior_partial_effect,
    storage, child, aggregate, missing_input_owner, input_assignment, input_role,
    participant, allocation, native_exception
};
struct RetailSavedProfileRecordsFailure {
    RetailSavedProfileRecordsError kind{RetailSavedProfileRecordsError::none};
    const char* operation{""};
    RetailSavedProfileChildFailure child;
    RetailProfileAggregateMainFailure aggregate;
    RetailInputAssignmentFailure assignment;
    RetailInputRoleFailure role;
    RetailParticipantFailure participant;
    save::SaveFailure storage;
};

struct NativeRetailSavedProfileChildNode {
    RetailSavedProfileChildValue value;
    std::uint32_t in_use_word{};
};
struct NativeRetailSavedCardSlot {
    std::int32_t status_word{};
    std::uint32_t version_word{};
    std::uint8_t flag_byte_12{};
    RetailProfilePreferenceValues preferences;
    // Eight entries are retail profile/menu identities and the external save
    // payload's extent. They do not limit native storage files or host memory.
    std::array<NativeRetailSavedProfileChildNode, 8> children;
};
struct NativeRetailActivePlayerProfile {
    RetailSavedProfileChildValue value;
    // Original loader-zero values. Only the later245DD0 stores minus one.
    std::int32_t card_slot{}, profile_index{};
    std::uint32_t status_word{}; // 64E920(P0)/64E914(P1); meaning stays neutral.
    std::uint32_t player_status_word{8}; // Original file-backed64C720/724.
};

// Actual persistent native records. Session owns stable nodes before171B10
// reaches2432A0/menu callouts. NativeMainState never owns a competing copy.
// Borrowed pointers require the containing Session to remain retained.
class NativeRetailSavedProfileRecords final {
public:
    NativeRetailSavedProfileRecords(const NativeRetailSavedProfileRecords&) = delete;
    NativeRetailSavedProfileRecords& operator=(const NativeRetailSavedProfileRecords&) = delete;
    NativeRetailSavedCardSlot* card_slot(std::uint32_t, RetailSavedProfileRecordsFailure&) noexcept;
    NativeRetailActivePlayerProfile* active_player(std::uint32_t, RetailSavedProfileRecordsFailure&) noexcept;
    std::uint32_t* working_control_word(RetailSavedProfileRecordsFailure&) noexcept;
    bool initialize_from_171b10(NativeRetailMainState&, RetailSavedProfileRecordsFailure&);
    bool reset_first_two_from_246070(NativeRetailMainState&, RetailSavedProfileRecordsFailure&);

private:
    friend class NativeRetailGameSession;
    explicit NativeRetailSavedProfileRecords(NativeRetailGameSession& session) noexcept : session_(&session) {}
    bool access(RetailSavedProfileRecordsFailure&) const noexcept;
    bool admit(NativeRetailMainState&, RetailSavedProfileRecordsFailure&) const noexcept;
    bool after_effect_failure(RetailSavedProfileRecordsFailure&, RetailSavedProfileRecordsError,
                              const char*) noexcept;
    NativeRetailGameSession* session_;
    std::array<NativeRetailSavedCardSlot, 2> card_slots_;
    std::array<NativeRetailActivePlayerProfile, 2> active_players_;
    // The actual shared Working+40 cleared by514D30. Per-child KAF control40
    // is distinct. Active/staging resource IDs retain their existing owners.
    std::uint32_t working_control_word_{};
    bool active_call_{}, failed_after_effect_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
