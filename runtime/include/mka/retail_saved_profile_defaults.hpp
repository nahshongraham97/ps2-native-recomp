#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mka::retail {

enum class RetailSavedProfileDefaultError {
    none, allocation, native_exception
};

struct RetailSavedProfileDefaultFailure {
    RetailSavedProfileDefaultError kind{RetailSavedProfileDefaultError::none};
    const char* operation{""};
};

// These are values of the reached 245CB0/514D30/4F3C80 leaves, not a
// replacement for the 3C30-byte retail child or the 171B10 parent.
// Source-bitword names identify fixed, proved stores; no offset-based access
// to a guest record is provided.
struct RetailSavedProfileGameplayDefaults {
    std::uint32_t bitword_1164{18};
    std::uint32_t bitword_1168{1}; // final 5F2550 overwrite, after initial -1
    std::uint32_t bitword_1188{4};
    std::uint32_t bitword_118c{1};
    std::uint32_t bitword_1190{27};
    std::uint32_t bitword_1194{0xffffffffu};
    std::uint32_t bitword_11a4{64};
    std::uint32_t bitword_11ac{64};
    std::uint32_t bitword_11b0{64};
};

struct RetailSavedProfileCommandDefault {
    std::int32_t modifier{};
    std::vector<std::uint32_t> input_codes; // Includes the source -1 terminator.
};

struct RetailSavedProfileCommandDefaults {
    std::array<std::string, 4> labels;
    std::array<RetailSavedProfileCommandDefault, 6> commands;
    std::uint32_t bitword_1fc{1};
    std::uint32_t bitword_264{4};
    std::uint32_t bitword_268{2};
    std::uint32_t bitword_2d0{1};
    std::uint32_t bitword_2d4{1};
    std::uint32_t bitword_2e0{10};
    std::uint32_t flag_bit2{4};
};

struct RetailSavedProfileDefaults {
    bool initialized{};
    std::string empty_name;
    std::array<std::uint32_t, 16> first_action_masks{};
    std::array<std::uint32_t, 16> second_action_masks{};
    RetailSavedProfileGameplayDefaults gameplay;
    RetailSavedProfileCommandDefaults command;
    std::array<std::int32_t, 40> first_unassigned_indices{};
    std::array<std::int32_t, 30> second_unassigned_indices{};
};

// language is the live 182E30 / Main.post_overlay_state input. The source
// maps 5 to 0, uses 0..4, and falls back to the one-space 600830 literal.
// shared_7bfc80 is the ONE retained shared source word owned by the caller.
// On native allocation failure the current output may contain ordered partial
// effects; the failure is named and the caller must not publish it as ready.
bool initialize_retail_saved_profile_known_defaults_from_245cb0(
    RetailSavedProfileDefaults&, std::int32_t language,
    std::uint32_t& shared_7bfc80,
    RetailSavedProfileDefaultFailure&) noexcept;

// Source 507F60 leaf. The input is a source/host byte string; unknown
// punctuation is ignored, modifier tokens replace the previous modifier.
bool decode_retail_saved_profile_command_from_507f60(
    std::string_view, RetailSavedProfileCommandDefault&,
    RetailSavedProfileDefaultFailure&) noexcept;

} // namespace mka::retail
