#include "mka/retail_scene_events.hpp"
#include "mka/retail_initial_objects.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <utility>

namespace mka::retail {

NativeRetailSceneEventGraph::NativeRetailSceneEventGraph() noexcept {
    set_event_mask(0xffffu);
    elapsed_word_ = 10000u;
}

NativeRetailSceneEventGraph::~NativeRetailSceneEventGraph() noexcept {
    if (!ownership_empty()) {
        std::fputs("Native retail scene graph still owns live nodes or actions at destruction\n", stderr);
        std::terminate();
    }
}

void NativeRetailSceneEventGraph::set_event_mask(std::uint16_t mask) noexcept {
    if (mask != 0) {
        mask_ = {true, mask, mask};
    } else if (mask_.enabled) {
        mask_ = {false, 0, mask_.current};
    }
}

bool NativeRetailSceneEventGraph::primary_blocked() const noexcept {
    return std::any_of(primary_.begin(), primary_.end(), [](const auto& action) {
        return action.active && action.blocks_local_delivery;
    });
}

bool NativeRetailSceneEventGraph::permits_local_delivery(
    std::uint32_t selector, bool forced) const noexcept {
    return forced || (!primary_blocked() &&
        (std::uint32_t(mask_.current) & (std::uint32_t{1} << (selector & 31u))) != 0);
}

void NativeRetailSceneEventGraph::accumulate_elapsed(std::uint32_t delta) noexcept {
    elapsed_word_ += delta;
}

RetailSceneGraphObservation NativeRetailSceneEventGraph::observe() const noexcept {
    return {mask_, elapsed_word_, bool(root_), bool(current_), primary_blocked(),
        active_selections_.size(), primary_.size(), secondary_.size()};
}

bool NativeRetailSceneEventGraph::install_root(std::shared_ptr<NativeRetailSceneNode> root) {
    if (!root || root_) return false;
    root_ = std::move(root);
    return true;
}

bool NativeRetailSceneEventGraph::select_current(std::shared_ptr<NativeRetailSceneNode> node) {
    if (!root_) return false;
    current_.swap(node);
    return true;
}

bool NativeRetailSceneEventGraph::record_active_selection(std::shared_ptr<NativeRetailSceneNode> node) {
    if (!root_ || !node) return false;
    if (std::find(active_selections_.begin(), active_selections_.end(), node) == active_selections_.end()) {
        active_selections_.push_back(std::move(node));
    }
    return true;
}

bool NativeRetailSceneEventGraph::forget_active_selection(const NativeRetailSceneNode& node) noexcept {
    const auto found = std::find_if(active_selections_.begin(), active_selections_.end(),
        [&](const auto& owned) { return owned.get() == &node; });
    if (found == active_selections_.end()) return false;
    auto retired = std::move(*found);
    active_selections_.erase(found);
    return true;
}

bool NativeRetailSceneEventGraph::release_root_after_retirement(const NativeRetailSceneNode& node) noexcept {
    if (root_.get() != &node || current_ || !active_selections_.empty()) return false;
    auto retired = std::move(root_);
    return true;
}

NativeRetailSceneEventGraph::Queue* NativeRetailSceneEventGraph::queue(RetailSceneQueue kind) noexcept {
    switch (kind) {
    case RetailSceneQueue::primary: return &primary_;
    case RetailSceneQueue::secondary: return &secondary_;
    }
    return nullptr;
}

bool NativeRetailSceneEventGraph::retain_action(RetailSceneQueue kind,
    std::shared_ptr<NativeRetailSceneAction> owner, bool active, bool blocks_local_delivery) {
    auto* target = queue(kind);
    if (!target || !owner) return false;
    const auto same_owner = [&](const auto& item) { return item.owner == owner; };
    if (std::any_of(primary_.begin(), primary_.end(), same_owner) ||
        std::any_of(secondary_.begin(), secondary_.end(), same_owner)) return false;
    target->push_back({std::move(owner), active, blocks_local_delivery});
    return true;
}

bool NativeRetailSceneEventGraph::update_action(RetailSceneQueue kind,
    const NativeRetailSceneAction& action, bool active, bool blocks_local_delivery) noexcept {
    auto* target = queue(kind);
    if (!target) return false;
    const auto found = std::find_if(target->begin(), target->end(),
        [&](const auto& item) { return item.owner.get() == &action; });
    if (found == target->end()) return false;
    found->active = active;
    found->blocks_local_delivery = blocks_local_delivery;
    return true;
}

bool NativeRetailSceneEventGraph::forget_action(RetailSceneQueue kind,
    const NativeRetailSceneAction& action) noexcept {
    auto* target = queue(kind);
    if (!target) return false;
    const auto found = std::find_if(target->begin(), target->end(),
        [&](const auto& item) { return item.owner.get() == &action; });
    if (found == target->end()) return false;
    auto retired = std::move(found->owner);
    target->erase(found);
    return true;
}

bool NativeRetailSceneEventGraph::ownership_empty() const noexcept {
    return !root_ && !current_ && active_selections_.empty() && primary_.empty() && secondary_.empty();
}

bool NativeRetailSceneEventGraph::reset_empty() noexcept {
    if (!ownership_empty()) return false;
    // 4C4C30 does not modify the mask. 4C4E20 separately re-enables FFFF.
    elapsed_word_ = 10000u;
    return true;
}

namespace main_binding {
std::shared_ptr<NativeRetailSceneEventGraph> construct_retail_scene_event_graph(
    NativeRetailMainState&, StartupFailure& failure) {
    failure = {};
    return std::make_shared<NativeRetailSceneEventGraph>();
}
} // namespace main_binding
} // namespace mka::retail
