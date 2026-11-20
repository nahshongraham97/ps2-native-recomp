#pragma once
#include "native_closure_codegen.hpp"
namespace recomp {
enum class NativeStartupStep {crt_prerequisites,pre_main_finalization,content_prefix,platform_initialization,boot_directory,main_continuation};
struct NativeStartupManifest {
    std::string original_elf_sha256;
    std::vector<NativeClosureWord> words;
    std::vector<NativeStartupStep> steps;
};
struct NativeStartupSources {std::string header,implementation,entry;};
NativeStartupManifest native_startup_manifest();
// Emits actual owned-state leaves and the source-ordered retail entry.
// Unimplemented CRT/platform/remainder functions are required link symbols;
// neither an interpreter nor a successful default provider is emitted.
std::optional<NativeStartupSources> emit_native_startup_closure(std::span<const std::uint8_t>,
    const NativeStartupManifest&,NativeClosureFailure&) noexcept;
} // namespace recomp
