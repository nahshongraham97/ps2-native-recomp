#pragma once

#include "mka/retail_profile_menu.hpp"

#include <cstdint>

namespace mka::retail {
struct NativeRetailMainState;

// The caller supplies the complete source kind (188720 passes role+0x11004).
// A true result means the source 12EDC0 visitor completed. False is a named
// native failure; 12EDC0 has no retail boolean return value.
bool retire_retail_profile_tasks_from_12edc0(
    NativeRetailMainState&, std::uint32_t full_kind, RetailProfileMenuFailure&);
} // namespace mka::retail
