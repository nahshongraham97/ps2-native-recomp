#pragma once
#include "mka/material_translation.hpp"
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace mka::game {
using GeometryPair=std::array<MaterialScalarWord,2>;
using GeometryQuad=std::array<MaterialScalarWord,4>;
using GeometryPacked=std::array<std::uint8_t,4>;
struct GeometryStreamAttributes {
    std::vector<MaterialVec3> primary;
    std::optional<std::vector<MaterialVec3>> companion;
};
enum class GeometryExtraChannels {none,current_default,explicit_streams};
// Decoded native arrays. Indices/extents are host-sized; the serialized U16
// index representation is an ingress format, not a native resource capacity.
struct GeometryAttributeData {
    std::size_t vertex_count;
    std::vector<GeometryStreamAttributes> streams;
    std::optional<std::vector<GeometryPair>> first_pairs,second_pairs;
    std::optional<std::vector<GeometryPacked>> packed;
    GeometryExtraChannels extra_mode;
    std::vector<std::vector<GeometryPacked>> extra_channels;
    std::optional<std::vector<GeometryQuad>> quadruples;
};
enum class GeometryAttributeError {none,invalid_extent,invalid_extra_mode,missing_primary,
    invalid_index,allocation,host_capacity,invalid_program};
struct GeometryAttributeFailure {
    GeometryAttributeError kind=GeometryAttributeError::none;
    const char* operation="";
    std::size_t stream=0;
};
struct NativeGeometryVertex {
    std::size_t source_index;
    std::vector<MaterialVec3> primary,companion;
    GeometryPair first_pair,second_pair;
    GeometryPacked packed;
    std::vector<GeometryPacked> extra;
    GeometryQuad quadruple;
    std::uint32_t components_not_below_threshold;
};
class NativeGeometryAttributes final {
public:
    static std::shared_ptr<const NativeGeometryAttributes> create(
        GeometryAttributeData&&,GeometryAttributeFailure&) noexcept;
    // Original31C530 reads the current fallback BEFORE each referenced vertex.
    // Supply that observation explicitly; asset creation never freezes it.
    std::optional<NativeGeometryVertex> vertex(std::size_t,
        GeometryPacked actual_current_fallback,GeometryAttributeFailure&) const noexcept;
    std::size_t vertex_count() const noexcept{return data_.vertex_count;}
    std::size_t stream_count() const noexcept{return data_.streams.size();}
    NativeGeometryAttributes(const NativeGeometryAttributes&)=delete;
    NativeGeometryAttributes& operator=(const NativeGeometryAttributes&)=delete;
private:
    explicit NativeGeometryAttributes(GeometryAttributeData&& data):data_(std::move(data)){}
    GeometryAttributeData data_;
};

// Closed semantic payloads of source31C100 /31C270 /31C3D0. Platform command
// headers, scratch capacities, code addresses and microprogram state are gone.
// Topology/admission, pipeline construction and actual GPU draw remain separate
// required bodies; this operation does not claim they have executed.
enum class GeometryAttributeProgram {with_quad,pair_banks,first_pair};
struct GeometryStreamEmission {
    MaterialVec3 primary;
    std::array<std::uint8_t,3> companion;
};
struct NativeGeometryVertexEmission {
    GeometryStreamEmission first;
    std::array<MaterialScalarWord,4> pair_words;
    std::size_t pair_word_count;
    GeometryPacked packed;
    std::optional<GeometryQuad> quadruple;
    std::optional<GeometryStreamEmission> second;
};
std::optional<NativeGeometryVertexEmission> emit_geometry_vertex_attributes(
    const NativeGeometryVertex&,GeometryAttributeProgram,GeometryAttributeFailure&) noexcept;
} // namespace mka::game
