#pragma once
#include "mka/retail_game_session.hpp"
#include <bit>
#include <cstdint>
#include <optional>
namespace mka::retail::generated {
std::optional<std::int32_t> retail_scalar_mode_get_185cb0(mka::retail::NativeRetailGameSession& session, mka::retail::RetailGameSessionFailure& failure);
bool retail_scalar_mode_set_185cc0(mka::retail::NativeRetailGameSession& session, std::int32_t arg0, mka::retail::RetailGameSessionFailure& failure);
} // namespace mka::retail::generated
