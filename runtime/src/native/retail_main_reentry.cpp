#include "mka/retail_main.hpp"
#include "retail_main_control.hpp"
namespace mka::retail {
[[noreturn]] void reenter_native_retail_main(NativeRetailMainState& main, std::uint32_t value) {
    throw detail::NativeMainReentry{&main, value == 0 ? 1 : value};
}
} // namespace mka::retail
