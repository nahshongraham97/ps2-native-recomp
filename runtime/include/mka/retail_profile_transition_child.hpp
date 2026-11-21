#pragma once

#include "mka/retail_profile_menu.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace mka::retail {

// Immutable retail record data, not a host resource capacity. The source
// table contains64 entries; unproved native indices fail by name.
std::optional<std::uint32_t> retail_profile_entry_key_from_5a2bc0(
    std::uint32_t source_index, RetailProfileMenuFailure&) noexcept;

// Actual18B0D0 body is declared by retail_profile_menu.hpp. Its event/task
// children remain strong production requirements until their bodies exist.
// 189690 receives the role-indexed participant, not18B0D0's selected base.
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState&, const std::shared_ptr<NativeRetailInputParticipant>&,
    RetailProfileMenuFailure&);
bool apply_retail_profile_participant_from_188720(
    NativeRetailMainState&, std::uint32_t source_role, RetailProfileMenuFailure&);

} // namespace mka::retail
