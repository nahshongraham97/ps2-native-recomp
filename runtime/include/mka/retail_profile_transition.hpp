#pragma once

#include "mka/retail_profile_menu.hpp"

#include <cstdint>
#include <optional>

namespace mka::retail {

// The engaged value is the original 18A260 result (zero or one). A missing
// optional is a native owner/child failure and must stop its caller.
std::optional<bool> run_retail_profile_transition_from_18a260(
    NativeRetailMainState&, std::uint32_t selected_role,
    RetailProfileMenuFailure&);

} // namespace mka::retail
