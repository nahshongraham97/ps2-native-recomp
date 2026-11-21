#include "mka/retail_main.hpp"

namespace mka::retail {
NativeRetailMainState::NativeRetailMainState(NativeRetailStartupState& owner,
    input::ControllerInput& samples) noexcept
    : startup(owner), controller_samples(samples) {}
} // namespace mka::retail
