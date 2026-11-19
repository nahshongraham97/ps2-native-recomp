#include "mka/bank_asset.hpp"
#include <new>
#include <stdexcept>
namespace mka::sound {
std::optional<BankAsset> BankAsset::parse(BankAssetSource source, BankAssetFailure& failure) {
    failure = {};
    if (!source.backing) { failure.kind = BankAssetError::no_backing; return std::nullopt; }
    try {
        auto bank = SoundBank::parse(std::move(source.bytes), failure.bank);
        if (!bank) { failure.kind = BankAssetError::malformed_bank; return std::nullopt; }
        return BankAsset(std::move(source.origin), std::move(*bank), std::move(source.backing));
    } catch (const std::bad_alloc&) { failure.kind = BankAssetError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = BankAssetError::allocation_failed; }
    return std::nullopt;
}
std::optional<BankSample> BankAsset::read_sample(std::int32_t selector, std::size_t index, BankAssetFailure& failure) const {
    failure = {};
    if (!backing_) { failure.kind = BankAssetError::no_backing; return std::nullopt; }
    try {
        const auto commands = bank_.lookup_commands(selector);
        if (commands.status != LookupStatus::found) {
            failure.kind = commands.status == LookupStatus::absent_selector ? BankAssetError::absent_selector :
                commands.status == LookupStatus::absent_program ? BankAssetError::absent_program : BankAssetError::malformed_bank;
            return std::nullopt;
        }
        if (index >= commands.commands.size()) { failure.kind = BankAssetError::command_out_of_bounds; return std::nullopt; }
        const auto command = commands.commands[index];
        if (command.kind() != 1) { failure.kind = BankAssetError::unsupported_command_kind; return std::nullopt; }
        // Bank flag0x10 selects numeric resource keys. Named command relocation
        // requires a distinct string path; do not reinterpret raw offsets as keys.
        if (!(bank_.raw_bytes()[4] & 0x10)) { failure.kind = BankAssetError::unsupported_named_command_route; return std::nullopt; }
        auto key = command.resource_bits();
        // Original MSL2A5D8..2A628 rewrites serialized -1 resource to0.
        // Preserve raw command bytes; exact lookup0 may be absent, never default.
        if (key == 0xffffffffu) key = 0;
        auto resource = bank_.lookup_resource(key);
        if (resource.status != ResourceLookupStatus::found) { failure.kind = BankAssetError::absent_resource; return std::nullopt; }
        auto payload = backing_->read_payload(resource, failure.payload);
        if (!payload) { failure.kind = BankAssetError::payload_failed; return std::nullopt; }
        return BankSample{command, resource, std::move(*payload)};
    } catch (const std::bad_alloc&) { failure.kind = BankAssetError::allocation_failed; }
      catch (const std::length_error&) { failure.kind = BankAssetError::allocation_failed; }
    return std::nullopt;
}
std::optional<BankMono> BankAsset::decode_mono(std::int32_t selector, std::size_t index, AdpcmHistory& history, BankAssetFailure& failure) const {
    auto source = read_sample(selector, index, failure);
    if (!source) return std::nullopt;
    if (source->payload.plane_count() != 1) { failure.kind = BankAssetError::unsupported_plane_count; return std::nullopt; }
    auto next = history;
    const auto raw = source->payload.encoded_payload();
    auto pcm = decode_adpcm_mono(raw.data, raw.size, next, failure.codec);
    if (!pcm) { failure.kind = BankAssetError::decode_failed; return std::nullopt; }
    BankMono result{std::move(*source), std::move(*pcm), next};
    history = next;
    return result;
}
}
