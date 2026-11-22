#include "mka/retail_sound_metadata.hpp"
#include "mka/retail_sound_catalog_data.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <bit>
namespace mka::retail {
namespace {
#include "retail_sound_metadata_data.inc"
}
std::optional<RetailSemanticSoundMetadata> decode_retail_semantic_sound_metadata(
    std::uint32_t id,RetailSoundMetadataFailure& f) noexcept {
    f={};
    if(id==0xffffffffu) {
        f.kind=RetailSoundMetadataError::invalid_semantic_id;
        f.operation="12C440 all-ones semantic identifier";return {};
    }
    const auto high=id>>16,low=id&0xffffu;
    const auto bank=high<sound_catalog_data::banks.size()?high:0u;
    const auto index=low<sound_words.size()?low:0u;
    const auto packed=sound_words[index];
    const auto category=packed>>25;
    const auto percent=(packed>>13)&127u;
    if(category>=categories.size() || percent>100) {
        f.kind=RetailSoundMetadataError::invalid_metadata;
        f.operation="12C440 category/percentage admission";return {};
    }
    const auto& row=categories[category];
    if(row.gain_index>=6) {
        f.kind=RetailSoundMetadataError::invalid_gain_index;
        f.operation="12C440 category live-gain index";return {};
    }
    const auto output_bits=(packed>>20)&7u;
    const auto output=output_bits>=4?static_cast<int>(output_bits)-8:static_cast<int>(output_bits);
    return RetailSemanticSoundMetadata{id,static_cast<std::uint16_t>(bank),
        static_cast<std::uint16_t>(index),static_cast<std::uint16_t>(packed&1023u),
        static_cast<std::uint8_t>(percent),static_cast<std::uint8_t>(category),
        row.gain_index,static_cast<std::uint8_t>((packed>>23)&3u),
        static_cast<std::int8_t>(output),row.scalar_bits};
}
std::optional<std::uint32_t> read_retail_sound_gain_operand_bits(
    NativeRetailGameSession& session,std::uint8_t index,RetailSoundMetadataFailure& f) noexcept {
    f={};
    if(index>=6) {f.kind=RetailSoundMetadataError::invalid_gain_index;
        f.operation="12C440 six live preference gain operands";return {};}
    const auto* words=session.game_records_values(f.game);
    if(!words) {f.kind=RetailSoundMetadataError::game_session;
        f.operation="12C440 retained Session preference owner";return {};}
    if(index==5) return words->profile_preferences.word_14;
    return std::bit_cast<std::uint32_t>(words->profile_preferences.levels[index]);
}
std::optional<RetailSemanticSoundOperands> resolve_retail_semantic_sound_operands(
    NativeRetailMainState& main,std::uint32_t id,RetailSoundMetadataFailure& f) {
    f={};
    if(!main.lifetime || !main.lifetime->active_on_owner_thread()) {
        f.kind=RetailSoundMetadataError::main_lifetime;
        f.operation="sound metadata Main lifetime/thread";return {};
    }
    const auto session=main.startup.game_session;
    if(!session) {f.kind=RetailSoundMetadataError::missing_session;
        f.operation="sound metadata actual game Session missing";return {};}
    if(!session->validate_current_thread(f.game)) {
        f.kind=RetailSoundMetadataError::game_session;
        f.operation="sound metadata actual game Session thread";return {};
    }
    const auto metadata=decode_retail_semantic_sound_metadata(id,f);
    if(!metadata) return {};
    const auto catalog=main.sound_bank_catalog;
    if(!catalog) {f.kind=RetailSoundMetadataError::missing_catalog;
        f.operation="sound metadata actual catalog missing";return {};}
    const auto row=catalog->observe(metadata->bank_index,f.catalog);
    if(!row) {f.kind=RetailSoundMetadataError::catalog;
        f.operation="sound metadata actual catalog observation";return {};}
    if(!row->bank) {f.kind=RetailSoundMetadataError::bank_not_present;
        f.operation="12C440 selected bank has no published native handle";return {};}
    auto native=catalog->share_bank_session(f.catalog);
    if(!native) {f.kind=RetailSoundMetadataError::bank_session;
        f.operation="sound metadata existing catalog Session unavailable";return {};}
    if(!native->load_state(*row->bank,f.bank)) {
        f.kind=RetailSoundMetadataError::bank_not_live;
        f.operation="sound metadata published native bank is no longer live";return {};
    }
    const auto gain=read_retail_sound_gain_operand_bits(*session,metadata->gain_index,f);
    if(!gain) return {};
    return RetailSemanticSoundOperands{*metadata,*row->bank,*gain};
}
}
