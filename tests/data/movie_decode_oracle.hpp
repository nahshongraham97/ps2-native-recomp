#pragma once
#include <cstdint>
namespace native_movie_oracle {
struct Times {std::int64_t first,last;std::uint64_t missing;};
struct Stream {std::uint64_t frames,samples,bytes,fnv;const char* sha256;Times pts,best;};
struct Asset {const char* name;Stream video,audio;};
inline constexpr Asset assets[]={
{"LOGON.SFD",{152ull,0ull,65372160ull,0x0468a47cfe425ca5ull,"8bea76eef656ec1df0244b3bae9e12b8f8c1abf9c61b0e7fa938ef6e5ab868a6",{0,453000,0},{0,453000,0}},{7601ull,243232ull,972928ull,0xd26294a0e63f19e3ull,"212139ecc6932a729a8934c66d2e6b62cc2ee8d229e8a0bd0eec3d5ad1e585fc",{0,456480,0},{0,456480,0}}},
{"TITLEN.SFD",{900ull,0ull,387072000ull,0x358f4d588b8c4aaaull,"56d0990779a43b0e5b4f9fc1fef75b747a43297398312b0a17772a7c9f3b3375",{0,2699787,0},{0,2699787,0}},{26804ull,857728ull,3430912ull,0xa974534c9bf69cb6ull,"b38387bf5e66dfebc3ecb64778f9445693c91fb88b0a9a1a8ad315d39d9823e7",{0,1608660,0},{0,1608660,0}}},
{"OPENINGN.SFD",{7841ull,0ull,3372257280ull,0x4968774fb3474c4eull,"1e5efef9f6a41f83b4e74eb054bbc0259717fdba5782602c72d604e4df9e83b8",{0,23543664,36},{0,23543664,0}},{388550ull,12433600ull,49734400ull,0xdff3e672dc7a5c41ull,"fd6ef956681f44f9e4108233c6224b70d7d2dd780717fbe257cc082013b8962b",{0,23313420,0},{0,23313420,0}}},
};
} // namespace native_movie_oracle
