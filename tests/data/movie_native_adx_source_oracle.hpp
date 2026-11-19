#pragma once
#include <cstdint>

// Original-byte integer-model certificate: native-adx-composition-20261119/
// validation-02/audio-oracle.json, PASS_ORIGINAL_ASSET_NATIVE_ADX_CORPUS.
// Authenticated full38-row receipt SHA256:
// 0f85cce649803a742ddab928fc1383bea0b82d700cc2d01086cf8e383a058422.
// Expected complete-row digest:
// 39c5b4a13b22b8b86dc10b5c47c028ae29864cc569a2fda6752af70205157ddd.
// These are exact N-count PCM hashes, frame-major/plane-major explicit LE S16.
// They are NOT the historical FFmpeg padded/planar audio goldens. Their scope
// uses the separately measured software coefficient pair, not hardware proof.
namespace native_movie_adx_source_oracle {
struct Asset {
    const char* name;
    std::uint64_t published_frames,raw_decoded_frames;
    const char* pcm_interleaved_s16le_sha256;
};
inline constexpr Asset assets[]={
    {"LOGON.SFD",243201,243232,"3ad71bb48fdc8cd37f258f80ba99b993331814c655a316ea20056dc9bc6965d4"},
    {"TITLEN.SFD",857700,857728,"7726f6f50fe6709a7064470ac178eec4bc0b67846a0fca6262d170cfb2de0969"},
    {"OPENINGN.SFD",12433600,12433600,"3490ceb9850f215606c51d45f7d3cd318f07282207417153cc576b1e16f6ce8e"},
};
} // namespace native_movie_adx_source_oracle
