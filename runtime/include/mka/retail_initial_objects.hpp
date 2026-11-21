#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
class NativeRetailSceneEventGraph;

enum class RetailInitialRangeRoute { keys_0, keys_40000, keys_48000, keys_69000 };
enum class RetailInitialRangeOperation { primary, secondary };

// Owns the first constructor's scene-event graph and its source range owners.
// Construction does not publish runtime dispatch membership. Source 2F22A0
// binds those memberships later in the post-scene startup phase.
struct NativeRetailConstructorOwners final {
    explicit NativeRetailConstructorOwners(std::shared_ptr<NativeRetailSceneEventGraph>) noexcept;
    NativeRetailConstructorOwners(const NativeRetailConstructorOwners&) = delete;
    NativeRetailConstructorOwners& operator=(const NativeRetailConstructorOwners&) = delete;

    std::optional<RetailInitialRangeRoute> find_range_route(
        RetailInitialRangeOperation, std::uint32_t key) const noexcept;
    std::optional<bool> deliver_builtin_event(NativeRetailMainState&,
        std::uint32_t event, std::uint32_t argument, StartupFailure&);
    bool bind_dispatch_after_scene(NativeRetailMainState&, StartupFailure&);
private:
    struct InclusiveRange { std::uint32_t first, last; };
    struct RangeRoute { RetailInitialRangeRoute identity; InclusiveRange primary, secondary; };
    // Destroy the route records before the event graph they serve.
    std::shared_ptr<NativeRetailSceneEventGraph> graph_;
    const std::array<RangeRoute, 4> range_routes_{{
        {RetailInitialRangeRoute::keys_0, {0, 15000}, {0, 15000}},
        {RetailInitialRangeRoute::keys_40000, {40000, 47999}, {40000, 47999}},
        {RetailInitialRangeRoute::keys_48000, {48000, 49999}, {48000, 49999}},
        {RetailInitialRangeRoute::keys_69000, {69000, 69999}, {69000, 69999}}
    }};
    bool dispatch_bound_{};
};

namespace main_binding {
// Strong requirements for the actual scene resource/event graph and the
// concrete built-in handler. No empty graph or source-success default exists.
std::shared_ptr<NativeRetailSceneEventGraph> construct_retail_scene_event_graph(
    NativeRetailMainState&, StartupFailure&); // 4C4B30/4C4C30
bool reset_and_bind_retail_scene_event_graph(NativeRetailMainState&,
    NativeRetailSceneEventGraph&, StartupFailure&); // 4C4D80
std::optional<bool> deliver_retail_builtin_scene_event(NativeRetailMainState&,
    NativeRetailSceneEventGraph&, std::uint32_t event, std::uint32_t argument,
    StartupFailure&); // 2F6EA0
} // namespace main_binding
} // namespace mka::retail
