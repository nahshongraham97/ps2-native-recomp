#pragma once
#include "mka/retail_initial_task_required.hpp"
#include "mka/retail_scene_owners.hpp"
#include <optional>
#include <thread>
#include <vector>
namespace mka::retail {
class NativeRetailMainLifetime;
struct StartupFailure;
// Native presentation registration. The transition mode, child procedure and
// 27B1D0 stage are distinct proved source domains, never history-stack aliases.
// Callers supply actual asset/HUD data; this does not invent any game-mode body.
struct RetailModeScenePlan {
    std::uint32_t transition_mode{};
    RetailInitialTransitionBody child{RetailInitialTransitionBody::source_245930};
    std::int32_t dispatch_stage{};
    RetailFirstSceneConfig scene;
};
class NativeRetailModeSceneRoutes final {
public:
    static bool install(NativeRetailMainState&, std::shared_ptr<NativeRetailRendererCore>,
        std::vector<RetailModeScenePlan>, StartupFailure&) noexcept;
    bool request(std::uint32_t, RetailInitialTransitionBody) noexcept;
    bool dispatch(NativeRetailMainState&, StartupFailure&);
    bool render_pending() const noexcept { return pending_.has_value(); }
    std::optional<std::uint32_t> active_mode() const noexcept;
    std::optional<std::int32_t> active_stage() const noexcept;
    std::size_t committed_transitions() const noexcept { return transitions_; }
private:
    NativeRetailModeSceneRoutes(std::weak_ptr<NativeRetailMainLifetime>,
        std::shared_ptr<NativeRetailRendererCore>, std::vector<RetailModeScenePlan>);
    bool access(StartupFailure&) const noexcept;
    const std::thread::id thread_{std::this_thread::get_id()};
    std::weak_ptr<NativeRetailMainLifetime> lifetime_;
    std::shared_ptr<NativeRetailRendererCore> core_;
    std::vector<RetailModeScenePlan> plans_;
    std::optional<std::size_t> pending_, active_;
    const char* request_failure_{};
    std::size_t transitions_{};
    bool busy_{};
};
} // namespace mka::retail

