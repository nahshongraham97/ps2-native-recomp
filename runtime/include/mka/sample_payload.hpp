#pragma once
#include "mka/native_file.hpp"
#include "mka/sound_bank.hpp"
#include <array>
#include <optional>
#include <utility>

namespace mka::sound {
enum class PayloadError {
    none, resource_not_found, unsupported_key_mode, unsupported_alternate_streaming,
    unsupported_key_lookup_route, extent_out_of_bounds, truncated_header,
    unsupported_count_encoding, unsupported_zero_count, unsupported_zero_rate,
    unsupported_rate_mismatch, unsupported_zero_plane_size, inconsistent_extent,
    host_capacity, allocation_failed, io_error, cursor_restore_failed
};
struct PayloadFailure { PayloadError kind = PayloadError::none; std::error_code host_error; };
struct PayloadBytes { const std::uint8_t* data = nullptr; std::size_t size = 0; };
// Owns serialized bytes only. No codec, channel order, Hz or playback meaning
// is inferred. Views last until this owner is destroyed, assigned or moved.
class SamplePayload {
public:
    SamplePayload(const SamplePayload&) = default;
    SamplePayload(SamplePayload&& other) noexcept { bytes_.swap(other.bytes_); }
    SamplePayload& operator=(SamplePayload other) noexcept { bytes_.swap(other.bytes_); return *this; }
    const std::vector<std::uint8_t>& raw_bytes() const noexcept { return bytes_; }
    std::array<std::uint8_t,20> header() const noexcept;
    std::uint32_t plane_count() const noexcept;
    std::uint32_t rate_value() const noexcept;
    std::uint32_t bytes_per_plane() const noexcept;
    // One raw encoded payload view only. Per-plane/channel layout and
    // deinterleaving are unresolved; no contiguous plane slices are exposed.
    PayloadBytes encoded_payload() const noexcept;
    // Uses the explicit open host file, restoring its cursor on success/failure.
    // An unsuccessful restoration is reported instead of returning payload.
    static std::optional<SamplePayload> read(content::NativeFile&, const ResourceLookup&, PayloadFailure&);
    // Owns a fresh temporary handle; no caller handle/cursor is involved.
    static std::optional<SamplePayload> read(const std::filesystem::path&, const ResourceLookup&, PayloadFailure&);
private:
    SamplePayload() = default;
    std::vector<std::uint8_t> bytes_;
    std::uint32_t word(std::size_t offset) const noexcept;
};
} // namespace mka::sound
