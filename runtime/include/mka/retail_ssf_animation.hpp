#pragma once
#include "mka/retail_ssf_geometry.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mka::retail {

enum class RetailSsfAnimationError {
    none, range, chunk, version, format, duration, track, keyframe,
    interpolation, hierarchy, allocation, native_exception,
    unsupported_keyframe_compression, unsupported_track_type
};

struct RetailSsfAnimationFailure {
    RetailSsfAnimationError kind{RetailSsfAnimationError::none};
    const char* operation{""};
    std::size_t offset{};
};

// A single keyframe for one bone track: time + rotation quaternion + translation.
struct RetailSsfKeyframe {
    float time{};
    std::array<float, 4> rotation{}; // quaternion (x, y, z, w)
    std::array<float, 3> translation{};
};

// One animated bone track: a sequence of keyframes.
struct RetailSsfAnimationTrack {
    std::uint32_t node_id{};
    std::vector<RetailSsfKeyframe> keyframes;
};

// A decoded retail animation clip.
struct RetailSsfAnimationData {
    std::string name;
    std::uint32_t duration_frames{};
    std::uint32_t format_flags{};
    std::uint32_t track_count{};
    std::vector<RetailSsfAnimationTrack> tracks;
    std::size_t serialized_bytes{};
};

// Evaluate all tracks at frame index t. Returns per-track interpolated
// rotation and translation. Fail-closed on out-of-range or non-finite results.
struct RetailSsfPoseSample {
    std::uint32_t node_id{};
    std::array<float, 4> rotation{}; // interpolated quaternion
    std::array<float, 3> translation{};
};

std::optional<std::vector<RetailSsfPoseSample>> evaluate_retail_ssf_animation(
    const RetailSsfAnimationData&, float frame, RetailSsfAnimationFailure&) noexcept;

// Decode one animation resource blob from the SSF art section. The blob
// starts with the Midway custom header (bone/clip name, parameters) followed
// by nested RW STRUCT chunks containing animation metadata and keyframes.
std::optional<RetailSsfAnimationData> decode_retail_ssf_animation(
    std::span<const std::byte>, RetailSsfAnimationFailure&) noexcept;

// Compose evaluated bone matrices into the RpSkin palette format:
// inv_bind_matrices * evaluated_world_bone_matrices per bone.
// The result is a vector of 4x4 column-major matrices (12 floats each in
// RetailRendererAffine layout), one per bone, ready for GPU skinning.
std::optional<std::vector<RetailRendererAffine>> compose_retail_ssf_skin_palette(
    const RetailSsfAnimationData&,
    const std::vector<RetailSsfGeometryFrame>& frames,
    const RetailSsfSkinData& skin,
    float frame,
    RetailSsfAnimationFailure&) noexcept;

} // namespace mka::retail
