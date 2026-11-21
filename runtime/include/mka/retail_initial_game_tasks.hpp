#pragma once

#include "mka/retail_game_task_lists.hpp"
#include "mka/retail_task_domain.hpp"
#include "mka/retail_task_registry.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace mka::retail {

enum class RetailInitialGameTaskError {
    none, wrong_thread, missing_pool, foreign_pool, missing_lists,
    missing_callback, actor_branch_unbound, observer_list, domain,
    registry, retired_body,
    allocation, native_exception
};
struct RetailInitialGameTaskFailure {
    RetailInitialGameTaskError kind{RetailInitialGameTaskError::none};
    const char* operation{""};
    RetailGameTaskListFailure lists{};
    RetailObserverListFailure observer{};
    RetailTaskDomainFailure domain{};
    RetailTaskRegistryFailure registry{};
};
class RetailInitialGameTaskException final : public std::runtime_error {
public:
    RetailInitialGameTaskException(RetailInitialGameTaskError,const char*);
    RetailInitialGameTaskError kind() const noexcept { return kind_; }
private:
    RetailInitialGameTaskError kind_;
};

enum class RetailInitialGameTaskBodyKind {
    camera_13c220, actors_13a7a0, scene_16c1f0, per_task_158080
};
enum class RetailInitialGameTaskCallback {
    actor_13a870, camera_13c540, actor_pose_139af0,
    actor_motion_139bd0, scene_16c270, scene_227f20,
    per_task_1580b0
};

// A typed view supplied by the actual scene actor producer. The original
// 127120 validates the slot's saved identity before reading actor yaw. The
// populated-actor camera transform remains named unbound in this cut.
struct NativeRetailCameraActorState {
    std::uint32_t identity{};
    float yaw_radians{};
};
struct RetailInitialCameraObservation {
    std::array<float,9> basis{}; // 65B980/84/88, 990/94/98, 9A0/A4/A8.
    std::array<float,3> position{}; // 65B9B0/B4/B8.
    std::uint32_t flags{}; // 65B98C.
    bool actor_slot_bound{};
};

// Persistent native owner of the 127120 camera state and four exact body
// sequences born by 13C180/139A30/16C1B0/1588C0. Global callback roots
// are the one GameSession list owner; this object does not clone them.
class NativeRetailInitialGameTasks final :
    public std::enable_shared_from_this<NativeRetailInitialGameTasks> {
public:
    static std::shared_ptr<NativeRetailInitialGameTasks> create(
        std::shared_ptr<NativeRetailGameTaskLists>,
        std::shared_ptr<NativeRetailGamePool>,
        RetailInitialGameTaskFailure&) noexcept;
    bool same_pool(const std::shared_ptr<NativeRetailGamePool>&,
        RetailInitialGameTaskFailure&) const noexcept;
    bool same_lists(const std::shared_ptr<NativeRetailGameTaskLists>&,
        RetailInitialGameTaskFailure&) const noexcept;
    bool bind_camera_actor_from_source(
        std::shared_ptr<NativeRetailCameraActorState>,std::uint32_t saved_identity,
        RetailInitialGameTaskFailure&) noexcept;
    bool bind_callback_from_native_source(
        RetailInitialGameTaskCallback,
        std::shared_ptr<NativeRetailObserverCallback>,
        RetailInitialGameTaskFailure&) noexcept;
    std::optional<RetailInitialCameraObservation> observe_camera(
        RetailInitialGameTaskFailure&) const noexcept;
    std::shared_ptr<NativeRetailGamePool> retained_pool(
        RetailInitialGameTaskFailure&) const noexcept;
    bool run_source_body(RetailInitialGameTaskBodyKind,
        const std::shared_ptr<NativeRetailObserverList>& task_e8,
        RetailInitialGameTaskFailure&);
    // Called at each source constructor's 12EC50/12F100 point after its
    // real scalar/head clears. Returns the actual linked task handle.
    std::shared_ptr<NativeRetailTaskObject> publish_source_task(
        RetailInitialGameTaskBodyKind, NativeRetailTaskDomain&,
        RetailInitialGameTaskFailure&);
private:
    NativeRetailInitialGameTasks(std::shared_ptr<NativeRetailGameTaskLists>,
        std::shared_ptr<NativeRetailGamePool>) noexcept;
    bool access(RetailInitialGameTaskFailure&) const noexcept;
    bool run_camera_127120(RetailInitialGameTaskFailure&) noexcept;
    bool visit(RetailGameTaskListRoot,RetailInitialGameTaskCallback,
        bool actor_route,RetailInitialGameTaskFailure&);
    bool visit_task_e8(const std::shared_ptr<NativeRetailObserverList>&,
        RetailInitialGameTaskFailure&);
    std::shared_ptr<NativeRetailGameTaskLists> lists_;
    std::shared_ptr<NativeRetailGamePool> pool_;
    std::weak_ptr<NativeRetailCameraActorState> actor_slot_;
    std::uint32_t actor_saved_identity_{};
    RetailInitialCameraObservation camera_{};
    std::array<std::shared_ptr<NativeRetailObserverCallback>,7> callbacks_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

// The registry owns +E8/+EC and executes their 12EE70 retirement. The body
// performs its exact source-order frame effects and holds the same +E8 list
// for 158080's current-task callback walk.
class NativeRetailInitialGameTaskBody final : public NativeRetailTaskBody {
public:
    static std::shared_ptr<NativeRetailInitialGameTaskBody> create(
        RetailInitialGameTaskBodyKind,
        std::shared_ptr<NativeRetailInitialGameTasks>,
        std::shared_ptr<NativeRetailObserverList> task_e8,
        std::shared_ptr<NativeRetailObserverList> task_ec,
        RetailInitialGameTaskFailure&) noexcept;
    NativeTaskStep run() override;
    void cleanup(bool release_allowed) override;
private:
    NativeRetailInitialGameTaskBody(RetailInitialGameTaskBodyKind,
        std::shared_ptr<NativeRetailInitialGameTasks>,
        std::shared_ptr<NativeRetailObserverList>,
        std::shared_ptr<NativeRetailObserverList>) noexcept;
    RetailInitialGameTaskBodyKind kind_;
    std::shared_ptr<NativeRetailInitialGameTasks> owner_;
    std::shared_ptr<NativeRetailObserverList> task_e8_,task_ec_;
    bool retired_{};
    bool after_effect_unknown_{};
};

} // namespace mka::retail
