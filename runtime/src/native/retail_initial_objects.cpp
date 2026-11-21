#include "mka/retail_initial_objects.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <utility>

namespace mka::retail {
NativeRetailConstructorOwners::NativeRetailConstructorOwners(
    std::shared_ptr<NativeRetailSceneEventGraph> graph) noexcept
    : graph_(std::move(graph)) {}

std::optional<RetailInitialRangeRoute> NativeRetailConstructorOwners::find_range_route(
    RetailInitialRangeOperation operation, std::uint32_t key) const noexcept {
    if (!dispatch_bound_ || (operation != RetailInitialRangeOperation::primary &&
        operation != RetailInitialRangeOperation::secondary)) return {};
    // Source 4E0380 appends, so both unsigned inclusive lookups see the
    // same first-match order regardless of which interval is selected.
    for (const auto& route : range_routes_) {
        const auto interval = operation == RetailInitialRangeOperation::primary
            ? route.primary : route.secondary;
        if (interval.first <= key && key <= interval.last) return route.identity;
    }
    return {};
}

std::optional<bool> NativeRetailConstructorOwners::deliver_builtin_event(
    NativeRetailMainState& main, std::uint32_t event, std::uint32_t argument,
    StartupFailure& failure) {
    failure = {};
    if (!dispatch_bound_) return false;
    // Source 4C6A70 prepends. The four range owners all have the same
    // proven unhandled body, 4C2C50. Only the base-derived object handles
    // this direction, via 2F6EA0. The independent range list is unchanged.
    return main_binding::deliver_retail_builtin_scene_event(
        main, *graph_, event, argument, failure);
}

bool NativeRetailConstructorOwners::bind_dispatch_after_scene(
    NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (!graph_) {
        failure = {StartupError::native_exception, "retail initial scene graph missing"};
        return false;
    }
    // 4C4D80 executes on each call. The original one-shot flag gates
    // only the list additions, not this preceding reset.
    if (!main_binding::reset_and_bind_retail_scene_event_graph(main, *graph_, failure)) return false;
    dispatch_bound_ = true;
    return true;
}

namespace main_binding {
bool construct_retail_initial_aggregate(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.constructors) {
        failure = {StartupError::already_initialized, "retail initial constructor owners"};
        return false;
    }
    auto graph = construct_retail_scene_event_graph(main, failure);
    if (!graph) {
        if (failure.kind == StartupError::none) {
            failure = {StartupError::native_exception, "retail initial scene graph missing"};
        }
        return false;
    }
    // The actual graph owns nested resource teardown and outlives the route
    // owners. Source addresses and cleanup nodes are not host lifetime IDs.
    main.constructors = std::make_shared<NativeRetailConstructorOwners>(std::move(graph));
    return true;
}
} // namespace main_binding
} // namespace mka::retail
