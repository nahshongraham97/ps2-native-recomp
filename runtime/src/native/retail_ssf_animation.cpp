#include "mka/retail_ssf_animation.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {

struct Rejected {};
struct AnimationReader {
    std::span<const std::byte> bytes;
    RetailSsfAnimationFailure& failure;
    [[noreturn]] void reject(RetailSsfAnimationError kind, const char* op, std::size_t at) {
        failure = {kind, op, at};
        throw Rejected{};
    }
    void need(bool value, RetailSsfAnimationError kind, const char* op, std::size_t at) {
        if (!value) reject(kind, op, at);
    }
    void need_size(std::size_t bytes_required, std::size_t at) {
        need(at <= bytes.size() && bytes.size() - at >= bytes_required,
             RetailSsfAnimationError::range, "SSF animation bounded extent", at);
    }
    std::uint32_t word(std::size_t at) {
        need_size(4, at);
        std::uint32_t v{};
        for (unsigned i = 0; i < 4; ++i)
            v |= std::to_integer<std::uint32_t>(bytes[at + i]) << (i * 8);
        return v;
    }
    float scalar(std::size_t at) {
        const auto raw = word(at);
        const auto v = std::bit_cast<float>(raw);
        need(std::isfinite(v), RetailSsfAnimationError::keyframe,
             "SSF animation finite scalar", at);
        return v;
    }
    struct Chunk { std::uint32_t id; std::size_t at, begin, end; };
    // Walk RW chunks, stopping cleanly at trailing alignment padding rather
    // than treating padding bytes as a chunk header. The retail animation
    // resources end with PAD128 alignment; those bytes are not chunks.
    std::vector<Chunk> children(std::size_t begin, std::size_t end) {
        need(begin <= end && end <= bytes.size(),
             RetailSsfAnimationError::range, "SSF animation bounded extent", begin);
        std::vector<Chunk> result;
        while (begin != end) {
            if (end - begin < 12) break; // trailing padding, not a chunk
            // Check if this looks like a valid RW chunk (version stamp at +8).
            if (begin + 12 > bytes.size()) break;
            const auto probe = word(begin + 8);
            if (probe != 0x1803ffffu) break; // not a chunk; alignment padding
            const auto id = word(begin), count = word(begin + 4);
            need(count <= end - begin - 12, RetailSsfAnimationError::chunk,
                 "SSF animation child crosses parent", begin);
            result.push_back({id, begin, begin + 12, begin + 12 + std::size_t(count)});
            begin = begin + 12 + std::size_t(count);
        }
        return result;
    }
};

std::array<float, 4> slerp(const std::array<float, 4>& a, const std::array<float, 4>& b, float t) noexcept {
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    std::array<float, 4> other = b;
    if (dot < 0.0f) {
        for (auto& x : other) x = -x;
        dot = -dot;
    }
    if (dot > 0.9995f) {
        std::array<float, 4> result{};
        for (unsigned i = 0; i < 4; ++i)
            result[i] = a[i] * (1.0f - t) + other[i] * t;
        float norm = std::sqrt(result[0]*result[0] + result[1]*result[1] +
                                result[2]*result[2] + result[3]*result[3]);
        if (norm > 0.0f) for (auto& x : result) x /= norm;
        return result;
    }
    const float theta = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    const float sin_theta = std::sin(theta);
    if (std::abs(sin_theta) < 1e-10f) return a;
    const float wa = std::sin((1.0f - t) * theta) / sin_theta;
    const float wb = std::sin(t * theta) / sin_theta;
    std::array<float, 4> result{};
    for (unsigned i = 0; i < 4; ++i)
        result[i] = a[i] * wa + other[i] * wb;
    return result;
}

float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

RetailRendererAffine identity_affine() noexcept {
    return {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
}

RetailRendererAffine quaternion_to_affine(const std::array<float, 4>& q,
                                           const std::array<float, 3>& t) noexcept {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float x2 = x + x, y2 = y + y, z2 = z + z;
    const float xx = x * x2, xy = x * y2, xz = x * z2;
    const float yy = y * y2, yz = y * z2, zz = z * z2;
    const float wx = w * x2, wy = w * y2, wz = w * z2;
    return {
        {1.0f - (yy + zz), xy + wz,           xz - wy},
        {xy - wz,           1.0f - (xx + zz), yz + wx},
        {xz + wy,           yz - wx,          1.0f - (xx + yy)},
        {t[0], t[1], t[2]}};
}

} // namespace

std::optional<RetailSsfAnimationData> decode_retail_ssf_animation(
    std::span<const std::byte> bytes, RetailSsfAnimationFailure& failure) noexcept {
    failure = {};
    if (bytes.empty()) {
        failure = {RetailSsfAnimationError::range, "SSF animation empty input", 0};
        return {};
    }
#ifdef SSF_ANIM_TRACE
    std::fprintf(stderr, "TRACE: decode input=%zu bytes\n", bytes.size());
#endif
    try {
        AnimationReader r{bytes, failure};
        r.need_size(12, 0);
        const auto name_len = std::to_integer<std::uint8_t>(bytes[4]);
        r.need(name_len > 0 && name_len < 64, RetailSsfAnimationError::format,
               "SSF animation name length", 4);
        r.need_size(5 + name_len + 8, 0);
        RetailSsfAnimationData result;
        result.name = std::string(reinterpret_cast<const char*>(bytes.data() + 5), name_len);
        result.serialized_bytes = bytes.size();
        const auto post_name = 5 + name_len;
        const auto header_p1 = r.word(post_name);
        const auto header_p2 = r.word(post_name + 4);
        (void)header_p2;

        const auto chunks = r.children(post_name + 8, bytes.size());
        r.need(!chunks.empty(), RetailSsfAnimationError::chunk,
               "SSF animation requires at least one chunk", post_name + 8);

        // First chunk: STRUCT with platform tag (id=1, len=8, "PS2"+ver).
        r.need(!chunks.empty() && chunks[0].id == 1, RetailSsfAnimationError::chunk,
               "SSF animation metadata chunk type", post_name + 8);
        (void)header_p1;

        // Find the large keyframe STRUCT chunk containing nested animation data.
        // Guard against size_t underflow: require the chunk to have at least
        // 76 bytes (12 header + 64 data) before treating it as the keyframe blob.
        const AnimationReader::Chunk* kf_chunk = nullptr;
        for (const auto& c : chunks) {
            if (c.id == 1 && c.end > c.begin && c.end - c.begin > 76) { kf_chunk = &c; break; }
        }
        if (!kf_chunk) {
            failure = {RetailSsfAnimationError::unsupported_keyframe_compression,
                       "SSF animation no separate keyframe data chunk found", 0};
            return {};
        }

        const auto inner = r.children(kf_chunk->begin, kf_chunk->end);
        r.need(inner.size() >= 2, RetailSsfAnimationError::chunk,
               "SSF animation keyframe inner chunks", kf_chunk->at);

        // Inner metadata STRUCT: inner[0].begin is already the DATA start.
        r.need(inner[0].id == 1, RetailSsfAnimationError::chunk,
               "SSF animation inner metadata type", inner[0].at);
        const auto m2 = inner[0].begin;
        const auto m2_size = inner[0].end - m2;
        r.need(m2_size >= 28, RetailSsfAnimationError::format,
               "SSF animation inner metadata extent", inner[0].at);
        const auto inner_duration = r.word(m2);
        const auto inner_duration2 = r.word(m2 + 4);
        const auto inner_bpk = r.word(m2 + 8);
        const auto inner_flags = r.word(m2 + 12);
        // Track count: the low byte of the word at m2+20 in observed retail
        // data (upper bits are flags, e.g. 0x20000006 → 6 tracks).
        const auto inner_track_word = r.word(m2 + 20);
        const auto inner_track_count = inner_track_word & 0xffu;
        const auto inner_kf_count = r.word(m2 + 24);
        result.duration_frames = inner_duration;
        result.format_flags = inner_flags;
        result.track_count = inner_track_count;
        (void)inner_duration2; (void)inner_bpk; (void)inner_kf_count;

        // Keyframe data chunk (second inner chunk): begin is already the DATA start.
        const auto* data_chunk = &inner[1];
        const auto dp = data_chunk->begin; // data start (past the inner chunk header)
        const auto dsz = data_chunk->end - dp;
        r.need(dsz >= 16, RetailSsfAnimationError::range,
               "SSF animation keyframe data extent", data_chunk->at);

        // The retail keyframe blob uses the Midway custom compressed format.
        // The observed data does NOT match simple u32_node + u32_count +
        // f32_quat[4] + f32_trans[3] per keyframe. Attempt the simple format
        // on the first track header; if it does not fit, refuse the entire
        // keyframe blob by name. This is a fail-closed named error, not a
        // partial decode or silent skip.
        //
        // Simple format check: the first 8 bytes of the data blob would be
        // a track header (u32 node_id, u32 kf_count). In the observed retail
        // data, these words contain compressed/interleaved bitfields that
        // don't represent valid simple-format headers.
        std::size_t cursor = dp; // dp already points to chunk data (begin+12)
        bool simple_format = true;
        // Probe the first track header.
        if (cursor + 8 <= data_chunk->end) {
            const auto probe_node = r.word(cursor);
            const auto probe_count = r.word(cursor + 4);
            // In the simple format, node_id would be a small bone index
            // (0..255) and kf_count would be a reasonable number (1..256).
            // The retail compressed data produces values that don't fit.
            if (probe_node > 255 || probe_count == 0 || probe_count > 256) {
                simple_format = false;
            } else {
                // Additional check: try to read the first keyframe's time
                // scalar. If it's not a reasonable finite float, the data
                // is not in the simple format.
                const auto kf_probe_off = cursor + 8;
                if (kf_probe_off + 4 <= data_chunk->end) {
                    const auto raw_time = r.word(kf_probe_off);
                    const auto kf_time = std::bit_cast<float>(raw_time);
                    if (!std::isfinite(kf_time) || kf_time < 0.0f || kf_time > 10000.0f) {
                        simple_format = false;
                    }
                }
            }
        } else {
            simple_format = false;
        }

        if (!simple_format) {
            // The retail blob uses a compressed interleaved format.
            // The chunk structure and metadata are valid, but the keyframe
            // compression requires the matching-source evaluator from the
            // pinned binary. Refuse by name.
            failure = {RetailSsfAnimationError::unsupported_keyframe_compression,
                       "SSF animation retail keyframe compression not yet decoded", dp};
            return {};
        }

        // Decode the simple format if it fits.
        result.tracks.reserve(std::min<std::size_t>(inner_track_count, 64));
        for (std::uint32_t ti = 0; ti < inner_track_count && cursor + 8 <= data_chunk->end; ++ti) {
            RetailSsfAnimationTrack track;
            track.node_id = r.word(cursor);
            const auto kf_count = r.word(cursor + 4);
            if (kf_count == 0 || kf_count > 256) break;
            cursor += 8;
            const auto kf_bytes = std::size_t(kf_count) * 32;
            if (cursor + kf_bytes > data_chunk->end) break;
            track.keyframes.reserve(kf_count);
            for (std::uint32_t ki = 0; ki < kf_count; ++ki) {
                RetailSsfKeyframe kf;
                kf.time = r.scalar(cursor);
                kf.rotation[0] = r.scalar(cursor + 4);
                kf.rotation[1] = r.scalar(cursor + 8);
                kf.rotation[2] = r.scalar(cursor + 12);
                kf.rotation[3] = r.scalar(cursor + 16);
                kf.translation[0] = r.scalar(cursor + 20);
                kf.translation[1] = r.scalar(cursor + 24);
                kf.translation[2] = r.scalar(cursor + 28);
                track.keyframes.push_back(kf);
                cursor += 32;
            }
            result.tracks.push_back(std::move(track));
        }
        return result;
    } catch (const Rejected&) {
        return {};
    } catch (const std::bad_alloc&) {
        failure = {RetailSsfAnimationError::allocation, "SSF animation allocation", 0};
    } catch (...) {
        failure = {RetailSsfAnimationError::native_exception, "SSF animation exception", 0};
    }
    return {};
}

std::optional<std::vector<RetailSsfPoseSample>> evaluate_retail_ssf_animation(
    const RetailSsfAnimationData& anim, float frame, RetailSsfAnimationFailure& failure) noexcept {
    failure = {};
    if (frame < 0.0f || !std::isfinite(frame)) {
        failure = {RetailSsfAnimationError::interpolation, "SSF animation frame out of range", 0};
        return {};
    }
    try {
        std::vector<RetailSsfPoseSample> result;
        result.reserve(anim.tracks.size());
        for (const auto& track : anim.tracks) {
            if (track.keyframes.empty()) continue;
            RetailSsfPoseSample sample;
            sample.node_id = track.node_id;
            if (track.keyframes.size() == 1) {
                sample.rotation = track.keyframes[0].rotation;
                sample.translation = track.keyframes[0].translation;
            } else {
                std::size_t hi = 0;
                while (hi < track.keyframes.size() && track.keyframes[hi].time < frame) ++hi;
                if (hi == 0) {
                    sample.rotation = track.keyframes[0].rotation;
                    sample.translation = track.keyframes[0].translation;
                } else if (hi >= track.keyframes.size()) {
                    sample.rotation = track.keyframes.back().rotation;
                    sample.translation = track.keyframes.back().translation;
                } else {
                    const auto& lo_kf = track.keyframes[hi - 1];
                    const auto& hi_kf = track.keyframes[hi];
                    const auto span = hi_kf.time - lo_kf.time;
                    if (span <= 0.0f || !std::isfinite(span)) {
                        failure = {RetailSsfAnimationError::interpolation,
                                  "SSF animation non-positive keyframe time span", 0};
                        return {};
                    }
                    const float t = (frame - lo_kf.time) / span;
                    sample.rotation = slerp(lo_kf.rotation, hi_kf.rotation, t);
                    sample.translation[0] = lerp(lo_kf.translation[0], hi_kf.translation[0], t);
                    sample.translation[1] = lerp(lo_kf.translation[1], hi_kf.translation[1], t);
                    sample.translation[2] = lerp(lo_kf.translation[2], hi_kf.translation[2], t);
                }
            }
            for (auto x : sample.rotation) {
                if (!std::isfinite(x)) {
                    failure = {RetailSsfAnimationError::interpolation,
                              "SSF animation non-finite interpolated rotation", 0};
                    return {};
                }
            }
            result.push_back(std::move(sample));
        }
        return result;
    } catch (const std::bad_alloc&) {
        failure = {RetailSsfAnimationError::allocation, "SSF animation evaluation allocation", 0};
    } catch (...) {
        failure = {RetailSsfAnimationError::native_exception, "SSF animation evaluation exception", 0};
    }
    return {};
}

std::optional<std::vector<RetailRendererAffine>> compose_retail_ssf_skin_palette(
    const RetailSsfAnimationData& anim,
    const std::vector<RetailSsfGeometryFrame>& frames,
    const RetailSsfSkinData& skin,
    float frame,
    RetailSsfAnimationFailure& failure) noexcept {
    failure = {};
    if (skin.inv_bind_matrices.empty()) {
        failure = {RetailSsfAnimationError::hierarchy,
                   "SSF animation skin has no inverse bind matrices", 0};
        return {};
    }
    auto samples = evaluate_retail_ssf_animation(anim, frame, failure);
    if (!samples) return {};
    try {
        const auto bone_count = skin.inv_bind_matrices.size();
        std::vector<RetailRendererAffine> palette(bone_count);
        for (std::size_t bi = 0; bi < bone_count && bi < frames.size(); ++bi) {
            palette[bi] = frames[bi].modelling;
        }
        for (std::size_t bi = frames.size(); bi < bone_count; ++bi) {
            palette[bi] = identity_affine();
        }
        for (const auto& sample : *samples) {
            if (sample.node_id < bone_count) {
                palette[sample.node_id] =
                    quaternion_to_affine(sample.rotation, sample.translation);
            }
        }
        return palette;
    } catch (const std::bad_alloc&) {
        failure = {RetailSsfAnimationError::allocation, "SSF animation palette allocation", 0};
    } catch (...) {
        failure = {RetailSsfAnimationError::native_exception, "SSF animation palette exception", 0};
    }
    return {};
}

} // namespace mka::retail
