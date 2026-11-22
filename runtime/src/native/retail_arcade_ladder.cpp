#include "mka/retail_arcade_ladder.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail {
namespace {
constexpr std::array<RetailArcadeLadder, 19> ladders{{
#include "retail_arcade_ladder_data.inc"
}};
}
const std::array<RetailArcadeLadder, 19>& retail_arcade_ladders_from_source() noexcept {
    return ladders;
}
namespace initial_reset_required {
bool reset_selected_resource_334700(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure = {StartupError::game_session, "334700 Main lifetime/thread"};
        return false;
    }
    const auto session = main.startup.game_session;
    if (!session || !session->validate_current_thread(failure.game)) {
        failure.kind = StartupError::game_session;
        failure.operation = "334700 actual persistent Session unavailable";
        return false;
    }
    auto* selection = session->arcade_ladder_values(failure.game);
    auto* records = session->game_records_values(failure.game);
    auto* random = session->random(failure.game);
    if (!selection || !records || !random) {
        failure.kind = StartupError::game_session;
        failure.operation = "334700 actual ladder/record/RNG owners unavailable";
        return false;
    }
    selection->progress = -1;             //334710
    records->word_6602e8 = 0;              //334718; same shared game record
    selection->tier = 0;                  //334720 call delay slot
    const auto draw = random->low16_range(static_cast<std::uint32_t>(ladders.size()), failure.game.random);
    if (!draw || *draw >= ladders.size()) {
        failure.kind = StartupError::game_session;
        failure.operation = "334700 same-Session source185C30(19) draw";
        return false; // Preserve already performed source stores.
    }
    selection->selected_index = *draw;    //33472C after source low16 mask
    selection->active_ladder = &ladders[*draw]; //334740 actual native data pointer
    return true;
}
} // namespace initial_reset_required
} // namespace mka::retail
