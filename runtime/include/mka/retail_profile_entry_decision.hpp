#pragma once

#include "mka/retail_profile_menu.hpp"

#include <cstdint>
#include <optional>

namespace mka::retail {
struct NativeRetailMainState;

// Exact 1892D0 V0 is 0 or 1. Disengaged optional is a native owner/child
// failure, never an invented retail result. The source accepts a 32-bit key
// but sign-extends it before its unsigned <64 admission check.
std::optional<std::uint32_t> decide_retail_profile_entry_from_1892d0(
    NativeRetailMainState&, std::uint32_t key, std::uint32_t selector,
    std::uint32_t role, RetailProfileMenuFailure&);

// Strong required 4EEE60 child. Its real retained profile first-byte owner
// and 9012 task lookup must be joined before production can link this symbol.
// A test may supply a fixture definition; there is no production default.
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState&, std::uint32_t role, RetailProfileMenuFailure&);
} // namespace mka::retail
