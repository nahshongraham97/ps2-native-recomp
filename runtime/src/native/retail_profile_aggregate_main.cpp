#include "mka/retail_profile_aggregate_main.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail {
bool refresh_retail_profile_aggregate_from_2432a0(NativeRetailMainState& main,
    std::span<const RetailProfileAggregateParentView> parents,
    RetailProfileAggregateMainFailure& failure) {
    failure = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        failure.kind = RetailProfileAggregateMainError::main_lifetime;
        failure.operation = "2432A0 Main lifetime and thread";
        return false;
    }
    const auto session = main.startup.game_session;
    if (!session) {
        failure.kind = RetailProfileAggregateMainError::session;
        failure.operation = "2432A0 retained game Session";
        return false;
    }
    auto* aggregate = session->profile_aggregate(failure.session);
    auto* words = session->game_records_values(failure.session);
    if (!aggregate || !words) {
        failure.kind = RetailProfileAggregateMainError::session;
        failure.operation = "2432A0 actual aggregate and live override owners";
        return false;
    }
    if (!aggregate->refresh_data_from_2432a0(parents, words->word_64e318, failure.data)) {
        failure.kind = RetailProfileAggregateMainError::data;
        failure.operation = "2432A0 aggregate data before menu effects";
        return false;
    }
    if (!run_retail_profile_menu_from_1888c0(main, failure.menu)) {
        failure.kind = RetailProfileAggregateMainError::menu;
        failure.operation = "2432A0 reached profile menu effects";
        return false;
    }
    return true;
}
} // namespace mka::retail
