#pragma once

#include "mka/allocation_class.hpp"
#include <array>
#include <cstdint>

namespace mka::retail {
enum class ClassCallbackPolicy { none, size_64, size_128, size_256, size_512, size_2048, size_4096 };
struct NativeClassProfile {
    memory::AllocationClassRole role;
    std::uint64_t configured_game_count;
    std::size_t payload_bytes, alignment;
    ClassCallbackPolicy callback;
};
// Game configuration counts, never native storage ceilings. The companion
// class roles are populated later by the separate 311260 source phase.
struct NativeProfileObjectCounts {
    std::uint64_t game_pool_records{};
    std::uint64_t npc_stacks{}, player_collision_objects{}, attack_collision_objects{};
    bool operator==(const NativeProfileObjectCounts&) const = default;
};
struct NativeClassProfilePlan {
    std::uint32_t requested_selector, selected_profile;
    std::array<NativeClassProfile, 13> classes;
    NativeProfileObjectCounts object_counts;
    const NativeClassProfile* find(memory::AllocationClassRole) const & noexcept;
    const NativeClassProfile* find(memory::AllocationClassRole) const && = delete;
};
// Exact class descriptor selection only. Counts preserve game predicates, never
// native allocation ceilings. This does not reset/create owners or run callbacks.
// The actual game-pool and companion-class configured counts are included.
// Source word7 is unwritten and has no invented native field/default here.
// Pool construction, all class lifecycles/callbacks and main remain separate.
NativeClassProfilePlan select_native_class_profile(std::uint32_t selector) noexcept;
} // namespace mka::retail
