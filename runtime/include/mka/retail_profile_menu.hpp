#pragma once

#include "mka/retail_participants.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace mka::retail {
struct NativeRetailMainState;

enum class RetailProfileMenuError {
    none, main_lifetime, missing_mode_history, missing_participants,
    participant, required_child, allocation, native_exception_after_effect
};
struct RetailProfileMenuFailure {
    RetailProfileMenuError kind{RetailProfileMenuError::none};
    const char* operation{""};
    RetailParticipantFailure participant{};
};

// The actual 2432A0 caller invokes this after publishing its aggregate data,
// twice per 171B10 initialization. A true result includes the source's early
// return branches; it does not imply a populated menu was processed.
bool run_retail_profile_menu_from_1888c0(
    NativeRetailMainState&, RetailProfileMenuFailure&);

// Required source children. Production intentionally supplies no definitions
// until the actual scene-cookie producer, resource/action route and transition
// bodies are implemented. The bool/optional results are native error channels,
// not replacement values for the original void/pointer calls.
std::optional<bool> observe_retail_profile_live_scene_from_2f18f0(
    NativeRetailMainState&, RetailProfileMenuFailure&);
bool invoke_retail_profile_select_from_2f1630(
    NativeRetailMainState&, std::string_view source_path, RetailProfileMenuFailure&);
bool transition_retail_profile_from_18b0d0(
    NativeRetailMainState&, std::uint32_t participant, std::uint32_t argument,
    RetailProfileMenuFailure&);
bool transition_retail_profile_from_18a260(
    NativeRetailMainState&, std::uint32_t participant, RetailProfileMenuFailure&);

} // namespace mka::retail
