#include "mka/allocation_profile.hpp"

namespace mka::retail {
namespace {
using Role = memory::AllocationClassRole;
constexpr std::array<Role, 13> roles{Role::pfx, Role::mkobj, Role::mkbone, Role::mksobj, Role::mkproc,
    Role::size_64, Role::size_128, Role::size_256, Role::size_512, Role::size_2048, Role::size_4096,
    Role::tiny_stack, Role::big_stack};
constexpr std::array<std::size_t, 13> payloads{0x310, 0x120, 0x100, 0x90, 0x100,
    0x40, 0x80, 0x100, 0x200, 0x800, 0x1000, 0x200, 0x4000};
constexpr std::array<ClassCallbackPolicy, 13> callbacks{ClassCallbackPolicy::none, ClassCallbackPolicy::none,
    ClassCallbackPolicy::none, ClassCallbackPolicy::none, ClassCallbackPolicy::none,
    ClassCallbackPolicy::size_64, ClassCallbackPolicy::size_128, ClassCallbackPolicy::size_256,
    ClassCallbackPolicy::size_512, ClassCallbackPolicy::size_2048, ClassCallbackPolicy::size_4096,
    ClassCallbackPolicy::none, ClassCallbackPolicy::none};
constexpr std::array<std::uint64_t, 13> base{128, 120, 500, 600, 160, 256, 256, 256, 128, 16, 24, 135, 25};
constexpr std::array<std::uint64_t, 13> shared{142, 36, 400, 100, 160, 600, 800, 500, 128, 16, 30, 135, 25};
constexpr std::array<std::uint64_t, 13> mode_two{208, 135, 675, 600, 160, 256, 256, 276, 128, 2, 24, 135, 25};
constexpr std::array<std::uint64_t, 13> mode_five{128, 16, 500, 16, 160, 200, 400, 500, 128, 16, 32, 135, 25};
constexpr std::array<std::uint64_t, 13> mode_ten{130, 256, 512, 512, 150, 512, 256, 300, 250, 90, 4, 30, 20};
constexpr std::array<std::uint64_t, 13> mode_eleven{250, 300, 895, 670, 160, 512, 490, 1150, 250, 45, 5, 138, 22};
constexpr std::array<std::uint64_t, 13> mode_twelve{82, 36, 400, 80, 160, 600, 800, 600, 128, 16, 10, 135, 25};
}
const NativeClassProfile* NativeClassProfilePlan::find(Role role) const & noexcept {
    for (const auto& item : classes) if (item.role == role) return &item;
    return nullptr;
}
NativeClassProfilePlan select_native_class_profile(std::uint32_t selector) noexcept {
    const auto* counts = &base;
    switch (selector) {
    case 1: case 3: case 8: case 9: counts = &shared; break;
    case 2: counts = &mode_two; break;
    case 5: counts = &mode_five; break;
    case 10: counts = &mode_ten; break;
    case 11: counts = &mode_eleven; break;
    case 12: counts = &mode_twelve; break;
    default: break;
    }
    NativeClassProfilePlan result{selector, selector < 13 ? selector : 13, {}};
    for (std::size_t i = 0; i < roles.size(); ++i)
        result.classes[i] = {roles[i], (*counts)[i], payloads[i], 16, callbacks[i]};
    return result;
}
} // namespace mka::retail
