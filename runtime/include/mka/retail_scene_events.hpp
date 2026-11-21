#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mka::retail {
class NativeRetailSceneNode;
class NativeRetailSceneAction;

enum class RetailSceneQueue { primary, secondary };
struct RetailSceneEventMask {
    bool enabled;
    std::uint16_t current, saved;
};
struct RetailSceneGraphObservation {
    RetailSceneEventMask mask;
    std::uint32_t elapsed_word;
    bool has_root, has_current, primary_blocked;
    std::size_t active_selections, primary_actions, secondary_actions;
};

// Main-thread-owned structural state of the original scene event manager.
// Actual node/resource construction, event delivery, queue execution, and
// populated scene retirement are supplied by the relevant source bodies.
class NativeRetailSceneEventGraph final {
public:
    NativeRetailSceneEventGraph() noexcept;
    NativeRetailSceneEventGraph(const NativeRetailSceneEventGraph&) = delete;
    NativeRetailSceneEventGraph& operator=(const NativeRetailSceneEventGraph&) = delete;
    // The real retirement path must detach every owned node/action first.
    // Destruction of a populated graph fails closed.
    ~NativeRetailSceneEventGraph() noexcept;

    void set_event_mask(std::uint16_t) noexcept;
    bool permits_local_delivery(std::uint32_t selector, bool forced) const noexcept;
    void accumulate_elapsed(std::uint32_t delta) noexcept;
    RetailSceneGraphObservation observe() const noexcept;

    // Commit structural state after the respective source child effects, not
    // as a replacement for activation or removal callbacks.
    bool install_root(std::shared_ptr<NativeRetailSceneNode>);
    bool select_current(std::shared_ptr<NativeRetailSceneNode>);
    bool record_active_selection(std::shared_ptr<NativeRetailSceneNode>);
    bool forget_active_selection(const NativeRetailSceneNode&) noexcept;
    bool release_root_after_retirement(const NativeRetailSceneNode&) noexcept;

    bool retain_action(RetailSceneQueue, std::shared_ptr<NativeRetailSceneAction>,
        bool active, bool blocks_local_delivery);
    bool update_action(RetailSceneQueue, const NativeRetailSceneAction&,
        bool active, bool blocks_local_delivery) noexcept;
    bool forget_action(RetailSceneQueue, const NativeRetailSceneAction&) noexcept;

    // 4C4C30 reset data is applicable after the actual resource and action
    // retirement. Refuse while any typed owner is still attached.
    bool reset_empty() noexcept;
private:
    struct QueuedAction {
        std::shared_ptr<NativeRetailSceneAction> owner;
        bool active, blocks_local_delivery;
    };
    using Queue = std::vector<QueuedAction>;
    Queue* queue(RetailSceneQueue) noexcept;
    bool primary_blocked() const noexcept;
    bool ownership_empty() const noexcept;

    RetailSceneEventMask mask_{};
    std::uint32_t elapsed_word_{};
    std::shared_ptr<NativeRetailSceneNode> root_, current_;
    std::vector<std::shared_ptr<NativeRetailSceneNode>> active_selections_;
    Queue primary_, secondary_;
};
} // namespace mka::retail
