#pragma once

#include "mka/native_save_store.hpp"
#include "mka/retail_profile_aggregate.hpp"
#include "mka/retail_profile_preferences.hpp"
#include "mka/retail_saved_profile_defaults.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <thread>

namespace mka::retail {

enum class RetailSavedProfileRecordsError {
    none,
    wrong_thread,
    storage_missing,
    storage_invalid,
    already_initialized,
    not_initialized,
    invalid_slot,
    invalid_profile_index,
    invalid_player_index,
    defaults_failure,
    aggregate_failure,
    allocation,
    native_exception
};

struct RetailSavedProfileRecordsFailure {
    RetailSavedProfileRecordsError kind{RetailSavedProfileRecordsError::none};
    const char* operation{""};
    RetailSavedProfileDefaultFailure defaults{};
    RetailProfileAggregateFailure aggregate{};
    save::SaveFailure storage{};
};

// Represents one profile child (1 of 8 per memory card slot).
// In original retail PS2, each profile child record was 0x3C30 bytes.
// In native execution, this holds typed defaults, status, and aggregate projection.
struct NativeRetailSavedProfileChild {
    bool in_use{false};
    RetailSavedProfileDefaults defaults{};

    RetailProfileAggregateChildView make_aggregate_view() const noexcept {
        RetailProfileAggregateChildView view;
        view.gate_word_3c28 = in_use ? 1u : 0u;
        view.or_word_fc = defaults.command.bitword_1fc;
        view.or_words_100_108_110_118 = {0ull, 0ull, 0ull, 0ull};
        for (std::size_t i = 0; i < 40 && i < defaults.first_unassigned_indices.size(); ++i) {
            view.and_words_120[i] = static_cast<std::uint32_t>(defaults.first_unassigned_indices[i]);
        }
        for (std::size_t i = 0; i < 30 && i < defaults.second_unassigned_indices.size(); ++i) {
            view.and_words_1c0[i] = static_cast<std::uint32_t>(defaults.second_unassigned_indices[i]);
        }
        return view;
    }
};

// Represents one memory card slot (Slot 0 or Slot 1).
// Original PS2 stride was 0x1E218 bytes per slot.
class NativeRetailSavedCardSlot final {
public:
    static constexpr std::size_t profile_count = 8;

    NativeRetailSavedCardSlot() noexcept = default;

    std::int32_t status_word() const noexcept { return status_word_; }
    void set_status_word(std::int32_t status) noexcept { status_word_ = status; }

    std::uint32_t version_word() const noexcept { return version_word_; }
    void set_version_word(std::uint32_t version) noexcept { version_word_ = version; }

    std::uint8_t flag_byte_12() const noexcept { return flag_byte_12_; }
    void set_flag_byte_12(std::uint8_t value) noexcept { flag_byte_12_ = value; }

    const RetailProfilePreferenceValues& preferences() const noexcept { return preferences_; }
    void set_preferences(const RetailProfilePreferenceValues& prefs) noexcept { preferences_ = prefs; }

    const std::array<NativeRetailSavedProfileChild, profile_count>& children() const noexcept {
        return children_;
    }
    std::array<NativeRetailSavedProfileChild, profile_count>& children() noexcept {
        return children_;
    }

    const NativeRetailSavedProfileChild& child(std::size_t index) const {
        return children_.at(index);
    }
    NativeRetailSavedProfileChild& child(std::size_t index) {
        return children_.at(index);
    }

    bool is_profile_in_use(std::size_t index) const noexcept {
        if (index < profile_count) return children_[index].in_use;
        return false;
    }
    void set_profile_in_use(std::size_t index, bool in_use) noexcept {
        if (index < profile_count) children_[index].in_use = in_use;
    }

private:
    std::int32_t status_word_{-1};
    std::uint32_t version_word_{0};
    std::uint8_t flag_byte_12_{0};
    RetailProfilePreferenceValues preferences_{};
    std::array<NativeRetailSavedProfileChild, profile_count> children_{};
};

// Active player profile record (Player 0 and Player 1).
// In PS2 retail:
// P0 active buffer 0x6af670; slot index 0x64e91c; profile index 0x64e918; dirty flag 0x64e920; status DAT_0064c720[0].
// P1 active buffer 0x6aba40; slot index 0x64e910; profile index 0x64e90c; dirty flag 0x64e914; status DAT_0064c720[1].
struct NativeRetailActivePlayerProfile {
    std::int32_t card_slot{-1};      // -1 when unassigned; 0 or 1 when assigned
    std::int32_t profile_index{-1};  // -1 when unassigned; 0..7 when assigned
    std::uint32_t dirty_flag{0};     // 0 = clean
    std::int32_t status{8};          // 8 = default unattached state from 00245dd0
    std::uint32_t context_word{0};   // cleared by 002817b0
    RetailSavedProfileDefaults profile_data{};
};

// Container owning the saved profile records for both card slots and active players.
class NativeRetailSavedProfileRecords final {
public:
    static constexpr std::size_t card_slot_count = 2;
    static constexpr std::size_t active_player_count = 2;

    NativeRetailSavedProfileRecords() noexcept;
    NativeRetailSavedProfileRecords(const NativeRetailSavedProfileRecords&) = delete;
    NativeRetailSavedProfileRecords& operator=(const NativeRetailSavedProfileRecords&) = delete;

    // 171B10: Initialize records for both memory card slots, their child profiles,
    // default preferences, and aggregate. Also resets active players.
    bool initialize_records_from_171b10(
        std::int32_t language,
        RetailSavedProfileRecordsFailure& failure) noexcept;

    // 246070: Reset active profile children for players 0 and 1.
    bool reset_children_246070(
        std::int32_t language,
        RetailSavedProfileRecordsFailure& failure) noexcept;

    // 245DD0: Per-player profile reset. Clears in-use flag on the card slot
    // if a card profile was assigned, resets active profile data to defaults,
    // clears dirty flag and context, and sets player status to 8.
    bool reset_player_child_245dd0(
        std::size_t player_index,
        std::int32_t language,
        RetailSavedProfileRecordsFailure& failure) noexcept;

    // Associate a player with a saved card slot profile (for testing & gameplay)
    bool attach_player_profile(
        std::size_t player_index,
        std::size_t card_slot,
        std::size_t profile_index,
        RetailSavedProfileRecordsFailure& failure) noexcept;

    bool is_initialized() const noexcept { return initialized_; }

    const NativeRetailSavedCardSlot& slot(std::size_t index) const {
        return card_slots_.at(index);
    }
    NativeRetailSavedCardSlot& slot(std::size_t index) {
        return card_slots_.at(index);
    }

    const NativeRetailActivePlayerProfile& active_player(std::size_t index) const {
        return active_players_.at(index);
    }
    NativeRetailActivePlayerProfile& active_player(std::size_t index) {
        return active_players_.at(index);
    }

    const NativeRetailProfileAggregate& aggregate() const noexcept {
        return aggregate_;
    }

    std::uint32_t shared_word_7bfc80() const noexcept {
        return shared_word_7bfc80_;
    }

    // Refresh 2432A0 aggregate data
    bool refresh_aggregate(RetailSavedProfileRecordsFailure& failure) noexcept;

private:
    std::thread::id owner_;
    bool initialized_{false};
    std::uint32_t shared_word_7bfc80_{0};
    std::array<NativeRetailSavedCardSlot, card_slot_count> card_slots_{};
    std::array<NativeRetailActivePlayerProfile, active_player_count> active_players_{};
    NativeRetailProfileAggregate aggregate_{};
};

} // namespace mka::retail
