#pragma once
#include "mka/material_translation.hpp"
#include <cstdint>
namespace mka::game {
// Source companion-byte encoding: ordered127 multiply, truncating signed-word
// conversion, then low8. All input words admitted; not a unit-range clamp.
std::uint8_t quantize_geometry_companion_component(MaterialScalarWord) noexcept;
}
