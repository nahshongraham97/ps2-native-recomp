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
struct NativeClassProfilePlan {
    std::uint32_t requested_selector, selected_profile;
    std::array<NativeClassProfile, 13> classes;
    const NativeClassProfile* find(memory::AllocationClassRole) const & noexcept;
    const NativeClassProfile* find(memory::AllocationClassRole) const && = delete;
};
// Exact class descriptor selection only. Counts preserve game predicates, never
// native allocation ceilings. This does not reset/create owners or run callbacks.
// The separate record producer, other tuning fields and unwritten source word
// are outside this leaf; no implicit zero or completed main phase is supplied.
NativeClassProfilePlan select_native_class_profile(std::uint32_t selector) noexcept;
} // namespace mka::retail
