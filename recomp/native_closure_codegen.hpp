#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace recomp {
struct NativeClosureWord { std::uint32_t pc,word; bool operator==(const NativeClosureWord&) const=default; };
enum class NativeLogonEffect { prepare_game_world, surround_movie, finish_surround_movie, suspend_game_inputs_after_movie, transition_task };
struct NativeLogonManifest {
    std::string original_elf_sha256;
    std::vector<NativeClosureWord> words;
    std::vector<NativeLogonEffect> effects;
};
enum class NativeClosureError { none,source_identity,source_format,source_word,recipe,allocation };
struct NativeClosureFailure { NativeClosureError kind=NativeClosureError::none; const char* operation=""; };
struct NativeClosureSources { std::string header,implementation; };
// Specialized source recipe, not a runtime address registry or a general ABI lift.
NativeLogonManifest native_logon_manifest();
// Pure emission: the caller owns separate output paths and exclusive publication.
std::optional<NativeClosureSources> emit_native_logon_closure(std::span<const std::uint8_t> original_elf,
    const NativeLogonManifest&,NativeClosureFailure&) noexcept;
} // namespace recomp
