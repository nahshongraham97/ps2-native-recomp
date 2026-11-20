#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
// Offline source checking shared by typed retail emitters. Never linked into
// their generated runtime objects; source addresses are compiler inputs only.
bool verify_native_closure_source(std::span<const std::uint8_t> original_elf,
    std::string_view expected_sha256,std::span<const NativeClosureWord>,NativeClosureFailure&) noexcept;
// Specialized source recipe, not a runtime address registry or a general ABI lift.
NativeLogonManifest native_logon_manifest();
// Pure emission: the caller owns separate output paths and exclusive publication.
std::optional<NativeClosureSources> emit_native_logon_closure(std::span<const std::uint8_t> original_elf,
    const NativeLogonManifest&,NativeClosureFailure&) noexcept;
} // namespace recomp
