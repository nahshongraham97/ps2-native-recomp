#pragma once
#include "mka/sound_archive.hpp"
#include "mka/adpcm_codec.hpp"
namespace mka::sound {
enum class BankAssetError { none, no_backing, malformed_bank, absent_selector, absent_program,
    command_out_of_bounds, unsupported_command_kind, unsupported_named_command_route,
    absent_resource, payload_failed, unsupported_plane_count, decode_failed, allocation_failed };
struct BankAssetFailure {
    BankAssetError kind = BankAssetError::none;
    BankError bank = BankError::none;
    PayloadFailure payload;
    AdpcmError codec = AdpcmError::none;
};
struct BankSample { CommandRecord command; ResourceLookup resource; SamplePayload payload; };
struct BankMono { BankSample source; AdpcmMono pcm; AdpcmHistory final_history; };
// Owns exact parsed bank data and retains the actual opened archive backing.
// Offline preparation only: no playback, loop, gain, callback or unload scheduler.
class BankAsset {
public:
    BankAsset(const BankAsset&) = default;
    BankAsset(BankAsset&&) noexcept = default;
    BankAsset& operator=(BankAsset other) noexcept {
        using std::swap;
        swap(origin_,other.origin_); swap(bank_,other.bank_); backing_.swap(other.backing_);
        return *this;
    }
    static std::optional<BankAsset> parse(BankAssetSource, BankAssetFailure&);
    const ArchiveRecord& origin() const noexcept { return origin_; }
    const SoundBank& bank() const noexcept { return bank_; }
    std::optional<BankSample> read_sample(std::int32_t selector, std::size_t command_index, BankAssetFailure&) const;
    // Atomic caller history: any lookup/read/count/decode failure leaves it unchanged.
    std::optional<BankMono> decode_mono(std::int32_t selector, std::size_t command_index, AdpcmHistory&, BankAssetFailure&) const;
private:
    BankAsset(ArchiveRecord origin, SoundBank bank, std::shared_ptr<SoundArchiveBacking> backing)
        : origin_(std::move(origin)), bank_(std::move(bank)), backing_(std::move(backing)) {}
    ArchiveRecord origin_;
    SoundBank bank_;
    std::shared_ptr<SoundArchiveBacking> backing_;
};
}
