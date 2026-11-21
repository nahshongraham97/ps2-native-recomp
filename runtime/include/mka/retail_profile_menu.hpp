#pragma once

#include "mka/retail_participants.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_profile_scene_object.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace mka::retail {
struct NativeRetailMainState;

enum class RetailProfileMenuError {
    none, main_lifetime, missing_mode_history, missing_participants,
    participant, required_child, allocation, native_exception_after_effect,
    session, scene_reference, scene_object, invalid_role, controls, task_domain,
    profile_entry_index
};
struct RetailProfileMenuFailure {
    RetailProfileMenuError kind{RetailProfileMenuError::none};
    const char* operation{""};
    RetailParticipantFailure participant{};
    RetailGameSessionFailure session{};
    RetailLiveSceneFailure scene_reference{};
    RetailProfileSceneObjectFailure scene_object{};
};

// The actual 2432A0 caller invokes this after publishing its aggregate data,
// twice per 171B10 initialization. A true result includes the source's early
// return branches; it does not imply a populated menu was processed.
bool run_retail_profile_menu_from_1888c0(
    NativeRetailMainState&, RetailProfileMenuFailure&);

// Source children use native error channels, not replacement retail results.
// Live reference observation uses the retained Session cookie pair. Resource
// action remains a strong requirement. The18A260/18B0D0 bodies preserve their
// source effects and require actual downstream decision/event/task services.
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
