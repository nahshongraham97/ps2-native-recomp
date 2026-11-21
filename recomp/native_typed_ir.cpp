#include "native_typed_ir.hpp"

#include <utility>

namespace recomp {

NativeTypedManifest retail_scalar_mode_manifest() {
    NativeFieldContract scalar{0x0064dea4u,"mka::retail::NativeRetailGameSession",
        "scalar_mode","store_scalar_mode_from_source","mka::retail::RetailGameSessionFailure",
        NativeValueType::signed_word};
    NativeFunctionContract get{"retail_scalar_mode_get_185cb0",0x00185cb0u,
        {{0x00185cb0u,0x03e00008u},{0x00185cb4u,0x8f829db4u}},0x006540f0u,
        NativeReturnType::signed_word,{}, {scalar}};
    NativeFunctionContract set{"retail_scalar_mode_set_185cc0",0x00185cc0u,
        {{0x00185cc0u,0x03e00008u},{0x00185cc4u,0xaf849db4u}},0x006540f0u,
        NativeReturnType::source_void,{NativeValueType::signed_word},{scalar}};
    return {"a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
        {std::move(get),std::move(set)}};
}

} // namespace recomp
