#pragma once
#include "mka/sample_payload.hpp"
#include "mka/adpcm_codec.hpp"
namespace mka::sound {
// The optimized retail transform leaves range1's first16 bytes untouched.
// This minimal DATA history is explicit; it is not a guest-memory workspace.
struct NormalCount2Head { std::array<std::uint8_t,16> bytes{}; bool known=false; };
enum class NormalSampleError {
    none, unsupported_count, unsupported_window_encoding, unsupported_zero_plane_size,
    misaligned_plane_size, inconsistent_payload, unknown_initial_head,
    host_capacity, allocation_failed, codec_failed
};
struct NormalSampleFailure { NormalSampleError kind=NormalSampleError::none; AdpcmError codec=AdpcmError::none; };
struct NormalCount2Encoded {
    std::array<std::vector<std::uint8_t>,2> planes;
    std::array<std::uint8_t,20> original_header{};
};
struct NormalCount2Decoded {
    NormalCount2Encoded encoded;
    std::array<AdpcmMono,2> pcm;
};
// Proven optimized source-format variant only: header+4=65536, window=word/4.
// 0x4000 is the source permutation's head-distance, never a total resource cap.
// Owns complete planes; no fallback selected by host allocation/memory state.
// Unsupported first short read requires caller-provided known head, not zeros.
// Both APIs are transactional for caller head and independent ADPCM histories.
std::optional<NormalCount2Encoded> convert_normal_count2(
    const SamplePayload&, NormalCount2Head&, NormalSampleFailure&);
// Encoded flags are preserved, not interpreted as loops/stop/audible duration.
// Plane order is ordered source output, not an asserted left/right mapping.
std::optional<NormalCount2Decoded> decode_normal_count2(
    const SamplePayload&, NormalCount2Head&, std::array<AdpcmHistory,2>&, NormalSampleFailure&);
} // namespace mka::sound
