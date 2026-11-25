#pragma once

#include "mka/content_volume.hpp"
#include "mka/native_heap.hpp"

#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailActiveOverlayError {
    none,
    wrong_thread,
    invalid_volume,
    missing_module,
    invalid_module,
    unsupported_initializer,
    inactive_parent,
    heap_retirement,
    heap_creation,
    allocation,
    host_capacity,
    native_exception
};

struct RetailActiveOverlayFailure {
    RetailActiveOverlayError kind{RetailActiveOverlayError::none};
    content::ContentVolumeFailure content{};
    memory::NativeHeapFailure heap{};
};

struct RetailActiveOverlayObservation {
    int selected_mode{};
    std::optional<memory::NativeHeapIdentity> companion_parent;
    bool active{};
};

// The only published parent is created by the source 3DDA60 -> 3DDA70
// mode-one path after reading and validating the actual GAMER overlay module.
class NativeRetailActiveOverlay final {
public:
    NativeRetailActiveOverlay(const NativeRetailActiveOverlay&) = delete;
    NativeRetailActiveOverlay& operator=(const NativeRetailActiveOverlay&) = delete;

    static std::unique_ptr<NativeRetailActiveOverlay> initialize_mode_one(
        const content::NativeContentVolume&, RetailActiveOverlayFailure&) noexcept;
    bool reload_mode_one(const content::NativeContentVolume&,
                         RetailActiveOverlayFailure&) noexcept;
    std::shared_ptr<memory::NativeHeapOwner> retained_companion_parent(
        RetailActiveOverlayFailure&) const noexcept;
    std::optional<RetailActiveOverlayObservation> observe(
        RetailActiveOverlayFailure&) const noexcept;

private:
    NativeRetailActiveOverlay() = default;
    std::thread::id owner_thread_{std::this_thread::get_id()};
    int selected_mode_{};
    std::shared_ptr<memory::NativeHeapOwner> companion_parent_;
    memory::NativeHeapIdentity companion_identity_{};
};

} // namespace mka::retail
