#pragma once

#include "mka/retail_profile_aggregate.hpp"
#include "mka/retail_profile_preferences.hpp"
#include "mka/retail_saved_profile_defaults.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace mka::retail {

struct NativeRetailMainState;
struct StartupFailure;
enum class RetailStartupGate;

// Single child profile record in a memory card slot (0x3C30 bytes in retail).
struct NativeRetailSavedChildRecord {
    RetailSavedProfileDefaults defaults{};
    std::uint32_t gate_word_3c28{0}; // Distinct aggregate gate at child+3C28
    std::uint32_t or_word_fc{0};
    std::array<std::uint64_t, 4> or_words_100_108_110_118{};
    std::array<std::uint32_t, 40> and_words_120{};
    std::array<std::uint32_t, 30> and_words_1c0{};

    RetailProfileAggregateChildView make_view() const noexcept {
        return RetailProfileAggregateChildView{
            .gate_word_3c28 = gate_word_3c28,
            .or_word_fc = or_word_fc,
            .or_words_100_108_110_118 = or_words_100_108_110_118,
            .and_words_120 = and_words_120,
            .and_words_1c0 = and_words_1c0,
        };
    }
};

// Represents one memory card slot / parent group (stride 0x1E218 in retail).
class NativeRetailSavedCardSlot final {
public:
    static constexpr std::size_t profile_count = 8;

    std::int32_t status_word() const noexcept { return status_word_; }
    void set_status_word(std::int32_t status) noexcept { status_word_ = status; }

    std::uint32_t version_word() const noexcept { return version_word_; }
    void set_version_word(std::uint32_t version) noexcept { version_word_ = version; }

    std::uint8_t flag_byte_0c() const noexcept { return flag_byte_0c_; }
    void set_flag_byte_0c(std::uint8_t flag) noexcept { flag_byte_0c_ = flag; }

    bool profile_in_use(std::size_t index) const noexcept {
        return index < profile_count && profile_in_use_[index] != 0;
    }
    void set_profile_in_use(std::size_t index, bool in_use) noexcept {
        if (index < profile_count) profile_in_use_[index] = in_use ? 1u : 0u;
    }

    const RetailProfilePreferenceValues& preferences() const noexcept { return preferences_; }
    void set_preferences(const RetailProfilePreferenceValues& prefs) noexcept { preferences_ = prefs; }

    NativeRetailSavedChildRecord& child(std::size_t index) noexcept { return children_[index]; }
    const NativeRetailSavedChildRecord& child(std::size_t index) const noexcept { return children_[index]; }

private:
    std::int32_t status_word_{-1}; // 0x00: published as -1 initially, 1 on completion
    std::uint32_t version_word_{0}; // 0x04: version
    std::uint8_t flag_byte_0c_{0};  // 0x0C: cleared by delay slot of 2432A0
    std::array<std::uint32_t, profile_count> profile_in_use_{}; // 0x18..0x34: slot in-use words
    RetailProfilePreferenceValues preferences_{};               // 0x38..0x90: preference values
    std::array<NativeRetailSavedChildRecord, profile_count> children_{}; // 0x90: 8 children
};

// Active player profile record (players 0 and 1, addresses 0x006AF670 and 0x006ABA40 in retail).
struct NativeRetailActivePlayerProfile {
    RetailSavedProfileDefaults defaults{};
    std::int32_t associated_card_slot{-1};     // gp - 0x57d4 (p0) / gp - 0x57e0 (p1)
    std::int32_t associated_profile_slot{-1};  // gp - 0x57d8 (p0) / gp - 0x57e4 (p1)
    std::uint32_t dirty_flag{0};               // gp - 0x57d0 (p0) / gp - 0x57dc (p1)
    std::uint32_t retained_per_player_word{0}; // gp - 0x565c (p0) / gp - 0x5660 (p1)
    std::uint32_t player_status{8};            // gp - 0x79d0 (0x0064C720 + player * 4)
};

class NativeRetailSavedProfileRecords final {
public:
    static constexpr std::size_t card_slot_count = 2;
    static constexpr std::size_t active_player_count = 2;

    NativeRetailSavedProfileRecords() noexcept;
    ~NativeRetailSavedProfileRecords() = default;
    NativeRetailSavedProfileRecords(const NativeRetailSavedProfileRecords&) = delete;
    NativeRetailSavedProfileRecords& operator=(const NativeRetailSavedProfileRecords&) = delete;

    // Ordered 171B10 procedure: initializes 2 card slots, 8 profiles each, with 2432A0 aggregate/menu
    // and 2955D0 preferences in exact retail order. Does NOT reset active players.
    bool initialize_records_from_171b10(NativeRetailMainState& main, StartupFailure& failure);

    // 246070 is the two-player wrapper calling 245DD0(0) then 245DD0(1).
    bool reset_saved_profile_children_246070(NativeRetailMainState& main, StartupFailure& failure);

    // 245DD0 single-player reset: cleans up role association, slot in-use, resets active record to defaults,
    // clears dirty flag, invokes 2817B0, and publishes player status 8.
    bool reset_player_child_245dd0(NativeRetailMainState& main, std::size_t player, StartupFailure& failure);

    bool is_initialized() const noexcept { return initialized_; }
    const NativeRetailSavedCardSlot& card_slot(std::size_t index) const;
    NativeRetailSavedCardSlot& card_slot(std::size_t index);
    const NativeRetailActivePlayerProfile& active_player(std::size_t player) const;
    NativeRetailActivePlayerProfile& active_player(std::size_t player);

private:
    bool access(StartupFailure& failure, const char* operation) const noexcept;

    std::array<NativeRetailSavedCardSlot, card_slot_count> card_slots_{};
    std::array<NativeRetailActivePlayerProfile, active_player_count> active_players_{};
    std::uint32_t shared_word_7bfc80_{0};
    bool initialized_{false};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
