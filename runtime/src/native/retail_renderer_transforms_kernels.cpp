#include "mka/retail_renderer_transforms.hpp"
#include <algorithm>

namespace mka::retail::renderer_required {

void transform_point_exact(RetailRendererPoint3& out, const RetailRendererPoint3& in, const RetailRendererAffine& m) {
    const float x = in.x * m.right.x + in.y * m.up.x + in.z * m.at.x + m.translation.x;
    const float y = in.x * m.right.y + in.y * m.up.y + in.z * m.at.y + m.translation.y;
    const float z = in.x * m.right.z + in.y * m.up.z + in.z * m.at.z + m.translation.z;
    out = {x, y, z};
}

void transform_points_exact(std::span<RetailRendererPoint3> out, std::span<const RetailRendererPoint3> in, const RetailRendererAffine& m) {
    const std::size_t n = std::min(out.size(), in.size());
    for (std::size_t i = 0; i < n; ++i) {
        transform_point_exact(out[i], in[i], m);
    }
}

void transform_vector_exact(RetailRendererPoint3& out, const RetailRendererPoint3& in, const RetailRendererAffine& m) {
    const float x = in.x * m.right.x + in.y * m.up.x + in.z * m.at.x;
    const float y = in.x * m.right.y + in.y * m.up.y + in.z * m.at.y;
    const float z = in.x * m.right.z + in.y * m.up.z + in.z * m.at.z;
    out = {x, y, z};
}

void transform_vectors_exact(std::span<RetailRendererPoint3> out, std::span<const RetailRendererPoint3> in, const RetailRendererAffine& m) {
    const std::size_t n = std::min(out.size(), in.size());
    for (std::size_t i = 0; i < n; ++i) {
        transform_vector_exact(out[i], in[i], m);
    }
}

} // namespace mka::retail::renderer_required
