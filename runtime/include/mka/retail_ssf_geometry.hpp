#pragma once
#include "mka/retail_renderer_frames.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mka::retail {
namespace ssf_geometry_detail {
// Host-sized serialized extent arithmetic, independently testable without a
// multi-gigabyte allocation. A 32-bit source length must not wrap at length+12.
constexpr std::optional<std::size_t> bounded_chunk_end(std::size_t begin,
    std::uint32_t length, std::size_t parent_end) noexcept {
    if(begin>parent_end||parent_end-begin<12||std::size_t(length)>parent_end-begin-12)return {};
    return begin+12+std::size_t(length);
}
}
enum class RetailSsfGeometryError {
    none, range, chunk, version, selection, frame, geometry, material,
    native_geometry_required, unsupported_plugin, topology, allocation, native_exception,
    dma_chain, vif_bounds, unsupported_vif_opcode, unsupported_unpack_format,
    skinned_native_pipe_unsupported
};
struct RetailSsfGeometryFailure {
    RetailSsfGeometryError kind{RetailSsfGeometryError::none};
    const char* operation{""};
    std::size_t offset{};
};
struct RetailSsfGeometrySelection {
    std::size_t atomic_index{}, morph_index{};
    // Serialized section coordinate, never a host pointer or inferred PAK base.
    std::uint64_t clump_offset_in_section{};
};
struct RetailSsfGeometryFrame {
    RetailRendererAffine modelling{};
    std::int32_t parent{-1};
    std::uint32_t flags{};
};
struct RetailSsfGeometryVertex {
    RetailRendererPoint3 position{}, normal{};
    std::array<float, 2> uv{};
    std::array<std::uint8_t, 4> stored_color{};
};
struct RetailSsfGeometryTriangle {
    std::array<std::size_t, 3> vertices{};
    std::size_t material{};
};
struct RetailSsfGeometryMaterial {
    std::array<std::uint8_t, 4> stored_color{};
    std::array<float, 3> surface{};
    std::uint32_t flags{}, filter_addressing{};
    std::uint32_t texture_mip_metadata{};
    bool textured{};
    std::string texture_name, mask_name;
    // Authenticated 2136D0 version-5 serialization. Retained without assigning
    // an unproved blend/effect interpretation to these source fields.
    std::array<std::uint32_t, 7> plugin_words{};
};
struct RetailSsfGeometryData {
    std::size_t atomic_index{}, geometry_index{}, frame_index{}, morph_index{}, morph_count{};
    std::uint32_t geometry_flags{}, atomic_flags{};
    std::array<std::uint32_t, 4> atomic_plugin_words{};
    std::vector<RetailSsfGeometryFrame> frames;
    std::vector<RetailSsfGeometryVertex> vertices;
    std::vector<RetailSsfGeometryTriangle> triangles;
    std::vector<RetailSsfGeometryMaterial> materials;
    RetailRendererPoint3 bound_center{};
    float bound_radius{};
    bool has_colors{}, has_uv{}, has_normals{};
    bool is_native_ps2_vif{};
    std::size_t adc_strip_breaks{};
};
// CPU decoding of explicitly selected authentic non-native RW geometry inside
// a complete SSF clump. No RW wrapper manufacture, GPU work, animation producer,
// material effect policy, color normalization or retail draw equivalence.
std::optional<RetailSsfGeometryData> decode_retail_ssf_geometry(
    std::span<const std::byte>, RetailSsfGeometrySelection, RetailSsfGeometryFailure&) noexcept;
} // namespace mka::retail
