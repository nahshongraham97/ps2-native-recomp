#pragma once
#include <array>
#include <cstdint>
namespace mka::game {
struct MaterialScalarWord {std::uint32_t bits;friend bool operator==(const MaterialScalarWord&,const MaterialScalarWord&)=default;};
struct MaterialVec3 {MaterialScalarWord x,y,z;friend bool operator==(const MaterialVec3&,const MaterialVec3&)=default;};
struct NativeMaterialMatrix {
 MaterialVec3 right,up,at,translation;
 std::uint32_t flags;
 std::array<std::uint32_t,3> opaque_padding;
 friend bool operator==(const NativeMaterialMatrix&,const NativeMaterialMatrix&)=default;
};
struct MaterialTransformPair {NativeMaterialMatrix first,second;friend bool operator==(const MaterialTransformPair&,const MaterialTransformPair&)=default;};
struct MaterialTranslationRates {MaterialScalarWord first_x,first_y,second_x,second_y;};
struct MaterialLayerSelection {bool first,second;friend bool operator==(const MaterialLayerSelection&,const MaterialLayerSelection&)=default;};
struct MaterialStepDelta {MaterialScalarWord value;};
// Source comparison treats exponent-zero words (including signed denormals) as zero.
MaterialLayerSelection select_material_layers(const MaterialTranslationRates&) noexcept;
// Business-scoped source-word program. No host FP arithmetic or hidden defaults.
MaterialTransformPair translate_material_layers(MaterialTransformPair,const MaterialTranslationRates&,MaterialStepDelta,MaterialLayerSelection) noexcept;
}
