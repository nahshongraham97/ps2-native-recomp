#include "native_typed_retail.hpp"
namespace mka::retail::generated {
std::optional<std::int32_t> retail_scalar_mode_get_185cb0(mka::retail::NativeRetailGameSession& session, mka::retail::RetailGameSessionFailure& failure) {
    const auto read1 = session.scalar_mode(failure);
    if (!read1) return {};
    const std::int32_t v1 = *read1;
    return v1;
}
bool retail_scalar_mode_set_185cc0(mka::retail::NativeRetailGameSession& session, std::int32_t arg0, mka::retail::RetailGameSessionFailure& failure) {
    const std::int32_t v1 = arg0;
    if (!session.store_scalar_mode_from_source(v1, failure)) return false;
    return true;
}
} // namespace mka::retail::generated
