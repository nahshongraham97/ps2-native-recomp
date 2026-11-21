#pragma once
#include "mka/retail_task_execution.hpp"
#include "mka/retail_game_pool.hpp"
#include "mka/retail_observer_list.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace mka::retail {

enum class RetailTaskRegistryError {
    none, wrong_thread, invalid_body, foreign_task, already_linked,
    retired_task, retention_guard, callout_active, after_effect_unknown,
    unsupported_source_branch, game_pool, observer_list, execution, allocation,
    native_exception
};
struct RetailTaskRegistryFailure {
    RetailTaskRegistryError kind{RetailTaskRegistryError::none};
    const char* operation{""};
    RetailGamePoolFailure pool{};
    RetailObserverListFailure observer{};
    RetailTaskExecutionFailure execution{};
};

// A published task must have executable native code and a real retirement
// capability. Test clients provide explicit CPU-only bodies. The retail body
// producers (18E100, 2800F0 and others) remain separate source work.
class NativeRetailTaskBody : public NativeRetailTaskContinuation {
public:
    virtual ~NativeRetailTaskBody() = default;
    // 12FA00 invokes the callback before 12EE70 applies current-task/bit2
    // release guards. Implementations must preserve effects even when false.
    virtual void cleanup(bool release_allowed) = 0;
};

class NativeRetailTaskObject final {
public:
    ~NativeRetailTaskObject() = default;
    NativeRetailTaskObject(const NativeRetailTaskObject&) = delete;
    NativeRetailTaskObject& operator=(const NativeRetailTaskObject&) = delete;
private:
    friend class NativeRetailTaskRegistry;
    friend class NativeRetailTaskExecution;
    NativeRetailTaskObject(std::uint32_t kind, std::int32_t priority,
        std::uint32_t cookie, std::shared_ptr<NativeRetailTaskBody> body) noexcept
        : kind_(kind), priority_(priority), cookie_(cookie),
          body_(body), continuation_(std::move(body)) {}
    std::uint32_t kind_{}, cookie_{};
    std::int32_t priority_{};
    std::shared_ptr<NativeRetailTaskBody> body_;
    // 12EC50 clears both heads. 12EE70 retires +E8 then +EC in that order.
    std::shared_ptr<NativeRetailObserverList> observers_e8_, observers_ec_;
    std::shared_ptr<NativeRetailTaskContinuation> continuation_, wait_resume_;
    float stored_wait_{};
    bool resume_pending_{}, execution_active_{}, execution_transfer_pending_{};
    bool linked_{}, retired_{}, retain_flag2_{};
};

struct RetailTaskLinkObservation {
    std::shared_ptr<NativeRetailTaskObject> object;
    std::uint32_t kind, object_cookie, cached_cookie;
    std::int32_t priority;
    std::uint8_t link_flags;
};
struct RetailTaskRegistryObservation {
    std::vector<RetailTaskLinkObservation> ordered;
    std::size_t retained_links{}, recycled_links{}, pending_links{};
    bool after_effect_unknown{};
};

// Native owned ordered links for the source 12ED10 lookup and 12F260 insert
// subset. No source task kind is born here. Publication requires an actual
// native body. The initial zero-flag 12E670 branch is available; other task
// branches and the second root need their real producers. The source cookie
// is supplied by its real producer; this owner never manufactures a
// monotonic generation or a 32-bit task capacity.
class NativeRetailTaskRegistry final {
public:
    static std::unique_ptr<NativeRetailTaskRegistry> create(
        RetailTaskRegistryFailure&) noexcept;
    static std::unique_ptr<NativeRetailTaskRegistry> create(
        std::shared_ptr<NativeRetailGamePool>, RetailTaskRegistryFailure&) noexcept;
    NativeRetailTaskRegistry(const NativeRetailTaskRegistry&) = delete;
    NativeRetailTaskRegistry& operator=(const NativeRetailTaskRegistry&) = delete;

    std::shared_ptr<NativeRetailTaskObject> publish_task(
        std::uint32_t kind, std::int32_t priority, std::uint32_t source_cookie,
        std::shared_ptr<NativeRetailTaskBody> body,
        RetailTaskRegistryFailure&);
    // A source body with a per-task +E8 list prepares that same native list
    // before publication, then gives the registry its retained identity.
    std::shared_ptr<NativeRetailTaskObject> publish_task(
        std::uint32_t kind, std::int32_t priority, std::uint32_t source_cookie,
        std::shared_ptr<NativeRetailTaskBody> body,
        std::shared_ptr<NativeRetailObserverList> observers_e8,
        std::shared_ptr<NativeRetailObserverList> observers_ec,
        RetailTaskRegistryFailure&);
    // Engaged null means the original not-found result. This lookup may
    // retire stale links and invoke real cleanup; control transfers propagate.
    std::optional<std::shared_ptr<NativeRetailTaskObject>> find_first_live(
        std::uint32_t kind, RetailTaskRegistryFailure&);
    bool store_object_cookie_from_source(
        const std::shared_ptr<NativeRetailTaskObject>&, std::uint32_t,
        RetailTaskRegistryFailure&) noexcept;
    bool store_link_flags_from_source(
        const std::shared_ptr<NativeRetailTaskObject>&, std::uint8_t,
        RetailTaskRegistryFailure&) noexcept;
    bool store_object_retention_flag2_from_source(
        const std::shared_ptr<NativeRetailTaskObject>&, bool,
        RetailTaskRegistryFailure&) noexcept;
    bool set_current_task(const std::shared_ptr<NativeRetailTaskObject>&,
        RetailTaskRegistryFailure&) noexcept;
    bool clear_current_task(RetailTaskRegistryFailure&) noexcept;
    std::shared_ptr<NativeRetailObserverList> current_task_observers_from_158080(
        RetailTaskRegistryFailure&) noexcept;
    // Source 12F030 clears selected/current, then 130AC0 retires first-root
    // live tasks through their actual virtual cleanup before link recycling.
    bool cancel_first_root_from_12f030(RetailTaskRegistryFailure&);
    bool dispatch_first_root_zero_flags_from_12e670(
        bool source_bit5_inhibited, bool source_root_wait_inhibited,
        RetailTaskRegistryFailure&);
    // Explicit 12FA00 link retirement. A flag-bit1 link is left unchanged.
    bool retire_link(const std::shared_ptr<NativeRetailTaskObject>&,
        RetailTaskRegistryFailure&);
    std::optional<RetailTaskRegistryObservation> observe(
        RetailTaskRegistryFailure&) const noexcept;

private:
    friend class NativeRetailTaskExecution;
    struct Link {
        std::shared_ptr<NativeRetailTaskObject> object;
        std::uint32_t cached_cookie{};
        std::uint8_t flags{};
        std::optional<NativeRetailGamePoolLink> pool_lease;
    };
    NativeRetailTaskRegistry() = default;
    bool access(RetailTaskRegistryFailure&) const noexcept;
    bool owner_thread(RetailTaskRegistryFailure&) const noexcept;
    bool owns(const std::shared_ptr<NativeRetailTaskObject>&) const noexcept;
    std::list<Link>::iterator find_active(
        const std::shared_ptr<NativeRetailTaskObject>&) noexcept;
    bool retire_active(std::list<Link>::iterator, RetailTaskRegistryFailure&);
    bool release_pool_link(Link&, RetailTaskRegistryFailure&) noexcept;
    bool drain_task_observers_from_12ee70(
        NativeRetailTaskObject&, RetailTaskRegistryFailure&);

    std::shared_ptr<NativeRetailGamePool> pool_;
    std::list<Link> active_, retained_, pending_, recycled_;
    std::shared_ptr<NativeRetailTaskObject> selected_, current_;
    bool in_callout_{}, after_effect_unknown_{}, execution_call_active_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
