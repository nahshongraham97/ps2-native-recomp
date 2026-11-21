#pragma once
#include <cstdint>
namespace mka::retail {
struct NativeRetailMainState;
namespace detail {
struct NativeMainReentry { NativeRetailMainState* owner; std::uint32_t value; };
}
} // namespace mka::retail
