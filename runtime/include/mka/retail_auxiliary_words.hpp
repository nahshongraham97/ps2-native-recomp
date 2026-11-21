#pragma once
#include <cstdint>

namespace mka::retail {
// Two game-owned values reset by 26F1E0. The second has a separate producer
// at 26ED50. This is not PRNG state, a guest address map or a device register.
struct NativeRetailAuxiliaryWords {
    std::uint32_t first{};
    std::uint32_t second{};
};
} // namespace mka::retail
