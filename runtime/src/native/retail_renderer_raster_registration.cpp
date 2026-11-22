#include "mka/retail_renderer_core.hpp"

namespace mka::retail::renderer_required {
// Keep the existing integration seam; adopt Xbox2C84D0's verified metadata
// operation, not PS2 device layouts/stream hooks or a console extension offset.
std::optional<bool> register_object_extensions_1b72b0(
 NativeRetailRendererCore& core,RetailRendererCoreFailure& failure){
 return core.register_native_raster_extension(failure);
}
void discard_object_extensions_after_core_init(NativeRetailRendererCore& core)noexcept{
 core.discard_native_raster_extension();
}
}
