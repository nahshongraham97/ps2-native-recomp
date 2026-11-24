#include "mka/retail_renderer_matrix.hpp"
#include <algorithm>
#include <cmath>

namespace mka::retail::renderer_required {

bool multiply_affines_19f680(RetailRendererAffine& out, const RetailRendererAffine& left,
    const RetailRendererAffine& right, RetailRendererMatrixFailure& f) {
    f = {};
    // Option C (Partitioned): Native host IEEE 754 float math matching Xbox 0x002be460 (RwMatrixMultiply)
    const float r_x = left.right.x * right.right.x + left.right.y * right.up.x + left.right.z * right.at.x;
    const float r_y = left.right.x * right.right.y + left.right.y * right.up.y + left.right.z * right.at.y;
    const float r_z = left.right.x * right.right.z + left.right.y * right.up.z + left.right.z * right.at.z;

    const float u_x = left.up.x * right.right.x + left.up.y * right.up.x + left.up.z * right.at.x;
    const float u_y = left.up.x * right.right.y + left.up.y * right.up.y + left.up.z * right.at.y;
    const float u_z = left.up.x * right.right.z + left.up.y * right.up.z + left.up.z * right.at.z;

    const float a_x = left.at.x * right.right.x + left.at.y * right.up.x + left.at.z * right.at.x;
    const float a_y = left.at.x * right.right.y + left.at.y * right.up.y + left.at.z * right.at.y;
    const float a_z = left.at.x * right.right.z + left.at.y * right.up.z + left.at.z * right.at.z;

    const float t_x = left.translation.x * right.right.x + left.translation.y * right.up.x + left.translation.z * right.at.x + right.translation.x;
    const float t_y = left.translation.x * right.right.y + left.translation.y * right.up.y + left.translation.z * right.at.y + right.translation.y;
    const float t_z = left.translation.x * right.right.z + left.translation.y * right.up.z + left.translation.z * right.at.z + right.translation.z;

    out.right = {r_x, r_y, r_z};
    out.up = {u_x, u_y, u_z};
    out.at = {a_x, a_y, a_z};
    out.translation = {t_x, t_y, t_z};
    return true;
}

bool classify_matrix_1a0240(RetailRendererMatrixValue& matrix, const RetailRendererMatrixTolerances& t, RetailRendererMatrixFailure& f) {
    f = {};
    // Option C: Native host math matching Xbox 0x002be150 tolerance classification
    const auto& m = matrix.affine;
    const float len_r2 = m.right.x * m.right.x + m.right.y * m.right.y + m.right.z * m.right.z;
    const float len_u2 = m.up.x * m.up.x + m.up.y * m.up.y + m.up.z * m.up.z;
    const float len_a2 = m.at.x * m.at.x + m.at.y * m.at.y + m.at.z * m.at.z;

    const float dot_ru = m.right.x * m.up.x + m.right.y * m.up.y + m.right.z * m.up.z;
    const float dot_ra = m.right.x * m.at.x + m.right.y * m.at.y + m.right.z * m.at.z;
    const float dot_ua = m.up.x * m.at.x + m.up.y * m.at.y + m.up.z * m.at.z;

    const float basis_err = std::max({std::abs(len_r2 - 1.0f), std::abs(len_u2 - 1.0f), std::abs(len_a2 - 1.0f)});
    const float ortho_err = std::max({std::abs(dot_ru), std::abs(dot_ra), std::abs(dot_ua)});

    std::uint32_t flags = 0;
    if (basis_err <= t.basis_length_error_squared && ortho_err <= t.orthogonality_error_squared) {
        flags |= 0x01; // Orthonormal
    }

    const float id_err = (m.right.x - 1.0f)*(m.right.x - 1.0f) + m.right.y*m.right.y + m.right.z*m.right.z
                       + m.up.x*m.up.x + (m.up.y - 1.0f)*(m.up.y - 1.0f) + m.up.z*m.up.z
                       + m.at.x*m.at.x + m.at.y*m.at.y + (m.at.z - 1.0f)*(m.at.z - 1.0f)
                       + m.translation.x*m.translation.x + m.translation.y*m.translation.y + m.translation.z*m.translation.z;
    if (id_err <= t.identity_error_squared) {
        flags |= NativeRetailRendererMatrixDomain::identity_shortcut_mask;
    }

    matrix.properties = flags;
    return true;
}

bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue& out, const RetailRendererMatrixValue& in, RetailRendererMatrixFailure& f) {
    f = {};
    // Option C: Native host math matching Xbox 0x002be560 (RwMatrixInvert)
    // Thresholds (|det| < 1e-12f and 1e-6f inversion tolerance) are native host
    // IEEE 754 stability design choices, documented as native design choices.
    const auto& m = in.affine;

    const float det = m.right.x * (m.up.y * m.at.z - m.up.z * m.at.y)
                    - m.right.y * (m.up.x * m.at.z - m.up.z * m.at.x)
                    + m.right.z * (m.up.x * m.at.y - m.up.y * m.at.x);

    if (std::abs(det) < 1e-12f) {
        f.kind = RetailRendererMatrixError::numeric_policy_unbound;
        f.operation = "singular matrix cannot be inverted (det near zero: native host design threshold)";
        return false;
    }

    const float inv_det = 1.0f / det;

    RetailRendererAffine inv;
    inv.right.x =  (m.up.y * m.at.z - m.up.z * m.at.y) * inv_det;
    inv.right.y = -(m.right.y * m.at.z - m.right.z * m.at.y) * inv_det;
    inv.right.z =  (m.right.y * m.up.z - m.right.z * m.up.y) * inv_det;

    inv.up.x    = -(m.up.x * m.at.z - m.up.z * m.at.x) * inv_det;
    inv.up.y    =  (m.right.x * m.at.z - m.right.z * m.at.x) * inv_det;
    inv.up.z    = -(m.right.x * m.up.z - m.right.z * m.up.x) * inv_det;

    inv.at.x    =  (m.up.x * m.at.y - m.up.y * m.at.x) * inv_det;
    inv.at.y    = -(m.right.x * m.at.y - m.right.y * m.at.x) * inv_det;
    inv.at.z    =  (m.right.x * m.up.y - m.right.y * m.up.x) * inv_det;

    inv.translation.x = -(m.translation.x * inv.right.x + m.translation.y * inv.up.x + m.translation.z * inv.at.x);
    inv.translation.y = -(m.translation.x * inv.right.y + m.translation.y * inv.up.y + m.translation.z * inv.at.y);
    inv.translation.z = -(m.translation.x * inv.right.z + m.translation.y * inv.up.z + m.translation.z * inv.at.z);

    out.affine = inv;
    out.properties = in.properties & ~NativeRetailRendererMatrixDomain::identity_shortcut_mask;
    return true;
}

} // namespace mka::retail::renderer_required
