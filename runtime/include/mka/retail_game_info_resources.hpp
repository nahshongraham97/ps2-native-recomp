#pragma once

#include "mka/retail_game_resource_registry.hpp"
#include "mka/retail_game_session.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace mka::retail {
enum class RetailGameInfoResourceError {
    none, missing_session, wrong_thread, missing_owner, allocation,
    registry, registry_partial, native_exception
};
struct RetailGameInfoResourceFailure {
    RetailGameInfoResourceError kind{RetailGameInfoResourceError::none};
    const char* operation{""};
    RetailGameSessionFailure session;
    RetailGameControlsFailure controls;
    RetailGameResourceRegistryFailure registry;
};

// The registered g_game_info value is a real persistent game owner, not a
// copied guest-layout buffer. Only source-proven fields have native accessors.
class NativeRetailGameInfoResource final {
public:
    NativeRetailGameInfoResource(const NativeRetailGameInfoResource&)=delete;
    NativeRetailGameInfoResource& operator=(const NativeRetailGameInfoResource&)=delete;
    bool clear_source_byte0_bit7_from_185ee0(
        RetailGameInfoResourceFailure&) noexcept;
    std::optional<bool> source_byte0_bit7(
        RetailGameInfoResourceFailure&) const noexcept;
    std::shared_ptr<NativeRetailGameSession> retained_session(
        RetailGameInfoResourceFailure&) const noexcept;
    std::shared_ptr<NativeRetailGameControls> retained_controls(
        RetailGameInfoResourceFailure&) const noexcept;
    NativeRetailInitialTaskState* initial_task_state(
        RetailGameInfoResourceFailure&) const noexcept;
private:
    friend class NativeRetailGameInfoResources;
    NativeRetailGameInfoResource(std::shared_ptr<NativeRetailGameSession>,
        std::shared_ptr<NativeRetailGameControls>) noexcept;
    bool access(RetailGameInfoResourceFailure&) const noexcept;
    std::shared_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailGameControls> controls_;
};

// These names come from source registrations. The arrays expose only bounded
// initialized words, with no inferred gameplay field meaning or object extent.
class NativeRetailHumanBoneTable final {
public:
    const std::array<std::uint32_t,17>& source_prefix_words() const noexcept {
        return words_;
    }
private:
    friend class NativeRetailGameInfoResources;
    NativeRetailHumanBoneTable() noexcept;
    std::array<std::uint32_t,17> words_;
};
class NativeRetailFlippedHumanBoneTable final {
public:
    std::uint32_t source_record_first_word() const noexcept { return first_word_; }
    const std::array<std::uint32_t,16>& referred_words() const noexcept {
        return referred_words_;
    }
private:
    friend class NativeRetailGameInfoResources;
    NativeRetailFlippedHumanBoneTable() noexcept;
    std::uint32_t first_word_{};
    // Original record word +4 refers to this separate static source array.
    std::array<std::uint32_t,16> referred_words_;
};
class NativeRetailNpcGroundCollisions final {
public:
    const std::array<std::uint32_t,16>& source_prefix_words() const noexcept {
        return words_;
    }
private:
    friend class NativeRetailGameInfoResources;
    NativeRetailNpcGroundCollisions() noexcept;
    std::array<std::uint32_t,16> words_;
};

// Retain this owner across the main sequence; the immutable source values have
// stable native identity and the mutable value keeps the actual GameSession.
class NativeRetailGameInfoResources final {
public:
    static std::unique_ptr<NativeRetailGameInfoResources> create(
        std::shared_ptr<NativeRetailGameSession>,
        RetailGameInfoResourceFailure&) noexcept;
    NativeRetailGameInfoResources(const NativeRetailGameInfoResources&)=delete;
    NativeRetailGameInfoResources& operator=(const NativeRetailGameInfoResources&)=delete;

    std::shared_ptr<NativeRetailGameInfoResource> game_info(
        RetailGameInfoResourceFailure&) const noexcept;
    // Call after the source 185EE0 -> 163A50 transition. The bit-7 clear is
    // earlier in 185EE0 and must be called separately at its exact point.
    bool publish_four_after_163a50(NativeRetailGameResourceRegistry&,
        RetailGameInfoResourceFailure&) noexcept;
    std::shared_ptr<NativeRetailHumanBoneTable> human_bones(
        RetailGameInfoResourceFailure&) const noexcept;
    std::shared_ptr<NativeRetailFlippedHumanBoneTable> flipped_human_bones(
        RetailGameInfoResourceFailure&) const noexcept;
    std::shared_ptr<NativeRetailNpcGroundCollisions> npc_ground_collisions(
        RetailGameInfoResourceFailure&) const noexcept;

private:
    NativeRetailGameInfoResources()=default;
    bool access(RetailGameInfoResourceFailure&) const noexcept;
    std::shared_ptr<NativeRetailGameInfoResource> game_info_;
    std::shared_ptr<NativeRetailHumanBoneTable> human_bones_;
    std::shared_ptr<NativeRetailFlippedHumanBoneTable> flipped_human_bones_;
    std::shared_ptr<NativeRetailNpcGroundCollisions> npc_ground_collisions_;
};
} // namespace mka::retail
