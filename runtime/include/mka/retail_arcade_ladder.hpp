#pragma once
#include <array>
#include <cstdint>
namespace mka::retail {
// Immutable retail game rows: consumer334750 uses four words at stride16.
// No guest pointer or native allocation/window capacity is stored here.
struct RetailArcadeLadderRow { std::array<std::uint32_t, 4> words{}; };
using RetailArcadeLadder = std::array<RetailArcadeLadderRow, 8>;
const std::array<RetailArcadeLadder, 19>& retail_arcade_ladders_from_source() noexcept;
// One actual Session value owner for reset334700 and its future consumers.
// Initial selector is original initialized data; the other fields are BSS0.
struct RetailArcadeLadderValues {
    std::int32_t progress{};
    std::uint32_t tier{};
    std::uint32_t selected_index{0xffffffffu};
    const RetailArcadeLadder* active_ladder{};
};
} // namespace mka::retail
