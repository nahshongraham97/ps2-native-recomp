#include "mka/retail_active_overlay.hpp"

#include <array>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {

// GAMER.OVL from the pinned retail disc has one initializer. These offsets
// describe its serialized module, not a guest execution buffer or host heap.
constexpr std::size_t gamer_bytes = 0x180;
constexpr std::uint32_t gamer_base = 0x007c4880;
constexpr std::uint32_t gamer_initializer = 0x007c4980;

std::uint32_t word(const std::array<std::uint8_t, gamer_bytes>& bytes,
                   std::size_t offset) noexcept {
    return std::uint32_t(bytes[offset]) |
           (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) |
           (std::uint32_t(bytes[offset + 3]) << 24);
}

bool read_gamer_initializer(const content::NativeContentVolume& volume,
                            RetailActiveOverlayFailure& failure) {
    if (!volume.validate(failure.content)) {
        failure.kind = RetailActiveOverlayError::invalid_volume;
        return false;
    }
    // Native extracted-volume key for retail cdrom0:\\GAMER.OVL;1.
    auto file = volume.open_resource("GAMER.OVL", failure.content);
    if (!file) {
        failure.kind = RetailActiveOverlayError::missing_module;
        return false;
    }
    const auto length = file->length(failure.content);
    if (!length) {
        failure.kind = RetailActiveOverlayError::invalid_volume;
        return false;
    }
    if (*length != gamer_bytes) {
        failure.kind = RetailActiveOverlayError::unsupported_initializer;
        return false;
    }
    std::array<std::uint8_t, gamer_bytes> bytes{};
    for (std::size_t at = 0; at < bytes.size();) {
        const auto result = file->read(bytes.data() + at, bytes.size() - at,
                                       failure.content);
        if (result.error) {
            failure.kind = RetailActiveOverlayError::invalid_volume;
            return false;
        }
        if (!result.bytes) {
            failure.kind = RetailActiveOverlayError::invalid_module;
            return false;
        }
        at += result.bytes;
    }

    // Header, load-base-relative initializer table, and all three original
    // instruction words are authenticated before calling their native body.
    if (word(bytes, 0x00) != 0x336f574d || word(bytes, 0x04) != 2 ||
        word(bytes, 0x08) != gamer_base ||
        word(bytes, 0x18) != gamer_base + 0x110 ||
        word(bytes, 0x1c) != gamer_base + 0x114 ||
        word(bytes, 0x110) != gamer_initializer ||
        word(bytes, 0x100) != 0x24030001 ||
        word(bytes, 0x104) != 0x03e00008 ||
        word(bytes, 0x108) != 0xaf83b0b0) {
        failure.kind = RetailActiveOverlayError::unsupported_initializer;
        return false;
    }
    return true;
}

} // namespace

std::unique_ptr<NativeRetailActiveOverlay>
NativeRetailActiveOverlay::initialize_mode_one(
    const content::NativeContentVolume& volume,
    RetailActiveOverlayFailure& failure) noexcept {
    failure = {};
    try {
        auto result = std::unique_ptr<NativeRetailActiveOverlay>(
            new NativeRetailActiveOverlay);
        if (!result->reload_mode_one(volume, failure)) return {};
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailActiveOverlayError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailActiveOverlayError::host_capacity;
    } catch (...) {
        failure.kind = RetailActiveOverlayError::native_exception;
    }
    return {};
}

bool NativeRetailActiveOverlay::reload_mode_one(
    const content::NativeContentVolume& volume,
    RetailActiveOverlayFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_thread_) {
        failure.kind = RetailActiveOverlayError::wrong_thread;
        return false;
    }
    try {
        // 3DDA70 retires 64EC50 before it tests the selected module. The
        // native owner refuses a live child rather than discarding ownership.
        if (companion_parent_) {
            if (!companion_parent_->shutdown(failure.heap)) {
                failure.kind = RetailActiveOverlayError::heap_retirement;
                return false;
            }
            companion_parent_.reset();
            companion_identity_ = {};
        }
        if (selected_mode_ != 1) {
            if (!read_gamer_initializer(volume, failure)) return false;
            // Native body of GAMER initializer 7C4980: li v1,1; jr ra;
            // sw v1,64F1A0 in the delay slot. 3DDA70 stores the same mode.
            selected_mode_ = 1;
        }
        auto parent = memory::NativeHeapOwner::create(
            "Wave overlay heap", nullptr, failure.heap);
        if (!parent) {
            failure.kind = RetailActiveOverlayError::heap_creation;
            return false;
        }
        const auto observation = parent->observe();
        if (!observation.active) {
            failure.kind = RetailActiveOverlayError::inactive_parent;
            return false;
        }
        companion_identity_ = observation.identity;
        companion_parent_ = std::move(parent);
        return true;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailActiveOverlayError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailActiveOverlayError::host_capacity;
    } catch (...) {
        failure.kind = RetailActiveOverlayError::native_exception;
    }
    return false;
}

std::shared_ptr<memory::NativeHeapOwner>
NativeRetailActiveOverlay::retained_companion_parent(
    RetailActiveOverlayFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_thread_) {
        failure.kind = RetailActiveOverlayError::wrong_thread;
        return {};
    }
    try {
        if (!companion_parent_) {
            failure.kind = RetailActiveOverlayError::inactive_parent;
            return {};
        }
        const auto state = companion_parent_->observe();
        if (!state.active || state.identity != companion_identity_) {
            failure.kind = RetailActiveOverlayError::inactive_parent;
            return {};
        }
        return companion_parent_;
    } catch (...) {
        failure.kind = RetailActiveOverlayError::native_exception;
        return {};
    }
}

std::optional<RetailActiveOverlayObservation>
NativeRetailActiveOverlay::observe(
    RetailActiveOverlayFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_thread_) {
        failure.kind = RetailActiveOverlayError::wrong_thread;
        return {};
    }
    try {
        RetailActiveOverlayObservation result;
        result.selected_mode = selected_mode_;
        if (companion_parent_) {
            const auto state = companion_parent_->observe();
            result.active = state.active && state.identity == companion_identity_;
            if (result.active) result.companion_parent = companion_identity_;
        }
        return result;
    } catch (...) {
        failure.kind = RetailActiveOverlayError::native_exception;
        return {};
    }
}

} // namespace mka::retail
