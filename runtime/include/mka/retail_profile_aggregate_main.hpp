#pragma once

#include "mka/retail_game_session.hpp"
#include "mka/retail_profile_menu.hpp"
#include <span>

namespace mka::retail {
struct NativeRetailMainState;
enum class RetailProfileAggregateMainError { none, main_lifetime, session, data, menu };
struct RetailProfileAggregateMainFailure {
    RetailProfileAggregateMainError kind{RetailProfileAggregateMainError::none};
    const char* operation{""};
    RetailGameSessionFailure session;
    RetailProfileAggregateFailure data;
    RetailProfileMenuFailure menu;
};

// Actual2432A0 flow: publish aggregate values on the retained Session, then
// perform1888C0 on that same Main. Menu effects may transfer control; published
// aggregate values survive. Complete171B10 must call this after EACH parent.
bool refresh_retail_profile_aggregate_from_2432a0(NativeRetailMainState&,
    std::span<const RetailProfileAggregateParentView>, RetailProfileAggregateMainFailure&);
} // namespace mka::retail
