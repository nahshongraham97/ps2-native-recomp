#pragma once

#include "mka/retail_scene_object.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {
class NativeRetailGameSession;

enum class RetailLiveSceneError {
    none, wrong_thread, foreign_object, retired_object
};
struct RetailLiveSceneFailure {
    RetailLiveSceneError kind{RetailLiveSceneError::none};
    const char* operation{""};
};
enum class RetailLiveScenePublishOutcome {
    null_no_effect, refused_valid_different, published
};
struct RetailLiveSceneObservation {
    bool reference_published{}, object_retained{}, object_live{}, valid{};
    std::uint32_t cached_cookie{};
    std::optional<std::uint32_t> current_cookie;
};

// Source 794D74/794D78 is a nonowning object/cookie pair. A real producer
// retains the object; this pair never becomes SceneEventGraph.current.
class NativeRetailSceneReference final {
public:
    NativeRetailSceneReference(const NativeRetailSceneReference&)=delete;
    NativeRetailSceneReference& operator=(const NativeRetailSceneReference&)=delete;

    // Disengaged optional is a native owner failure. Engaged null is the
    // source's null/stale result; a matching cookie of 0 or 1 is valid.
    std::optional<std::shared_ptr<NativeRetailSceneObject>> lookup_from_2f18f0(
        RetailLiveSceneFailure&) const noexcept;
    std::optional<RetailLiveScenePublishOutcome> publish_from_2f1920(
        std::shared_ptr<NativeRetailSceneObject>,RetailLiveSceneFailure&) noexcept;
    std::optional<RetailLiveScenePublishOutcome> publish_from_2f1ad0(
        std::shared_ptr<NativeRetailSceneObject>,RetailLiveSceneFailure&) noexcept;
    // Call 2F1AD0 reset only when its preceding source condition is satisfied.
    bool reset_from_2f1750(RetailLiveSceneFailure&) noexcept;
    bool reset_from_2f1ad0(RetailLiveSceneFailure&) noexcept;
    std::optional<RetailLiveSceneObservation> observe(
        RetailLiveSceneFailure&) const noexcept;

private:
    friend class NativeRetailGameSession;
    explicit NativeRetailSceneReference(NativeRetailGameSession& session) noexcept
        : session_(&session) {}
    bool access(RetailLiveSceneFailure&) const noexcept;
    std::optional<RetailLiveScenePublishOutcome> publish(
        std::shared_ptr<NativeRetailSceneObject>,const char*,
        RetailLiveSceneFailure&) noexcept;
    bool reset(RetailLiveSceneFailure&) noexcept;

    NativeRetailGameSession* session_; // Borrowed from enclosing Session.
    std::weak_ptr<NativeRetailSceneObject> object_;
    std::uint32_t cached_cookie_{};
    bool reference_published_{}; // Distinguishes null birth from expired weak owner.
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
