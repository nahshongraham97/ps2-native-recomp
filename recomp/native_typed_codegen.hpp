#pragma once

#include "native_typed_ir.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace recomp {

struct NativeTypedSources {
    std::string header;
    std::string implementation;
    std::vector<NativeFunctionIr> lowered;
};

// Offline only. No original ELF bytes, source addresses, register ABI or
// instruction dispatcher appear in the generated native object.
std::optional<NativeTypedSources> emit_native_typed_functions(
    std::span<const std::uint8_t> original_elf,
    const NativeTypedManifest& manifest, NativeTypedFailure& failure) noexcept;

} // namespace recomp
