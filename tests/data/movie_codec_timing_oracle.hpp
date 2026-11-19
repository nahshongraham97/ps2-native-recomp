#pragma once
#include <cstdint>
namespace native_movie_codec_timing_oracle {
// Independent documented-default FFmpeg harness, same pinned codec DLLs.
// Exact33byte per-frame serialization; no source SDK origin/clock equivalence.
struct Asset { const char* name; std::uint64_t frames,gop_records; const char* sha256; };
inline constexpr Asset assets[] = {
    {"LOGON.SFD",152,12,"97b00b2ece25710c38374f4568b33625f7fdaa720d0150e42f6feffdc3ef1713"},
    {"TITLEN.SFD",900,60,"fbcd6f26dfeeff30d72f6a93dfec3b3aefa1b0d237374e6bccb5ecb8bf3ccf6b"},
    {"OPENINGN.SFD",7841,551,"4de9413a8201af9d70d4bb3f6f6342e8ab88d7d76fea25169963c7d556f6ce77"},
};
}
