#pragma once
#include "mka/retail_game_session.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include <cstdint>
#include <optional>
namespace mka::retail {
struct NativeRetailMainState;
enum class RetailSoundMetadataError {
    none, invalid_semantic_id, invalid_metadata, invalid_gain_index,
    main_lifetime, missing_session, game_session, missing_catalog,
    catalog, bank_not_present, bank_session, bank_not_live
};
struct RetailSoundMetadataFailure {
    RetailSoundMetadataError kind{RetailSoundMetadataError::none};
    const char* operation{""};
    RetailGameSessionFailure game;
    RetailSoundCatalogFailure catalog;
    sound::AudioBanksFailure bank;
};
// Original game fields only. This is not a completed12C440 request or playback.
struct RetailSemanticSoundMetadata {
    std::uint32_t semantic_id{};
    std::uint16_t bank_index{},sound_index{},selector{};
    std::uint8_t percent{},category{},gain_index{},priority{};
    std::int8_t output{};
    std::uint32_t category_scalar_bits{};
};
// The operands retain exact game binary32 bits. No arithmetic/quantization,
// request clamp, envelope, output-device mapping or flags policy is performed.
struct RetailSemanticSoundOperands {
    RetailSemanticSoundMetadata metadata;
    sound::AudioBankHandle bank;
    std::uint32_t current_gain_bits{};
};
std::optional<RetailSemanticSoundMetadata> decode_retail_semantic_sound_metadata(
    std::uint32_t semantic_id,RetailSoundMetadataFailure&) noexcept;
// Reads the existing Session preferences,including sixth raw word_14 alias.
std::optional<std::uint32_t> read_retail_sound_gain_operand_bits(
    NativeRetailGameSession&,std::uint8_t index,RetailSoundMetadataFailure&) noexcept;
// Borrow Main only during this call. The copied bank handle is the actual
// catalog callback result; its subsequent admission still requires live Session.
std::optional<RetailSemanticSoundOperands> resolve_retail_semantic_sound_operands(
    NativeRetailMainState&,std::uint32_t semantic_id,RetailSoundMetadataFailure&);
}
